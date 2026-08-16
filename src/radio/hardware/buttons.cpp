#include "hardware/buttons.h"

#include <OneButton.h>

#include "../pins.h"
#include "config.h"

namespace {

OneButton s_btn_a(PIN_BTN_A, true, true);
OneButton s_btn_b(PIN_BTN_B, true, true);

}  // namespace

void buttonsInit() {
  s_btn_a.setDebounceMs(40);
  s_btn_a.setClickMs(config::kClickMs);
  s_btn_a.setPressMs(static_cast<unsigned int>(config::kPowerHoldOffMs));

  s_btn_b.setDebounceMs(40);
  s_btn_b.setClickMs(config::kClickMs);
  s_btn_b.setPressMs(static_cast<unsigned int>(config::kWifiResetHoldMs));
}

void buttonsTick() {
  s_btn_a.tick();
  s_btn_b.tick();
}

void buttonsOnAClick(void (*fn)()) { s_btn_a.attachClick(fn); }

void buttonsOnADoubleClick(void (*fn)()) { s_btn_a.attachDoubleClick(fn); }

void buttonsOnALongPress(void (*fn)()) { s_btn_a.attachLongPressStart(fn); }

void buttonsOnBClick(void (*fn)()) { s_btn_b.attachClick(fn); }

void buttonsOnBDoubleClick(void (*fn)()) { s_btn_b.attachDoubleClick(fn); }

void buttonsOnBLongPress(void (*fn)()) { s_btn_b.attachLongPressStart(fn); }

void buttonsOnPress(void (*fn)()) {
  s_btn_a.attachPress(fn);
  s_btn_b.attachPress(fn);
}
