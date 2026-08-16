#include "hardware/beep.h"

#include <Arduino.h>
#include <Preferences.h>
#include <cmath>
#include <cstring>

#include "AudioTools/AudioLibs/I2SCodecStream.h"
#include "../pins.h"

namespace {

constexpr int kSampleRate = 24000;
constexpr int kChannels = 2;
constexpr int kBits = 16;
constexpr float kPingHz = 1250.0f;
constexpr float kPingSeconds = 0.055f;
constexpr int kPingFrames = static_cast<int>(kSampleRate * kPingSeconds);
constexpr size_t kPingBytes = static_cast<size_t>(kPingFrames * kChannels * (kBits / 8));
constexpr int kMaxQueued = 6;
constexpr size_t kWriteChunk = 512;

AudioInfo s_info(kSampleRate, kChannels, kBits);
DriverPins s_pins;
AudioBoard s_board(AudioDriverES8311, s_pins);
I2SCodecStream s_i2s(s_board);

int16_t s_wave[kPingFrames * kChannels];
size_t s_play_pos = 0;
bool s_playing = false;
int s_queued = 0;
bool s_enabled = false;
bool s_inited = false;

void persistEnabled() {
  Preferences prefs;
  if (!prefs.begin("planeradar", false)) {
    return;
  }
  prefs.putBool("audioOn", s_enabled);
  prefs.end();
}

void loadEnabled() {
  Preferences prefs;
  if (!prefs.begin("planeradar", true)) {
    return;
  }
  s_enabled = prefs.getBool("audioOn", false);
  prefs.end();
}

void buildPing() {
  constexpr float kTwoPi = 6.28318530718f;
  for (int i = 0; i < kPingFrames; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(kPingFrames);
    const float env = (1.0f - t) * (1.0f - t);
    const float sample = sinf(kTwoPi * kPingHz * static_cast<float>(i) / kSampleRate) *
                         env * 0.28f;
    const int16_t v = static_cast<int16_t>(sample * 32767.0f);
    s_wave[i * 2] = v;
    s_wave[i * 2 + 1] = v;
  }
}

void applyAmp() {
  digitalWrite(PIN_SPKR_EN, s_enabled ? HIGH : LOW);
}

}  // namespace

void beepInit() {
  loadEnabled();
  buildPing();

  pinMode(PIN_SPKR_EN, OUTPUT);
  applyAmp();

  s_pins.addI2C(PinFunction::CODEC, I2C_SCL, I2C_SDA);
  s_pins.addI2S(PinFunction::CODEC, I2S_MCLK, I2S_BCLK, I2S_LRCLK, I2S_DOUT, I2S_DIN);

  auto cfg = s_i2s.defaultConfig(TX_MODE);
  cfg.copyFrom(s_info);
  cfg.output_device = DAC_OUTPUT_ALL;
  cfg.is_master = true;
  cfg.fixed_mclk = kSampleRate * 256;
  if (!s_i2s.begin(cfg)) {
    Serial.println("beep: ES8311 init failed");
    return;
  }
  s_i2s.setVolume(0.7f);
  s_inited = true;
  Serial.printf("beep: ready (%s)\n", s_enabled ? "on" : "muted");
}

void beepSetEnabled(bool on) {
  s_enabled = on;
  if (!s_enabled) {
    s_queued = 0;
    s_playing = false;
    s_play_pos = 0;
  }
  applyAmp();
  persistEnabled();
  Serial.printf("beep: %s\n", s_enabled ? "on" : "muted");
}

bool beepEnabled() { return s_enabled; }

void beepToggle() { beepSetEnabled(!s_enabled); }

void beepPing() {
  if (!s_inited || !s_enabled) {
    return;
  }
  if (s_queued < kMaxQueued) {
    ++s_queued;
  }
}

void beepService() {
  if (!s_inited || !s_enabled) {
    return;
  }

  if (!s_playing) {
    if (s_queued <= 0) {
      return;
    }
    --s_queued;
    s_playing = true;
    s_play_pos = 0;
  }

  const uint8_t *src = reinterpret_cast<const uint8_t *>(s_wave);
  while (s_playing) {
    const size_t remain = kPingBytes - s_play_pos;
    const size_t n = remain < kWriteChunk ? remain : kWriteChunk;
    const size_t w = s_i2s.write(src + s_play_pos, n);
    if (w == 0) {
      break;
    }
    s_play_pos += w;
    if (s_play_pos >= kPingBytes) {
      s_playing = false;
      s_play_pos = 0;
      break;
    }
  }
}
