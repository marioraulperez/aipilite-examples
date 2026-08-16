#pragma once

#include <cstdint>

void playerInit();
void playerLoop();
void playerConnect(const char *url);
void playerStop();
bool playerIsRunning();
uint8_t playerVolume();
void playerSetVolume(uint8_t volume);
void playerVolumeUp();
void playerVolumeDown();
/** ICY stream title; empty if none yet. */
const char *playerStreamTitle();
void playerClearTitle();
bool playerTakeEof();
