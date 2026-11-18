// Weather satellite viewer for AIPI-Lite.
// GPIO10 (PWR_CTL) must be HIGH before anything else or the board dies on battery.
// Panel: SCK 16, MOSI 17, CS 15, DC 7, RST 18, BL 3. Constructor matches ST7735 bring-up.
// If a solid red fill is offset, swap to Arduino_ST7789. If R/B are swapped, flip PNG_ENDIAN.

#include <Arduino.h>
#include <strings.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Arduino_GFX_Library.h>
#include <PNGdec.h>
#include "../pins.h"

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_CLK, PIN_LCD_MOSI);
Arduino_GFX *lcd = new Arduino_ST7735(bus, PIN_LCD_RST, 3, false, 128, 128, 0, 0);

struct Sat {
  const char *id;
  const char *sector;
  const char *product;
};

static const Sat kRing[] = {
  {"goes-19",       "full_disk", "geocolor"},
  {"goes-18",       "full_disk", "geocolor"},
  {"himawari",      "full_disk", "geocolor"},
  {"gk2a",          "full_disk", "geocolor"},
  {"meteosat-9",    "full_disk", "geocolor"},
  {"meteosat-0deg", "full_disk", "geocolor"},
  {"meteosat-12",   "full_disk", "geocolor"},
  {"jpss",          "conus",     "abi_cira_geocolor"},
};
static const int kCount = 8;
static int satIndex = 0;  // GOES-19

static const char *kHost = "slider.cira.colostate.edu";
static const size_t kMaxPng = 900 * 1024;
static const unsigned long kRefreshMs = 10UL * 60UL * 1000UL;
static const unsigned long kLeftHoldMs = 500;
// Five decoded neighbor frames; plus the display and staging frames (~224 KB total).
static const int kPrefetchSlots = 5;
static const size_t kFrameBytes = 128 * 128 * sizeof(uint16_t);

// PNGdec RGB565_BIG_ENDIAN + draw16bitBeRGBBitmap. Switch to LITTLE_ENDIAN +
// draw16bitRGBBitmap if red/blue are swapped on this panel.
#define PNG_ENDIAN PNG_RGB565_BIG_ENDIAN

static uint16_t *frame = nullptr;  // 128×128 RGB565 in PSRAM
static uint16_t *stagingFrame = nullptr;  // Decode here so the displayed frame stays intact until success.
static uint16_t *decodeFrame = nullptr;
struct CachedFrame {
  uint16_t *pixels;
  int index;
  unsigned long fetchedMs;
  char stamp[15];
};
static CachedFrame frameCache[kPrefetchSlots] = {};
static int activePrefetchSlot = -1;
static PNG png;
static bool fetching = false;
static bool haveFrame = false;
static int loadedIndex = -1;
static char loadedStamp[15] = "";
static unsigned long loadedFetchedMs = 0;
static int failedIndex = -1;
static uint32_t resizeUs = 0;
static unsigned long lastFetchMs = 0;
static unsigned long lastAttemptMs = 0;
static unsigned long lastStatusMs = 0;
static int loadingStatusIndex = -1;
static uint8_t loadingDots = 0;
static int prefetchFailedIndex = -1;
static unsigned long prefetchFailedMs = 0;

Preferences prefs;
WebServer server(80);
DNSServer dnsServer;
String wifi_ssid = "";
String wifi_pass = "";
uint32_t cached_ip = 0, cached_gw = 0, cached_sn = 0, cached_dns = 0;
bool has_ip_cache = false;

void timeListUrl(const Sat &s, char *out, size_t n) {
  snprintf(out, n,
    "https://slider.cira.colostate.edu/data/json/%s/%s/%s/latest_times.json",
    s.id, s.sector, s.product);
}

void tileUrl(const Sat &s, const char *stamp, char *out, size_t n) {
  char y[5], mo[3], d[3];
  memcpy(y,  stamp + 0, 4); y[4] = 0;
  memcpy(mo, stamp + 4, 2); mo[2] = 0;
  memcpy(d,  stamp + 6, 2); d[2] = 0;
  snprintf(out, n,
    "https://slider.cira.colostate.edu/data/imagery/%s/%s/%s/%s---%s/%s/%s/00/000_000.png",
    y, mo, d, s.id, s.sector, s.product, stamp);
}

