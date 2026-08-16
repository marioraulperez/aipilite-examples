#pragma once

/** Drive PWR_CTL high, enable backlight, arm the idle-off timer. */
void powerInit();
/** Tick buttons (OneButton) and the 3-minute idle timer. */
void powerPoll();
/** Reset the inactivity timer (any user input). */
void powerNoteActivity();
/** Pause idle-off while the captive portal is on screen. */
void powerPauseIdleTimeout(bool paused);
/** Screen off, drop PWR_CTL, deep-sleep fallback if USB keeps the chip alive. */
void powerOff();
