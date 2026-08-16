#include "hardware/gfx_draw.h"

#include <cmath>
#include <cstring>

int gfxTextWidth(Arduino_GFX &g, const char *s) {
  if (s == nullptr || s[0] == '\0') {
    return 0;
  }
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  g.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return static_cast<int>(w);
}

int gfxTextHeight(Arduino_GFX &g) {
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  g.getTextBounds("A", 0, 0, &x1, &y1, &w, &h);
  return static_cast<int>(h);
}

void gfxDrawText(Arduino_GFX &g, const char *s, int x, int y, TextAnchor anchor,
                 uint16_t fg, uint16_t bg) {
  if (s == nullptr || s[0] == '\0') {
    return;
  }
  const int w = gfxTextWidth(g, s);
  const int h = gfxTextHeight(g);
  int left = x;
  int top = y;
  switch (anchor) {
    case TextAnchor::TopLeft:
      break;
    case TextAnchor::TopCenter:
      left = x - w / 2;
      break;
    case TextAnchor::TopRight:
      left = x - w;
      break;
    case TextAnchor::MiddleLeft:
      top = y - h / 2;
      break;
    case TextAnchor::MiddleCenter:
      left = x - w / 2;
      top = y - h / 2;
      break;
    case TextAnchor::MiddleRight:
      left = x - w;
      top = y - h / 2;
      break;
    case TextAnchor::BottomLeft:
      top = y - h;
      break;
    case TextAnchor::BottomCenter:
      left = x - w / 2;
      top = y - h;
      break;
    case TextAnchor::BottomRight:
      left = x - w;
      top = y - h;
      break;
  }
  g.setTextColor(fg, bg);
  g.setCursor(left, top);
  g.print(s);
}

void gfxDrawWideLine(Arduino_GFX &g, int x0, int y0, int x1, int y1, float half_w,
                     uint16_t color) {
  g.drawLine(x0, y0, x1, y1, color);
  if (half_w < 0.75f) {
    return;
  }
  const int dx = x1 - x0;
  const int dy = y1 - y0;
  if (std::abs(dx) >= std::abs(dy)) {
    g.drawLine(x0, y0 + 1, x1, y1 + 1, color);
  } else {
    g.drawLine(x0 + 1, y0, x1 + 1, y1, color);
  }
}