static const char *urlPath(const char *url) {
  const char *p = strstr(url, "://");
  if (!p) return url;
  p = strchr(p + 3, '/');
  return p ? p : "/";
}

static int weekdaySunday0(int year, int month, int day) {
  static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) year--;
  return (year + year / 4 - year / 100 + year / 400 + offsets[month - 1] + day) % 7;
}

static void formatPacificTime(const char *stamp, char *out, size_t outSize) {
  int year = (stamp[0] - '0') * 1000 + (stamp[1] - '0') * 100 +
             (stamp[2] - '0') * 10 + stamp[3] - '0';
  int month = (stamp[4] - '0') * 10 + stamp[5] - '0';
  int day = (stamp[6] - '0') * 10 + stamp[7] - '0';
  int hour = (stamp[8] - '0') * 10 + stamp[9] - '0';
  int minute = (stamp[10] - '0') * 10 + stamp[11] - '0';
  bool daylight = false;

  if (month > 3 && month < 11) {
    daylight = true;
  } else if (month == 3) {
    int firstSunday = 1 + ((7 - weekdaySunday0(year, 3, 1)) % 7);
    int secondSunday = firstSunday + 7;
    daylight = day > secondSunday || (day == secondSunday && hour >= 10); // 02:00 PST
  } else if (month == 11) {
    int firstSunday = 1 + ((7 - weekdaySunday0(year, 11, 1)) % 7);
    daylight = day < firstSunday || (day == firstSunday && hour < 9); // 02:00 PDT
  }

  int localMinutes = hour * 60 + minute - (daylight ? 7 * 60 : 8 * 60);
  if (localMinutes < 0) localMinutes += 24 * 60;
  snprintf(out, outSize, "%02d:%02d", localMinutes / 60, localMinutes % 60);
}

static void drawSatHeader() {
  if (loadedIndex < 0 || !loadedStamp[0]) return;
  lcd->setTextColor(RGB565_WHITE);
  lcd->setTextSize(1);
  const char *name = kRing[loadedIndex].id;
  int nameWidth = (int)strlen(name) * 6 + 4;
  lcd->fillRect(0, 0, nameWidth, 10, RGB565_BLACK);
  lcd->setCursor(2, 1);
  lcd->print(name);

  char timeText[6];
  formatPacificTime(loadedStamp, timeText, sizeof(timeText));
  int timeBoxWidth = (int)strlen(timeText) * 6 + 4;
  int timeBoxX = 128 - timeBoxWidth;
  lcd->fillRect(timeBoxX, 0, timeBoxWidth, 10, RGB565_BLACK);
  int timeX = timeBoxX + 2;
  lcd->setCursor(timeX, 1);
  lcd->print(timeText);
}

static void drawCacheDots();

void pushFrame() {
  lcd->draw16bitBeRGBBitmap(0, 0, frame, 128, 128);
  drawSatHeader();
  drawCacheDots();
}

static void drawLoadingIndicator() {
  if (loadingStatusIndex < 0) return;
  if (haveFrame) {
    lcd->fillRect(34, 118, 60, 10, RGB565_BLACK);
  } else {
    lcd->fillScreen(RGB565_BLACK);
  }
  lcd->setTextColor(RGB565_WHITE);
  lcd->setTextSize(1);
  lcd->setCursor(36, 119);
  lcd->print("Loading");
  for (int i = 0; i < loadingDots; i++) lcd->print('.');
  drawCacheDots();
}

static void showLoadingFor(int index) {
  loadingStatusIndex = index;
  loadingDots = 0;
  lastStatusMs = millis();
  drawLoadingIndicator();
}

