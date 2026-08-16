#pragma once

void powerInit();
void powerPoll();
void powerNoteActivity();
void powerPauseIdleTimeout(bool paused);
/** Pause idle-off while a stream is playing so listening is not "inactivity". */
void powerSetPlaybackActive(bool playing);
void powerOff();
