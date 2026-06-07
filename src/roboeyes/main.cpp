// RoboEyes demo for AIPI-Lite — animated robot eyes on the 128x128 face display.
// Vendored FluxGarage RoboEyes (patched for square screens) + Arduino_GFX adapter.
// Button A: cycle mood. Button B: play animation.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "FluxGarage_RoboEyes.h"
#include "../pins.h"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 128

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_CLK, PIN_LCD_MOSI);
Arduino_GFX *panel = new Arduino_ST7735(bus, PIN_LCD_RST, 3, false, 128, 128, 0, 0);
Arduino_Canvas *canvas = new Arduino_Canvas(SCREEN_WIDTH, SCREEN_HEIGHT, panel);

// Draw into an off-screen buffer; flush once per frame to avoid visible flicker.
class GfxRoboEyes {
  Arduino_Canvas *g;

public:
  explicit GfxRoboEyes(Arduino_Canvas *gfx) : g(gfx) {}

  void clearDisplay() { g->fillScreen(RGB565_BLACK); }
  void display() { g->flush(); }

  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                     int16_t r, uint16_t color) {
    g->fillRoundRect(x, y, w, h, r, color ? RGB565_WHITE : RGB565_BLACK);
  }

  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    int16_t x2, int16_t y2, uint16_t color) {
    g->fillTriangle(x0, y0, x1, y1, x2, y2, color ? RGB565_WHITE : RGB565_BLACK);
  }
};

GfxRoboEyes display(canvas);
RoboEyes<GfxRoboEyes> roboEyes(display);

struct Button {
  int pin, state;
  unsigned long lastChange;
};

static Button btns[] = {{PIN_BTN_A, HIGH, 0}, {PIN_BTN_B, HIGH, 0}};
static const uint8_t MOODS[] = {DEFAULT, TIRED, ANGRY, HAPPY};
static uint8_t moodIndex = 0;
static uint8_t animIndex = 0;

bool readButton(Button &btn) {
  int raw = digitalRead(btn.pin);
  if (raw == btn.state) { btn.lastChange = 0; return false; }
  if (!btn.lastChange) { btn.lastChange = millis(); return false; }
  if (millis() - btn.lastChange < 50) return false;
  btn.state = raw;
  btn.lastChange = 0;
  return btn.state == LOW;
}

void setup() {
  pinMode(PIN_PWR_CTL, OUTPUT);
  digitalWrite(PIN_PWR_CTL, HIGH);

  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  Serial.begin(115200);

  canvas->begin();
  canvas->fillScreen(RGB565_BLACK);
  canvas->flush();

  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 30);
  roboEyes.setWidth(40, 40);
  roboEyes.setHeight(40, 40);
  roboEyes.setBorderradius(10, 10);
  roboEyes.setSpacebetween(10);
  roboEyes.setPosition(DEFAULT);
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);
  roboEyes.setCuriosity(ON);
}

void loop() {
  if (readButton(btns[0])) {
    moodIndex = (moodIndex + 1) % (sizeof(MOODS) / sizeof(MOODS[0]));
    roboEyes.setMood(MOODS[moodIndex]);
    Serial.printf("Mood: %u\n", MOODS[moodIndex]);
  }

  if (readButton(btns[1])) {
    if (animIndex % 2 == 0) {
      roboEyes.anim_laugh();
      Serial.println("Anim: laugh");
    } else {
      roboEyes.anim_confused();
      Serial.println("Anim: confused");
    }
    animIndex++;
  }

  roboEyes.update();
}