static int findCachedFrame(int index) {
  unsigned long now = millis();
  for (int i = 0; i < kPrefetchSlots; i++) {
    if (i != activePrefetchSlot && frameCache[i].index == index &&
        now - frameCache[i].fetchedMs < kRefreshMs) return i;
  }
  return -1;
}

static int satelliteDistance(int a, int b) {
  int forward = (a - b + kCount) % kCount;
  int backward = (b - a + kCount) % kCount;
  return forward < backward ? forward : backward;
}

static bool displayCachedFrame(int index) {
  int slot = findCachedFrame(index);
  if (slot < 0) return false;
  memcpy(frame, frameCache[slot].pixels, kFrameBytes);
  loadedIndex = index;
  memcpy(loadedStamp, frameCache[slot].stamp, sizeof(loadedStamp));
  loadedFetchedMs = frameCache[slot].fetchedMs;
  loadingStatusIndex = -1;
  failedIndex = -1;
  haveFrame = true;
  lastFetchMs = loadedFetchedMs;
  pushFrame();
  Serial.printf("cache hit %s\n", kRing[index].id);
  return true;
}

static void retainDisplayedFrame(int nextIndex) {
  if (!haveFrame || loadedIndex < 0 || !loadedStamp[0] || findCachedFrame(loadedIndex) >= 0) return;

  int slot = -1;
  for (int i = 0; i < kPrefetchSlots; i++) {
    if (i != activePrefetchSlot &&
        (frameCache[i].index < 0 || millis() - frameCache[i].fetchedMs >= kRefreshMs)) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    // Keep the destination satellite's cached frame; evict the cache entry
    // farthest from the satellite we are moving to.
    int farthestDistance = -1;
    for (int i = 0; i < kPrefetchSlots; i++) {
      if (i == activePrefetchSlot || frameCache[i].index == nextIndex) continue;
      int distance = satelliteDistance(frameCache[i].index, nextIndex);
      if (distance > farthestDistance) {
        farthestDistance = distance;
        slot = i;
      }
    }
  }
  if (slot < 0) return;

  memcpy(frameCache[slot].pixels, frame, kFrameBytes);
  frameCache[slot].index = loadedIndex;
  frameCache[slot].fetchedMs = loadedFetchedMs;
  memcpy(frameCache[slot].stamp, loadedStamp, sizeof(frameCache[slot].stamp));
  Serial.printf("cache keep %s\n", kRing[loadedIndex].id);
}

static void drawCacheDots() {
  int focus = (loadingStatusIndex >= 0) ? loadingStatusIndex : loadedIndex;
  if (focus < 0) return;
  lcd->fillCircle(3, 124, 3, RGB565_BLACK);
  lcd->fillCircle(124, 124, 3, RGB565_BLACK);
  uint16_t color = RGB565_WHITE;
  if (findCachedFrame((focus + kCount - 1) % kCount) >= 0) {
    lcd->fillCircle(3, 124, 2, color);
  }
  if (findCachedFrame((focus + 1) % kCount) >= 0) {
    lcd->fillCircle(124, 124, 2, color);
  }
}

void showMessage(const char *msg) {
  lcd->fillScreen(RGB565_BLACK);
  lcd->setTextColor(RGB565_WHITE);
  lcd->setTextSize(1);
  lcd->setCursor(4, 10);
  lcd->print(msg);
}

static int leftStable = HIGH;
static int rightStable = HIGH;
static unsigned long leftEdgeMs = 0;
static unsigned long rightEdgeMs = 0;
static unsigned long leftDownMs = 0;
static bool leftHoldIgnored = false;

