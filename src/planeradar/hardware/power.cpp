#include "hardware/power.h"

#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "../pins.h"
#include "config.h"
#include "hardware/buttons.h"
#include "hardware/display.h"

namespace {

unsigned long s_last_activity_ms = 0;
bool s_idle_paused = false;

}  // namespace

void powerInit() {
  rtc_gpio_hold_dis(static_cast<gpio_num_t>(PIN_PWR_CTL));
  pinMode(PIN_PWR_CTL, OUTPUT);
  digitalWrite(PIN_PWR_CTL, HIGH);

  pinMode(PIN_SPKR_EN, OUTPUT);
  digitalWrite(PIN_SPKR_EN, LOW);

  s_last_activity_ms = millis();
  s_idle_paused = false;
}

void powerNoteActivity() { s_last_activity_ms = millis(); }

void powerPauseIdleTimeout(bool paused) {
  s_idle_paused = paused;
  if (!paused) {
    s_last_activity_ms = millis();
  }
}

void powerOff() {
  digitalWrite(PIN_SPKR_EN, LOW);
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(RGB565_WHITE, RGB565_BLACK);
  gfx->setTextSize(1);
  gfx->setCursor(50, 60);
  gfx->print("Off");
  displayFlush();
  delay(400);

  displayOff();
  digitalWrite(PIN_PWR_CTL, LOW);
  delay(50);

  // USB may keep the chip powered after the battery latch drops.
  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(PIN_BTN_A), 0);
  esp_deep_sleep_start();
}

void powerPoll() {
  buttonsTick();
  if (!s_idle_paused && millis() - s_last_activity_ms >= config::kIdleOffMs) {
    powerOff();
  }
}
