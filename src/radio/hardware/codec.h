#pragma once

#include <cstdint>

/** ES8311 DAC init (I2S slave, 16-bit, 48 kHz, MCLK = 256 * Fs). */
bool codecInit();
void codecSetVolume(uint8_t percent);