static void serviceButtons() {
  unsigned long now = millis();

  int left = digitalRead(PIN_BTN_A);
  if (left != leftStable) {
    if (!leftEdgeMs) leftEdgeMs = now;
    else if (now - leftEdgeMs >= 50) {
      leftStable = left;
      leftEdgeMs = 0;
      if (leftStable == LOW) {
        leftDownMs = now;
        leftHoldIgnored = false;
      } else {
        if (!leftHoldIgnored && now - leftDownMs < kLeftHoldMs) {
          int nextIndex = (satIndex + kCount - 1) % kCount;
          retainDisplayedFrame(nextIndex);
          satIndex = nextIndex;
          Serial.printf("sat <- %s\n", kRing[satIndex].id);
          if (!displayCachedFrame(satIndex)) {
            showLoadingFor(satIndex);
          }
        }
      }
    }
  } else {
    leftEdgeMs = 0;
    if (leftStable == LOW && !leftHoldIgnored && now - leftDownMs >= kLeftHoldMs) {
      leftHoldIgnored = true;  // GPIO1 is also power; a long press turns the board off
    }
  }

  int right = digitalRead(PIN_BTN_B);
  if (right != rightStable) {
    if (!rightEdgeMs) rightEdgeMs = now;
    else if (now - rightEdgeMs >= 50) {
      rightStable = right;
      rightEdgeMs = 0;
      if (rightStable == LOW) {
        int nextIndex = (satIndex + 1) % kCount;
        retainDisplayedFrame(nextIndex);
        satIndex = nextIndex;
        Serial.printf("sat -> %s\n", kRing[satIndex].id);
        if (!displayCachedFrame(satIndex)) {
          showLoadingFor(satIndex);
        }
      }
    }
  } else {
    rightEdgeMs = 0;
  }

  if (fetching && loadingStatusIndex >= 0 && now - lastStatusMs >= 300) {
    loadingDots = (loadingDots + 1) % 4;
    lastStatusMs = now;
    drawLoadingIndicator();
  }
}

static bool readHeaders(WiFiClientSecure &client, int &status, size_t &contentLength) {
  status = 0;
  contentLength = 0;
  char line[128];
  size_t len = 0;
  bool first = true;
  unsigned long t0 = millis();

  while (millis() - t0 < 20000) {
    serviceButtons();
    if (!client.available()) {
      if (!client.connected() && !client.available()) return false;
      delay(1);
      continue;
    }
    int c = client.read();
    if (c < 0) continue;
    if (c == '\n') {
      while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ')) len--;
      line[len] = 0;
      if (len == 0) return status == 200;
      if (first) {
        const char *sp = strchr(line, ' ');
        if (sp) status = atoi(sp + 1);
        first = false;
      } else if (strncasecmp(line, "Content-Length:", 15) == 0) {
        const char *v = line + 15;
        while (*v == ' ') v++;
        contentLength = (size_t)strtoul(v, nullptr, 10);
      }
      len = 0;
    } else if (len + 1 < sizeof(line)) {
      line[len++] = (char)c;
    }
  }
  return false;
}

static bool httpsOpen(WiFiClientSecure &client, const char *url) {
  client.setInsecure();
  client.setTimeout(20);
  if (!client.connect(kHost, 443, 15000)) {
    Serial.println("TLS connect fail");
    return false;
  }
  client.printf(
    "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
    urlPath(url), kHost);
  return true;
}

static bool fetchStamp(const Sat &s, char *stamp, uint32_t *fetchMs, uint32_t *parseUs) {
  uint32_t fetchStart = millis();
  char url[256];
  timeListUrl(s, url, sizeof(url));

  WiFiClientSecure client;
  if (!httpsOpen(client, url)) return false;

  int status = 0;
  size_t clen = 0;
  if (!readHeaders(client, status, clen) || status != 200) {
    Serial.printf("times HTTP %d\n", status);
    client.stop();
    return false;
  }
  if (clen == 0 || clen > 32768) {
    Serial.printf("json length %u refused\n", (unsigned)clen);
    client.stop();
    return false;
  }

  char *body = (char *)ps_malloc(clen + 1);
  if (!body) {
    Serial.println("json ps_malloc fail");
    client.stop();
    return false;
  }

  size_t got = 0;
  unsigned long bodyStart = millis();
  while (got < clen && millis() - bodyStart < 10000) {
    serviceButtons();
    int n = client.read((uint8_t *)body + got, clen - got);
    if (n > 0) {
      got += (size_t)n;
      bodyStart = millis();
    } else if (!client.connected() && !client.available()) {
      break;
    } else {
      delay(1);
    }
  }
  client.stop();
  *fetchMs = millis() - fetchStart;
  if (got != clen) {
    Serial.printf("json short %u/%u\n", (unsigned)got, (unsigned)clen);
    free(body);
    return false;
  }
  body[got] = 0;

  uint32_t parseStart = micros();
  int digits = 0;
  bool found = false;
  for (size_t i = 0; i < got && !found; i++) {
    int c = (uint8_t)body[i];
    if (c >= '0' && c <= '9') {
      if (digits < 14) stamp[digits] = (char)c;
      digits++;
    } else {
      if (digits == 14) {
        stamp[14] = 0;
        found = true;
        break;
      }
      digits = 0;
    }
  }
  if (!found && digits == 14) {
    stamp[14] = 0;
    found = true;
  }
  *parseUs = micros() - parseStart;
  free(body);
  if (found) Serial.printf("stamp %s\n", stamp);
  return found;
}

