/**
 * Plane Radar for AIPI-Lite — ADS-B sonar-style radar on the 128×128 ST7735.
 *
 * Port of https://github.com/RockBase-iot/ESP32-Plane-Radar
 *
 * Button A: tap = sound on/off; hold 5 s = power off
 * Button B: range presets (first tap ignored; later taps cycle 5/10/15/25 km)
 *           hold 3 s to clear Wi-Fi and reopen the setup portal
 */

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "hardware/beep.h"
#include "hardware/buttons.h"
#include "hardware/display.h"
#include "hardware/power.h"
#include "services/adsb_client.h"
#include "services/radar_location.h"
#include "services/time_settings.h"
#include "services/wifi_setup.h"
#include "ui/radar_animation_policy.h"
#include "ui/radar_display.h"
#include "ui/radar_range.h"
#include "ui/status_screens.h"

namespace {

volatile bool g_radar_visible = false;
bool g_range_button_armed = false;
unsigned long g_wifi_down_since = 0;
unsigned long g_last_reconnect_ms = 0;
unsigned long g_last_animation_refresh_ms = 0;

portMUX_TYPE g_adsb_update_lock = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t g_adsb_fetch_task = nullptr;
volatile bool g_adsb_refresh_pending = false;
unsigned long g_last_background_adsb_fetch_ms = 0;

void showRadarIfConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    g_radar_visible = false;
    return;
  }
  ui::radarDisplayDraw();
  g_last_animation_refresh_ms = millis();
  g_radar_visible = true;
}

void onRangeTap() {
  ui::radar::rangeNext();
  char range_label[12];
  ui::radar::formatCurrentRing3Label(range_label, sizeof(range_label));
  Serial.printf("Range: %s (outer ~%.0f km)\n", range_label,
                ui::radar::rangeCurrent().outer_km);

  if (g_radar_visible && WiFi.status() == WL_CONNECTED) {
    ui::radarDisplayDraw();
    g_last_animation_refresh_ms = millis();
  }
}

void onRangeClick() {
  powerNoteActivity();
  // First tap after boot is ignored so a wake/press does not change range.
  if (!g_range_button_armed) {
    g_range_button_armed = true;
    Serial.println("Range: first B tap ignored");
    return;
  }
  onRangeTap();
}

void onSoundClick() {
  beepToggle();
  if (beepEnabled()) {
    beepPing();
  }
  if (g_radar_visible && WiFi.status() == WL_CONNECTED) {
    ui::radarDisplayDraw();
    g_last_animation_refresh_ms = millis();
  }
}

void onWifiResetHold() {
  Serial.println("B held — resetting WiFi");
  wifiResetCredentialsAndReboot();
}

void serviceSweepPings() {
  const uint8_t hits = ui::radarDisplayTakeSweepHits();
  for (uint8_t i = 0; i < hits; ++i) {
    beepPing();
  }
  beepService();
}

void serviceRadarAnimation() {
  if (!g_radar_visible || WiFi.status() != WL_CONNECTED) {
    return;
  }

  const unsigned long now_ms = millis();
  if (!ui::radar::animationNeeded(ui::radar::showSweep(),
                                  services::adsb::aircraftCount()) ||
      now_ms - g_last_animation_refresh_ms <
          ui::radar::animationIntervalMs(true)) {
    return;
  }

  g_last_animation_refresh_ms = now_ms;
  ui::radarDisplayRefreshAircraft();
}

void pollBackgroundNetwork() { taskYIELD(); }

void fetchAndRequestAircraft() {
  const float fetch_km = ui::radar::fetchRadiusKm();
  if (!services::adsb::fetchUpdate(services::location::lat(),
                                   services::location::lon(), fetch_km)) {
    return;
  }

  taskENTER_CRITICAL(&g_adsb_update_lock);
  g_adsb_refresh_pending = true;
  taskEXIT_CRITICAL(&g_adsb_update_lock);
}

bool consumeBackgroundAdsbUpdate() {
  taskENTER_CRITICAL(&g_adsb_update_lock);
  const bool pending = g_adsb_refresh_pending;
  if (pending) {
    g_adsb_refresh_pending = false;
  }
  taskEXIT_CRITICAL(&g_adsb_update_lock);
  return pending;
}

void adsbFetchTask(void *) {
  for (;;) {
    if (g_radar_visible && WiFi.status() == WL_CONNECTED) {
      const unsigned long now_ms = millis();
      if (now_ms - g_last_background_adsb_fetch_ms >=
          config::kAdsbFetchIntervalMs) {
        g_last_background_adsb_fetch_ms = now_ms;
        fetchAndRequestAircraft();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

}  // namespace

void setup() {
  powerInit();
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.printf("Plane Radar %s\n", config::kFirmwareVersion);

  buttonsInit();
  buttonsOnPress(powerNoteActivity);
  buttonsOnAClick(onSoundClick);
  buttonsOnALongPress(powerOff);
  buttonsOnBClick(onRangeClick);
  buttonsOnBLongPress(onWifiResetHold);
  displayInit();
  beepInit();

  if (wifiShowsSetupScreenOnBoot()) {
    statusScreenPortal();
  }

  services::location::init();
  services::time_settings::init();
  ui::radar::rangeInit();
  services::adsb::setPollFn(pollBackgroundNetwork);

  if (wifiSetupConnect()) {
    showRadarIfConnected();
  }

  // Core 0 = Wi-Fi; Arduino loop stays on core 1 so the sweep keeps moving
  // while HTTPS/ADS-B runs in the background.
  if (xTaskCreatePinnedToCore(adsbFetchTask, "adsb-fetch", 16384, nullptr, 1,
                              &g_adsb_fetch_task, 0) != pdPASS) {
    Serial.println("adsb: background task create failed");
  }
}

void loop() {
  powerPoll();
  wifiLoop();

  if (WiFi.status() != WL_CONNECTED) {
    if (g_radar_visible) {
      Serial.println("WiFi lost — will reconnect");
      g_radar_visible = false;
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
        showRadarIfConnected();
      }
    }
  } else {
    g_wifi_down_since = 0;
    if (!g_radar_visible) {
      showRadarIfConnected();
    } else if (consumeBackgroundAdsbUpdate()) {
      ui::radarDisplayRefreshAircraft();
      g_last_animation_refresh_ms = millis();
    } else {
      serviceRadarAnimation();
    }
  }

  serviceSweepPings();
  delay(10);
}
