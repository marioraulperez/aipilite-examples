#include "services/player.h"

#include <Arduino.h>
#include <Audio.h>
#include <WiFi.h>

#include <cstring>

#include "../pins.h"
#include "config.h"
#include "hardware/codec.h"

namespace {

Audio s_audio;
char s_title[96] = "Ahora suena...";
volatile bool s_eof = false;
uint8_t s_volume = config::kVolumeDefault;

}  // namespace

// ESP32-audioI2S 3.0.x uses weak callbacks (not Audio::msg_t).
void audio_info(const char *info) {
  if (info != nullptr) {
    Serial.printf("audio: %s\n", info);
  }
}

void audio_showstreamtitle(const char *info) {
  if (info == nullptr || info[0] == '\0') {
    return;
  }
  strncpy(s_title, info, sizeof(s_title) - 1);
  s_title[sizeof(s_title) - 1] = '\0';
}

void audio_eof_stream(const char *info) {
  Serial.printf("audio eof: %s\n", info != nullptr ? info : "");
  s_eof = true;
}

void playerInit() {
  pinMode(PIN_SPKR_EN, OUTPUT);
  digitalWrite(PIN_SPKR_EN, HIGH);

  if (!codecInit()) {
    Serial.println("codec init failed — continuing anyway");
  }
  codecSetVolume(config::kCodecVolume);

  s_audio.setPinout(I2S_BCLK, I2S_LRCLK, I2S_DOUT, I2S_MCLK);
  s_audio.setVolume(s_volume);
  s_audio.forceMono(true);
  s_audio.setConnectionTimeout(3500, 8000);
}

void playerLoop() { s_audio.loop(); }

void playerConnect(const char *url) {
  if (url == nullptr || url[0] == '\0') {
    return;
  }
  playerClearTitle();
  pinMode(PIN_SPKR_EN, OUTPUT);
  digitalWrite(PIN_SPKR_EN, HIGH);
  s_audio.stopSong();
  Serial.printf("radio: connect %s\n", url);
  s_audio.connecttohost(url);
}

void playerStop() {
  s_audio.stopSong();
  digitalWrite(PIN_SPKR_EN, LOW);
}

bool playerIsRunning() { return s_audio.isRunning(); }

uint8_t playerVolume() { return s_volume; }

void playerSetVolume(uint8_t volume) {
  if (volume > config::kVolumeMax) {
    volume = config::kVolumeMax;
  }
  s_volume = volume;
  s_audio.setVolume(s_volume);
  Serial.printf("volume: %u\n", s_volume);
}

void playerVolumeUp() {
  if (s_volume < config::kVolumeMax) {
    playerSetVolume(static_cast<uint8_t>(s_volume + 1));
  }
}

void playerVolumeDown() {
  if (s_volume > config::kVolumeMin) {
    playerSetVolume(static_cast<uint8_t>(s_volume - 1));
  }
}

const char *playerStreamTitle() { return s_title; }

void playerClearTitle() {
  strncpy(s_title, "Ahora suena...", sizeof(s_title) - 1);
  s_title[sizeof(s_title) - 1] = '\0';
}

bool playerTakeEof() {
  const bool eof = s_eof;
  s_eof = false;
  return eof;
}