static bool fetchPng(const Sat &s, const char *stamp, uint8_t **outBuf, int *outLen, uint32_t *fetchMs) {
  uint32_t fetchStart = millis();
  char url[320];
  tileUrl(s, stamp, url, sizeof(url));

  WiFiClientSecure client;
  if (!httpsOpen(client, url)) return false;

  int status = 0;
  size_t clen = 0;
  if (!readHeaders(client, status, clen) || status != 200) {
    Serial.printf("png HTTP %d\n", status);
    client.stop();
    return false;
  }
  if (clen == 0 || clen > kMaxPng) {
    Serial.printf("png length %u refused\n", (unsigned)clen);
    client.stop();
    return false;
  }

  uint8_t *buf = (uint8_t *)ps_malloc(clen);
  if (!buf) {
    Serial.println("ps_malloc fail");
    client.stop();
    return false;
  }

  size_t got = 0;
  unsigned long t0 = millis();
  while (got < clen && millis() - t0 < 60000) {
    serviceButtons();
    int n = client.read(buf + got, clen - got);
    if (n > 0) {
      got += (size_t)n;
      t0 = millis();
    } else if (!client.connected() && !client.available()) {
      break;
    } else {
      delay(1);
    }
  }
  client.stop();

  if (got != clen) {
    Serial.printf("png short %u/%u\n", (unsigned)got, (unsigned)clen);
    free(buf);
    return false;
  }
  *outBuf = buf;
  *outLen = (int)clen;
  *fetchMs = millis() - fetchStart;
  Serial.printf("png %u bytes\n", (unsigned)clen);
  return true;
}

int onLine(PNGDRAW *p) {
  uint32_t resizeStart = micros();
  int dstY = (int)((int32_t)p->y * 128 / png.getHeight());
  int prev = (int)((int32_t)(p->y - 1) * 128 / png.getHeight());
  if (p->y > 0 && dstY == prev) {
    resizeUs += micros() - resizeStart;
    return 1;
  }

  uint16_t line[700];
  png.getLineAsRGB565(p, line, PNG_ENDIAN, 0x00000000);
  int srcW = png.getWidth();
  if (srcW > 700) srcW = 700;
  for (int x = 0; x < 128; x++) {
    int sx = (int)((int32_t)x * srcW / 128);
    decodeFrame[dstY * 128 + x] = line[sx];
  }
  resizeUs += micros() - resizeStart;
  return 1;
}

static bool decodeToFrame(uint8_t *pngBuf, int pngLen, uint16_t *target,
                          uint32_t *decodeMs, uint32_t *resizeTimeUs) {
  uint32_t decodeStart = millis();
  resizeUs = 0;
  decodeFrame = target;
  int openRc = png.openRAM(pngBuf, pngLen, onLine);
  if (openRc != PNG_SUCCESS) {
    Serial.printf("png open rc=%d err=%d\n", openRc, png.getLastError());
    *decodeMs = millis() - decodeStart;
    *resizeTimeUs = resizeUs;
    return false;
  }
  int w = png.getWidth();
  int h = png.getHeight();
  if (w <= 0 || w > 700 || h <= 0) {
    Serial.printf("png size refused %dx%d\n", w, h);
    png.close();
    *decodeMs = millis() - decodeStart;
    *resizeTimeUs = resizeUs;
    return false;
  }
  int rc = png.decode(NULL, 0);
  if (rc != PNG_SUCCESS) {
    Serial.printf("png decode rc=%d err=%d\n", rc, png.getLastError());
  }
  png.close();
  *decodeMs = millis() - decodeStart;
  *resizeTimeUs = resizeUs;
  return rc == PNG_SUCCESS;
}

