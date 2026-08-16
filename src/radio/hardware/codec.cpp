#include "hardware/codec.h"

#include <Arduino.h>
#include <Wire.h>

#include "../pins.h"

namespace {

bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

}  // namespace

bool codecInit() {
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  delay(20);

  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ES8311 not found on I2C");
    return false;
  }

  writeReg(0x00, 0x1F);
  delay(20);
  writeReg(0x00, 0x80);
  writeReg(0x01, 0x3F);

  // 48 kHz, MCLK = 12.288 MHz (256 * Fs). Matches Audio::SR_48000.
  writeReg(0x02, 0x00);
  writeReg(0x03, 0x10);
  writeReg(0x04, 0x10);
  writeReg(0x05, 0x00);
  writeReg(0x06, 0x03);
  writeReg(0x07, 0x00);
  writeReg(0x08, 0xFF);

  writeReg(0x09, 0x0C);
  writeReg(0x0A, 0x0C);
  writeReg(0x0D, 0x01);
  writeReg(0x0E, 0x02);
  writeReg(0x12, 0x00);
  writeReg(0x13, 0x10);
  writeReg(0x1C, 0x6A);
  writeReg(0x37, 0x08);
  codecSetVolume(80);
  return true;
}

void codecSetVolume(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  const uint8_t reg = (percent == 0) ? 0 : static_cast<uint8_t>((percent * 256 / 100) - 1);
  writeReg(0x32, reg);
}
