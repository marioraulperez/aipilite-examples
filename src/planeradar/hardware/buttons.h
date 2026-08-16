#pragma once

/** OneButton wrappers for A (sound / power) and B (range / Wi-Fi reset). */
void buttonsInit();
void buttonsTick();
void buttonsOnAClick(void (*fn)());
void buttonsOnALongPress(void (*fn)());
void buttonsOnBClick(void (*fn)());
void buttonsOnBLongPress(void (*fn)());
void buttonsOnPress(void (*fn)());