static bool loadSatellite(int i, uint16_t *target, bool displayWhenReady, char *stampOut) {
  const Sat &s = kRing[i];
  uint32_t totalStart = millis();
  uint32_t jsonFetchMs = 0, jsonParseUs = 0, imageFetchMs = 0;
  uint32_t decodeMs = 0, resizeTimeUs = 0;
  Serial.printf("load %s\n", s.id);

  char stamp[15];
  if (!fetchStamp(s, stamp, &jsonFetchMs, &jsonParseUs)) return false;
  if (stampOut) memcpy(stampOut, stamp, sizeof(stamp));

  uint8_t *pngBuf = nullptr;
  int pngLen = 0;
  if (!fetchPng(s, stamp, &pngBuf, &pngLen, &imageFetchMs)) return false;

  bool ok = decodeToFrame(pngBuf, pngLen, target, &decodeMs, &resizeTimeUs);
  free(pngBuf);
  Serial.printf("time %s json=%lu ms parse=%lu us img=%lu ms decode=%lu ms resize=%lu us total=%lu ms\n",
    s.id, (unsigned long)jsonFetchMs, (unsigned long)jsonParseUs,
    (unsigned long)imageFetchMs, (unsigned long)decodeMs,
    (unsigned long)resizeTimeUs, (unsigned long)(millis() - totalStart));
  if (!ok) {
    Serial.println("png decode fail");
    return false;
  }
  if (displayWhenReady) {
    pushFrame();
    haveFrame = true;
    loadingStatusIndex = -1;
  }
  return true;
}

static void loadUntilCaughtUp() {
  fetching = true;
  int started;
  do {
    started = satIndex;
    int cached = findCachedFrame(started);
    if (cached < 0) showLoadingFor(started);
    bool ok;
    char stamp[15] = "";
    unsigned long frameFetchedAt = 0;
    if (cached >= 0) {
      ok = displayCachedFrame(started);
      if (ok) {
        memcpy(stamp, loadedStamp, sizeof(stamp));
        frameFetchedAt = loadedFetchedMs;
      }
    } else {
      ok = loadSatellite(started, stagingFrame, false, stamp);
      if (ok) frameFetchedAt = millis();
    }
    if (ok && satIndex == started) {
      if (cached < 0) memcpy(frame, stagingFrame, kFrameBytes);
      loadedIndex = started;
      memcpy(loadedStamp, stamp, sizeof(loadedStamp));
      loadedFetchedMs = frameFetchedAt;
      loadingStatusIndex = -1;
      pushFrame();
      haveFrame = true;
      lastFetchMs = loadedFetchedMs;
      failedIndex = -1;
    } else if (!ok) {
      failedIndex = started;
      if (!haveFrame) showMessage("Fetch failed");
    }
    // If a press arrived mid-download, satIndex moved; fetch that one next.
  } while (satIndex != started);
  fetching = false;
}

