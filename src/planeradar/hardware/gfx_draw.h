#pragma once

#include <Arduino_GFX_Library.h>

enum class TextAnchor {
  TopLeft,
  TopCenter,
  TopRight,
  MiddleLeft,
  MiddleCenter,
  MiddleRight,
  BottomLeft,
  BottomCenter,
  BottomRight,
};

int gfxTextWidth(Arduino_GFX &g, const char *s);
int gfxTextHeight(Arduino_GFX &g);
void gfxDrawText(Arduino_GFX &g, const char *s, int x, int y, TextAnchor anchor,
                 uint16_t fg, uint16_t bg);
void gfxDrawWideLine(Arduino_GFX &g, int x0, int y0, int x1, int y1, float half_w,
                     uint16_t color);
