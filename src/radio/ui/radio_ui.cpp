#include "ui/radio_ui.h"

#include <Arduino.h>
#include <time.h>

#include <cstdio>
#include <cstring>

#include "config.h"
#include "hardware/display.h"
#include "services/player.h"
#include "services/radio_manager.h"
#include "ui/flags.h"

namespace {

constexpr int kCenterX = config::kDisplayWidth / 2;
int s_scroll_px = 0;
unsigned long s_last_scroll_ms = 0;
unsigned long s_last_clock_ms = 0;
char s_last_clock[8] = "";
char s_last_title[96] = "";
bool s_dirty = true;

int textWidth(const char *s) {
  if (s == nullptr || s[0] == '\0') {
    return 0;
  }
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return static_cast<int>(w);
}

void drawCentered(const char *s, int y, uint8_t size, uint16_t color) {
  gfx->setTextSize(size);
  gfx->setTextColor(color, config::kColorBlack);
  const int w = textWidth(s);
  gfx->setCursor((config::kDisplayWidth - w) / 2, y);
  gfx->print(s);
}

void formatClock(char *out, size_t out_len) {
  time_t now = time(nullptr);
  struct tm local;
  if (now < 1600000000 || localtime_r(&now, &local) == nullptr) {
    snprintf(out, out_len, "--:--");
    return;
  }
  snprintf(out, out_len, "%02d:%02d", local.tm_hour, local.tm_min);
}

void drawVolume() {
  const uint8_t vol = playerVolume();
  const int bars = 7;
  const int filled =
      (vol <= config::kVolumeMin)
          ? 0
          : 1 + (static_cast<int>(vol) * (bars - 1)) / config::kVolumeMax;
  const int y = 116;
  const int total_w = bars * 8 - 2;
  int x = (config::kDisplayWidth - total_w) / 2;
  for (int i = 0; i < bars; ++i) {
    const uint16_t color =
        (i < filled) ? config::kColorYellow : gfx->color565(40, 40, 40);
    gfx->fillRect(x, y, 6, 6, color);
    x += 8;
  }
}

void drawTitle(const char *title) {
  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_WHITE, config::kColorBlack);
  gfx->fillRect(0, 92, config::kDisplayWidth, 18, config::kColorBlack);

  const int w = textWidth(title);
  const int y = 96;
  if (w <= config::kDisplayWidth - 8) {
    gfx->setCursor((config::kDisplayWidth - w) / 2, y);
    gfx->print(title);
    return;
  }

  const int gap = 24;
  int x = 4 - s_scroll_px;
  gfx->setCursor(x, y);
  gfx->print(title);
  gfx->setCursor(x + w + gap, y);
  gfx->print(title);
}

void drawFrame() {
  gfx->fillScreen(config::kColorBlack);

  char clock[8];
  formatClock(clock, sizeof(clock));
  strncpy(s_last_clock, clock, sizeof(s_last_clock) - 1);
  drawCentered(clock, 6, 2, RGB565_WHITE);

  const RadioCountry &country = radioCurrentCountry();
  const RadioStation &station = radioCurrentStation();

  flagsDraw(country.code, 8, 32);
  gfx->setTextSize(1);
  gfx->setTextColor(config::kColorYellow, config::kColorBlack);
  gfx->setCursor(36, 36);
  gfx->print(country.name);

  gfx->setTextSize(2);
  const int name_w = textWidth(station.name);
  const uint8_t name_size = (name_w < config::kDisplayWidth - 8) ? 2 : 1;
  drawCentered(station.name, 62, name_size, RGB565_WHITE);

  strncpy(s_last_title, playerStreamTitle(), sizeof(s_last_title) - 1);
  s_last_title[sizeof(s_last_title) - 1] = '\0';
  drawTitle(s_last_title);
  drawVolume();
  displayFlush();
}

}  // namespace

void radioUiNoteStationChange() {
  s_scroll_px = 0;
  s_dirty = true;
}

void radioUiDraw() {
  s_dirty = true;
  drawFrame();
  s_dirty = false;
}

void radioUiLoop() {
  const unsigned long now = millis();
  const char *title = playerStreamTitle();
  const bool title_changed = strncmp(title, s_last_title, sizeof(s_last_title)) != 0;

  if (s_dirty || title_changed) {
    s_scroll_px = 0;
    drawFrame();
    s_dirty = false;
    s_last_clock_ms = now;
    s_last_scroll_ms = now;
    return;
  }

  if (now - s_last_clock_ms >= config::kClockRefreshMs) {
    char clock[8];
    formatClock(clock, sizeof(clock));
    if (strcmp(clock, s_last_clock) != 0) {
      drawFrame();
      s_last_clock_ms = now;
      return;
    }
    s_last_clock_ms = now;
  }

  gfx->setTextSize(1);
  const int w = textWidth(title);
  if (w > config::kDisplayWidth - 8 &&
      now - s_last_scroll_ms >= config::kScrollMs) {
    s_scroll_px++;
    const int gap = 24;
    if (s_scroll_px >= w + gap) {
      s_scroll_px = 0;
    }
    drawTitle(title);
    displayFlush();
    s_last_scroll_ms = now;
  }
}
