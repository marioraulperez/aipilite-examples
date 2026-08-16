#include "ui/status_screens.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "hardware/display.h"
#include "hardware/gfx_draw.h"

namespace {

constexpr int kCenterX = config::kDisplayWidth / 2;
constexpr int kCenterY = config::kDisplayHeight / 2;
constexpr int kSpinnerDotCount = 8;
constexpr int kSpinnerRadius = 56;
constexpr int kSpinnerDotRadius = 2;
constexpr float kSpinnerStepDeg = 8.0f;

char s_connecting_ssid[33];
float s_spinner_angle_deg = -90.0f;

void drawCenteredBlock(uint16_t bg, uint16_t fg, const char *const *lines,
                       size_t count) {
  gfx->fillScreen(bg);
  gfx->setTextSize(1);
  const int line_h = gfxTextHeight(*gfx);
  const int total_h = static_cast<int>(count) * line_h +
                      static_cast<int>(count > 0 ? count - 1 : 0) * 2;
  int y = (config::kDisplayHeight - total_h) / 2;
  for (size_t i = 0; i < count; ++i) {
    gfxDrawText(*gfx, lines[i], kCenterX, y + line_h / 2, TextAnchor::MiddleCenter,
                fg, bg);
    y += line_h + 2;
  }
  displayFlush();
}

void drawSpinnerDots() {
  constexpr float kDegToRad = 0.01745329252f;
  const float head_rad = s_spinner_angle_deg * kDegToRad;
  for (int i = 0; i < kSpinnerDotCount; ++i) {
    const float a =
        head_rad - static_cast<float>(i) * (6.283185307f / kSpinnerDotCount);
    const int x = kCenterX + static_cast<int>(lroundf(cosf(a) * kSpinnerRadius));
    const int y = kCenterY + static_cast<int>(lroundf(sinf(a) * kSpinnerRadius));
    const int fade = 255 - i * 28;
    const uint16_t color = gfx->color565(0, fade < 40 ? 40 : fade, 0);
    gfx->fillCircle(x, y, kSpinnerDotRadius, color);
  }
}

void drawConnectingFrame() {
  gfx->fillScreen(config::kColorBlack);
  gfx->setTextSize(1);
  gfxDrawText(*gfx, "Connecting to", kCenterX, kCenterY - 8,
              TextAnchor::MiddleCenter, config::kTextOnBlack, config::kColorBlack);
  gfxDrawText(*gfx, s_connecting_ssid, kCenterX, kCenterY + 8,
              TextAnchor::MiddleCenter, config::kTextOnBlack, config::kColorBlack);
  drawSpinnerDots();
  displayFlush();
}

}  // namespace

void statusScreenConnectingBegin(const char *ssid) {
  const char *name = (ssid != nullptr && ssid[0] != '\0') ? ssid : "network";
  strncpy(s_connecting_ssid, name, sizeof(s_connecting_ssid) - 1);
  s_connecting_ssid[sizeof(s_connecting_ssid) - 1] = '\0';
  if (strlen(s_connecting_ssid) > 18) {
    s_connecting_ssid[15] = '.';
    s_connecting_ssid[16] = '.';
    s_connecting_ssid[17] = '.';
    s_connecting_ssid[18] = '\0';
  }
  s_spinner_angle_deg = -90.0f;
  drawConnectingFrame();
}

void statusScreenConnectingTick() {
  s_spinner_angle_deg += kSpinnerStepDeg;
  if (s_spinner_angle_deg >= 270.0f) {
    s_spinner_angle_deg -= 360.0f;
  }
  drawConnectingFrame();
}

void statusScreenPortal() {
  const char *lines[] = {
      "Wi-Fi setup",
      "Join AP:",
      config::kPortalApName,
      "Then open:",
      config::kPortalHostUrl,
      "or 192.168.4.1",
  };
  drawCenteredBlock(config::kColorYellow, config::kTextOnYellow, lines,
                    sizeof(lines) / sizeof(lines[0]));
}

void statusScreenConnectFailed() {
  const char *lines[] = {
      "Wi-Fi failed",
      "Hold B 3 sec",
      "to reset Wi-Fi",
      "Hold A 5 sec",
      "to power off",
  };
  drawCenteredBlock(config::kColorYellow, config::kTextOnYellow, lines,
                    sizeof(lines) / sizeof(lines[0]));
}

void statusScreenWifiReset() {
  const char *lines[] = {
      "Wi-Fi reset",
      "Restarting...",
  };
  drawCenteredBlock(config::kColorYellow, config::kTextOnYellow, lines,
                    sizeof(lines) / sizeof(lines[0]));
}
