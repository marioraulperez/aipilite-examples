#include "ui/flags.h"

#include <cstring>

#include "hardware/display.h"

namespace {

constexpr int kW = 24;
constexpr int kH = 16;

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return gfx->color565(r, g, b); }

void hBands3(int x, int y, uint16_t a, uint16_t b, uint16_t c) {
  gfx->fillRect(x, y, kW, 5, a);
  gfx->fillRect(x, y + 5, kW, 6, b);
  gfx->fillRect(x, y + 11, kW, 5, c);
}

void vBands3(int x, int y, uint16_t a, uint16_t b, uint16_t c) {
  gfx->fillRect(x, y, 8, kH, a);
  gfx->fillRect(x + 8, y, 8, kH, b);
  gfx->fillRect(x + 16, y, 8, kH, c);
}

}  // namespace

void flagsDraw(const char *iso, int x, int y) {
  gfx->drawRect(x - 1, y - 1, kW + 2, kH + 2, gfx->color565(40, 40, 40));

  if (iso == nullptr || iso[0] == '\0') {
    gfx->fillRect(x, y, kW, kH, RGB565_WHITE);
    return;
  }

  if (strcmp(iso, "AR") == 0) {
    hBands3(x, y, rgb(116, 172, 223), RGB565_WHITE, rgb(116, 172, 223));
    gfx->fillCircle(x + 12, y + 8, 2, rgb(246, 180, 14));
    return;
  }
  if (strcmp(iso, "MX") == 0) {
    vBands3(x, y, rgb(0, 104, 71), RGB565_WHITE, rgb(206, 17, 38));
    gfx->fillCircle(x + 12, y + 8, 1, rgb(0, 104, 71));
    return;
  }
  if (strcmp(iso, "UY") == 0) {
    gfx->fillRect(x, y, kW, kH, RGB565_WHITE);
    for (int i = 0; i < 4; ++i) {
      gfx->fillRect(x, y + 2 + i * 4, kW, 2, rgb(0, 56, 168));
    }
    gfx->fillRect(x, y, 10, 8, RGB565_WHITE);
    gfx->fillCircle(x + 5, y + 4, 2, rgb(246, 180, 14));
    return;
  }
  if (strcmp(iso, "CL") == 0) {
    gfx->fillRect(x, y, kW, 8, RGB565_WHITE);
    gfx->fillRect(x, y, 8, 8, rgb(0, 57, 166));
    gfx->fillRect(x, y + 8, kW, 8, rgb(213, 43, 30));
    gfx->fillCircle(x + 4, y + 4, 1, RGB565_WHITE);
    return;
  }
  if (strcmp(iso, "AU") == 0) {
    gfx->fillRect(x, y, kW, kH, rgb(0, 0, 102));
    gfx->fillRect(x, y, 10, 8, rgb(0, 51, 153));
    gfx->drawLine(x, y, x + 9, y + 7, RGB565_WHITE);
    gfx->drawLine(x, y + 7, x + 9, y, RGB565_WHITE);
    gfx->fillCircle(x + 16, y + 10, 1, RGB565_WHITE);
    gfx->fillCircle(x + 20, y + 6, 1, RGB565_WHITE);
    return;
  }
  if (strcmp(iso, "US") == 0) {
    gfx->fillRect(x, y, kW, kH, rgb(187, 19, 50));
    for (int i = 0; i < 4; ++i) {
      gfx->fillRect(x, y + 2 + i * 4, kW, 2, RGB565_WHITE);
    }
    gfx->fillRect(x, y, 10, 8, rgb(10, 49, 97));
    return;
  }
  if (strcmp(iso, "GB") == 0) {
    gfx->fillRect(x, y, kW, kH, rgb(1, 33, 105));
    gfx->drawLine(x, y, x + kW - 1, y + kH - 1, RGB565_WHITE);
    gfx->drawLine(x, y + kH - 1, x + kW - 1, y, RGB565_WHITE);
    gfx->fillRect(x + 10, y, 4, kH, RGB565_WHITE);
    gfx->fillRect(x, y + 6, kW, 4, RGB565_WHITE);
    gfx->fillRect(x + 11, y, 2, kH, rgb(200, 16, 46));
    gfx->fillRect(x, y + 7, kW, 2, rgb(200, 16, 46));
    return;
  }
  if (strcmp(iso, "DE") == 0) {
    hBands3(x, y, RGB565_BLACK, rgb(221, 0, 0), rgb(255, 206, 0));
    return;
  }
  if (strcmp(iso, "ES") == 0) {
    gfx->fillRect(x, y, kW, 4, rgb(198, 11, 30));
    gfx->fillRect(x, y + 4, kW, 8, rgb(255, 196, 0));
    gfx->fillRect(x, y + 12, kW, 4, rgb(198, 11, 30));
    return;
  }

  gfx->fillRect(x, y, kW, kH, RGB565_WHITE);
}
