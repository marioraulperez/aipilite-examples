/**
 * AIPi World Radio — kids internet radio on AIPI-Lite.
 *
 * Button A: tap = next country; double = volume down; hold 5 s = power off
 * Button B: station presets (first tap ignored; later taps cycle stations);
 *           double = volume up; hold 3 s = clear Wi-Fi
 */

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "hardware/buttons.h"
#include "hardware/display.h"
#include "hardware/power.h"
#include "services/player.h"
#include "services/radio_manager.h"
#include "services/wifi_setup.h"
#include "ui/radio_ui.h"
#include "ui/status_screens.h"

namespace {

bool g_radio_visible = false;
bool g_preset_button_armed = false;
unsigned long g_wifi_down_since = 0;
unsigned long g_last_reconnect_ms = 0;

void showRadioIfConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    g_radio_visible = false;
    powerSetPlaybackActive(false);
    return;
  }
  radioUiDraw();
  g_radio_visible = true;
}

void onCountryClick() {
  if (!g_radio_visible) {
    return;
  }
  radioManagerNextCountry();
  radioUiDraw();
}

void onStationClick() {
  powerNoteActivity();
  if (!g_preset_button_armed) {
    g_preset_button_armed = true;
    Serial.println("Station: first B tap ignored");
    return;
  }
  if (!g_radio_visible) {
    return;
  }
  radioManagerNextStation();
  radioUiDraw();
}

void onVolumeDown() {
  playerVolumeDown();
  if (g_radio_visible) {
    radioUiDraw();
  }
}

void onVolumeUp() {
  playerVolumeUp();
  if (g_radio_visible) {
    radioUiDraw();
  }
}

void onWifiResetHold() {
  Serial.println("B held — resetting WiFi");
  playerStop();
  wifiResetCredentialsAndReboot();
}

}  // namespace

void setup() {
  powerInit();
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.printf("AIPi Radio %s\n", config::kFirmwareVersion);

  buttonsInit();
  buttonsOnPress(powerNoteActivity);
  buttonsOnAClick(onCountryClick);
  buttonsOnADoubleClick(onVolumeDown);
  buttonsOnALongPress(powerOff);
  buttonsOnBClick(onStationClick);
  buttonsOnBDoubleClick(onVolumeUp);
  buttonsOnBLongPress(onWifiResetHold);

  displayInit();
  playerInit();
  radioManagerInit();

  if (wifiShowsSetupScreenOnBoot()) {
    statusScreenPortal();
  }

  if (wifiSetupConnect()) {
    radioManagerPlayCurrent();
    showRadioIfConnected();
  }
}

void loop() {
  playerLoop();
  powerPoll();
  wifiLoop();

  if (WiFi.status() != WL_CONNECTED) {
    if (g_radio_visible) {
      Serial.println("WiFi lost — will reconnect");
      playerStop();
      g_radio_visible = false;
      powerSetPlaybackActive(false);
    }

    if (g_wifi_down_since == 0) {
      g_wifi_down_since = millis();
    }

    const unsigned long down_ms = millis() - g_wifi_down_since;
    if (down_ms >= config::kWifiDownGraceMs &&
        millis() - g_last_reconnect_ms >= config::kWifiReconnectIntervalMs) {
      g_last_reconnect_ms = millis();
      if (wifiReconnect()) {
        g_wifi_down_since = 0;
        radioManagerReconnect();
        showRadioIfConnected();
      }
    }
  } else {
    g_wifi_down_since = 0;
    if (!g_radio_visible) {
      showRadioIfConnected();
    } else {
      if (playerTakeEof()) {
        Serial.println("stream ended — reconnecting");
        radioManagerReconnect();
      }
      powerSetPlaybackActive(playerIsRunning());
      radioUiLoop();
    }
  }

  vTaskDelay(1);
}
