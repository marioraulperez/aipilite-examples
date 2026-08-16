#include "hardware/display.h"

#include <Arduino.h>

#include "../pins.h"

namespace {

Arduino_DataBus *s_bus =
    new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_CLK, PIN_LCD_MOSI);
Arduino_GFX *s_panel =
    new Arduino_ST7735(s_bus, PIN_LCD_RST, 3, false, 128, 128, 0, 0);
Arduino_Canvas s_canvas(128, 128, s_panel);

}  // namespace

Arduino_Canvas *gfx = &s_canvas;

void displayInit() {
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  gfx->begin();
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextWrap(false);
  gfx->setTextSize(1);
  displayFlush();
}

void displayFlush() { gfx->flush(); }

void displayOff() {
  gfx->fillScreen(RGB565_BLACK);
  displayFlush();
  digitalWrite(PIN_LCD_BL, LOW);
}