static void prefetchOneNeighbor() {
  if (loadedIndex < 0 || fetching) return;
  for (int i = 0; i < kPrefetchSlots; i++) {
    if (!frameCache[i].pixels) return;
  }

  // Fill nearest neighbors first, alternating forward/backward, then widen out.
  int candidates[kCount - 1];
  int candidateCount = 0;
  for (int distance = 1; distance <= kCount / 2; distance++) {
    int next = (loadedIndex + distance) % kCount;
    int prev = (loadedIndex + kCount - distance) % kCount;
    candidates[candidateCount++] = next;
    if (prev != next && prev != loadedIndex) candidates[candidateCount++] = prev;
  }

  int target = -1;
  for (int i = 0; i < candidateCount; i++) {
    if (findCachedFrame(candidates[i]) < 0 &&
        !(prefetchFailedIndex == candidates[i] && millis() - prefetchFailedMs < 60000)) {
      target = candidates[i];
      break;
    }
  }
  if (target < 0) return;

  int slot = -1;
  for (int i = 0; i < kPrefetchSlots; i++) {
    if (frameCache[i].index < 0 || millis() - frameCache[i].fetchedMs >= kRefreshMs) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    int farthestDistance = -1;
    for (int i = 0; i < kPrefetchSlots; i++) {
      if (frameCache[i].index == loadedIndex || frameCache[i].index == target) continue;
      int distance = satelliteDistance(frameCache[i].index, loadedIndex);
      if (distance > farthestDistance) {
        farthestDistance = distance;
        slot = i;
      }
    }
  }
  if (slot < 0) return;
  frameCache[slot].index = -1;
  activePrefetchSlot = slot;
  fetching = true;
  Serial.printf("prefetch %s\n", kRing[target].id);
  char stamp[15] = "";
  bool ok = loadSatellite(target, frameCache[slot].pixels, false, stamp);
  activePrefetchSlot = -1;
  if (ok) {
    frameCache[slot].index = target;
    frameCache[slot].fetchedMs = millis();
    memcpy(frameCache[slot].stamp, stamp, sizeof(frameCache[slot].stamp));
    drawCacheDots();
    prefetchFailedIndex = -1;
    Serial.printf("cached %s\n", kRing[target].id);
  } else {
    frameCache[slot].index = -1;
    prefetchFailedIndex = target;
    prefetchFailedMs = millis();
  }
  fetching = false;
}

void loadConfiguration() {
  prefs.begin("wxsat", true);
  wifi_ssid = prefs.getString("ssid", "");
  wifi_pass = prefs.getString("pass", "");
  has_ip_cache = prefs.getBool("has_cache", false);
  cached_ip = prefs.getUInt("ip", 0);
  cached_gw = prefs.getUInt("gw", 0);
  cached_sn = prefs.getUInt("sn", 0);
  cached_dns = prefs.getUInt("dns", 0);
  prefs.end();
}

bool connectWifi() {
  if (wifi_ssid.isEmpty()) return false;
  if (!haveFrame) showMessage("Connecting...");
  WiFi.mode(WIFI_STA);
  if (has_ip_cache) {
    WiFi.config(IPAddress(cached_ip), IPAddress(cached_gw), IPAddress(cached_sn), IPAddress(cached_dns));
  }
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
    serviceButtons();
    delay(50);
  }
  if (WiFi.status() != WL_CONNECTED) {
    if (!haveFrame) {
      showMessage("Wi-Fi timeout");
      delay(1500);
    }
    return false;
  }
  if (!has_ip_cache) {
    prefs.begin("wxsat", false);
    prefs.putBool("has_cache", true);
    prefs.putUInt("ip", uint32_t(WiFi.localIP()));
    prefs.putUInt("gw", uint32_t(WiFi.gatewayIP()));
    prefs.putUInt("sn", uint32_t(WiFi.subnetMask()));
    prefs.putUInt("dns", uint32_t(WiFi.dnsIP()));
    prefs.end();
    has_ip_cache = true;
  }
  return true;
}

