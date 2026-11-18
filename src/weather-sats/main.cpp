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

// PNGdec RGB565_BIG_ENDIAN + draw16bitBeRGBBitmap. Switch to LITTLE_ENDIAN +
// draw16bitRGBBitmap if red/blue are swapped on this panel.
#define PNG_ENDIAN PNG_RGB565_BIG_ENDIAN

static uint16_t *frame = nullptr;  // 128×128 RGB565 in PSRAM
static PNG png;
static bool fetching = false;
static bool haveFrame = false;
static int loadedIndex = -1;
static int failedIndex = -1;
static unsigned long lastFetchMs = 0;
static unsigned long lastAttemptMs = 0;

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

void pushFrame() {
  lcd->draw16bitBeRGBBitmap(0, 0, frame, 128, 128);
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
          satIndex = (satIndex + 7) % kCount;
          Serial.printf("sat <- %s\n", kRing[satIndex].id);
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
        satIndex = (satIndex + 1) % kCount;
        Serial.printf("sat -> %s\n", kRing[satIndex].id);
      }
    }
  } else {
    rightEdgeMs = 0;
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

static bool fetchStamp(const Sat &s, char *stamp) {
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

  int digits = 0;
  bool found = false;
  unsigned long t0 = millis();
  while (!found && millis() - t0 < 10000) {
    serviceButtons();
    int avail = client.available();
    if (avail <= 0) {
      if (!client.connected()) break;
      delay(1);
      continue;
    }
    int c = client.read();
    if (c < 0) continue;
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
  client.stop();
  if (found) Serial.printf("stamp %s\n", stamp);
  return found;
}

static bool fetchPng(const Sat &s, const char *stamp, uint8_t **outBuf, int *outLen) {
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
  Serial.printf("png %u bytes\n", (unsigned)clen);
  return true;
}

int onLine(PNGDRAW *p) {
  int dstY = (int)((int32_t)p->y * 128 / png.getHeight());
  int prev = (int)((int32_t)(p->y - 1) * 128 / png.getHeight());
  if (p->y > 0 && dstY == prev) return 1;

  uint16_t line[700];
  png.getLineAsRGB565(p, line, PNG_ENDIAN, 0x00000000);
  int srcW = png.getWidth();
  if (srcW > 700) srcW = 700;
  for (int x = 0; x < 128; x++) {
    int sx = (int)((int32_t)x * srcW / 128);
    frame[dstY * 128 + x] = line[sx];
  }
  return 1;
}

static bool decodeToFrame(uint8_t *pngBuf, int pngLen) {
  int openRc = png.openRAM(pngBuf, pngLen, onLine);
  if (openRc != PNG_SUCCESS) {
    Serial.printf("png open rc=%d err=%d\n", openRc, png.getLastError());
    return false;
  }
  int w = png.getWidth();
  int h = png.getHeight();
  if (w <= 0 || w > 700 || h <= 0) {
    Serial.printf("png size refused %dx%d\n", w, h);
    png.close();
    return false;
  }
  int rc = png.decode(NULL, 0);
  if (rc != PNG_SUCCESS) {
    Serial.printf("png decode rc=%d err=%d\n", rc, png.getLastError());
  }
  png.close();
  return rc == PNG_SUCCESS;
}

static bool loadSatellite(int i) {
  const Sat &s = kRing[i];
  Serial.printf("load %s\n", s.id);

  char stamp[15];
  if (!fetchStamp(s, stamp)) return false;

  uint8_t *pngBuf = nullptr;
  int pngLen = 0;
  if (!fetchPng(s, stamp, &pngBuf, &pngLen)) return false;

  bool ok = decodeToFrame(pngBuf, pngLen);
  free(pngBuf);
  if (!ok) {
    Serial.println("png decode fail");
    return false;
  }
  pushFrame();
  haveFrame = true;
  return true;
}

static void loadUntilCaughtUp() {
  fetching = true;
  int started;
  do {
    started = satIndex;
    if (!haveFrame) {
      char msg[40];
      snprintf(msg, sizeof(msg), "Loading\n%s", kRing[started].id);
      showMessage(msg);
    }
    if (loadSatellite(started)) {
      lastFetchMs = millis();
      loadedIndex = started;
      failedIndex = -1;
    } else {
      failedIndex = started;
      if (!haveFrame) showMessage("Fetch failed");
    }
    // If a press arrived mid-download, satIndex moved; fetch that one next.
  } while (satIndex != started);
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
  if (!frame) {
    showMessage("PSRAM fail");
    while (true) delay(1000);
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
  }
  delay(10);
}
