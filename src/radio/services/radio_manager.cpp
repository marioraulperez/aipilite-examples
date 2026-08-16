#include "services/radio_manager.h"

#include <Arduino.h>

#include "services/player.h"
#include "ui/radio_ui.h"

namespace {

uint8_t s_country = 0;
uint8_t s_station = 0;

}  // namespace

void radioManagerInit() {
  s_country = 0;
  s_station = 0;
}

void radioManagerPlayCurrent() {
  const RadioStation &st = radioCurrentStation();
  radioUiNoteStationChange();
  playerConnect(st.url);
}

void radioManagerNextCountry() {
  s_country = static_cast<uint8_t>((s_country + 1) % kCountryCount);
  s_station = 0;
  Serial.printf("country: %s\n", radioCurrentCountry().name);
  radioManagerPlayCurrent();
}

void radioManagerNextStation() {
  const RadioCountry &c = radioCurrentCountry();
  if (c.station_count < 2) {
    Serial.println("station: only one in this country");
    return;
  }
  s_station = static_cast<uint8_t>((s_station + 1) % c.station_count);
  Serial.printf("station: %s\n", radioCurrentStation().name);
  radioManagerPlayCurrent();
}

void radioManagerReconnect() { radioManagerPlayCurrent(); }

const RadioCountry &radioCurrentCountry() { return kCountries[s_country]; }

const RadioStation &radioCurrentStation() {
  const RadioCountry &c = radioCurrentCountry();
  return c.stations[s_station];
}

uint8_t radioCountryIndex() { return s_country; }

uint8_t radioStationIndex() { return s_station; }