void handlePortalRoot() {
  int n = WiFi.scanNetworks();
  String options;
  String seen[30];
  int unique = 0;
  for (int i = 0; i < n && unique < 30; i++) {
    String ssid = WiFi.SSID(i);
    bool dup = false;
    for (int j = 0; j < unique; j++) if (seen[j] == ssid) { dup = true; break; }
    if (dup || ssid.isEmpty()) continue;
    seen[unique++] = ssid;
    String sel = (ssid == wifi_ssid) ? " selected" : "";
    options += "<option value=\"" + ssid + "\"" + sel + ">" + ssid + "</option>";
  }
  WiFi.scanDelete();

  String html = F(
    "<html><head><meta name=viewport content='width=device-width'>"
    "<title>Sat-Config</title></head><body>"
    "<h3>Weather sats Wi-Fi</h3>"
    "<form method=POST action=/save>"
    "<p>SSID <select name=ssid><option value=''>other</option>");
  html += options;
  html += F("</select></p>"
    "<p>Other <input name=ssid_custom></p>"
    "<p>Password <input type=password name=pass></p>"
    "<p><button>Save</button></p></form></body></html>");
  server.send(200, "text/html", html);
}

void handlePortalSave() {
  String ssid = server.arg("ssid");
  if (ssid.isEmpty()) ssid = server.arg("ssid_custom");
  wifi_ssid = ssid;
  wifi_pass = server.arg("pass");
  prefs.begin("wxsat", false);
  prefs.putString("ssid", wifi_ssid);
  prefs.putString("pass", wifi_pass);
  prefs.putBool("has_cache", false);
  prefs.end();
  server.send(200, "text/html", "saved, rebooting");
  delay(500);
  ESP.restart();
}

void startCaptivePortal() {
  showMessage("CONFIG AP\nSSID: Sat-Config\n192.168.4.1");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Sat-Config");
  delay(100);
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
  server.on("/", HTTP_GET, handlePortalRoot);
  server.on("/save", HTTP_POST, handlePortalSave);
  server.on("/generate_204", handlePortalRoot);
  server.onNotFound(handlePortalRoot);
  server.begin();
  while (true) {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(10);
  }
}

void setup() {
  pinMode(PIN_PWR_CTL, OUTPUT);
  digitalWrite(PIN_PWR_CTL, HIGH);

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);

  Serial.begin(115200);

  lcd->begin();
  lcd->fillScreen(RGB565_RED);  // confirm panel mapping before any satellite code
  delay(400);
  lcd->fillScreen(RGB565_BLACK);
  lcd->setTextColor(RGB565_WHITE);
  lcd->setTextSize(1);

  frame = (uint16_t *)ps_malloc(128 * 128 * sizeof(uint16_t));
  stagingFrame = (uint16_t *)ps_malloc(kFrameBytes);
  if (!frame || !stagingFrame) {
    showMessage("PSRAM fail");
    while (true) delay(1000);
  }
  for (int i = 0; i < kPrefetchSlots; i++) {
    frameCache[i].pixels = (uint16_t *)ps_malloc(kFrameBytes);
    frameCache[i].index = -1;
    frameCache[i].fetchedMs = 0;
    if (!frameCache[i].pixels) {
      showMessage("PSRAM cache fail");
      while (true) delay(1000);
    }
  }

  loadConfiguration();
  delay(50);

  if (digitalRead(PIN_BTN_B) == LOW) {
    delay(200);
    if (digitalRead(PIN_BTN_B) == LOW) startCaptivePortal();
  }

  if (wifi_ssid.isEmpty()) {
    showMessage("Hold B on boot\nfor Wi-Fi config");
    delay(2500);
    startCaptivePortal();
  }

  if (connectWifi()) {
    loadUntilCaughtUp();
  }
}

void loop() {
  serviceButtons();

  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastWifiTry = 0;
    if (millis() - lastWifiTry > 3000) {
      lastWifiTry = millis();
      connectWifi();
    }
    delay(10);
    return;
  }

  unsigned long now = millis();
  bool need = false;
  if (satIndex != loadedIndex) {
    need = !(satIndex == failedIndex && now - lastAttemptMs < 5000);
  } else if (!haveFrame) {
    need = now - lastAttemptMs > 5000;
  } else if (now - lastFetchMs >= kRefreshMs) {
    need = now - lastAttemptMs > 5000;
  }
  if (need && !fetching) {
    lastAttemptMs = now;
    loadUntilCaughtUp();
  } else if (!need && loadedIndex == satIndex && haveFrame) {
    prefetchOneNeighbor();
  }
  delay(10);
}
