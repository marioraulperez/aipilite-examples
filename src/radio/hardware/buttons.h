#pragma once

void buttonsInit();
void buttonsTick();
void buttonsOnAClick(void (*fn)());
void buttonsOnADoubleClick(void (*fn)());
void buttonsOnALongPress(void (*fn)());
void buttonsOnBClick(void (*fn)());
void buttonsOnBDoubleClick(void (*fn)());
void buttonsOnBLongPress(void (*fn)());
void buttonsOnPress(void (*fn)());
