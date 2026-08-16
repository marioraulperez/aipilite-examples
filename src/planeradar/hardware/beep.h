#pragma once

/** ES8311 ping when the sweep beam hits an aircraft. */
void beepInit();
void beepSetEnabled(bool on);
bool beepEnabled();
void beepToggle();
/** Queue one short ping (ignored while muted). */
void beepPing();
/** Feed I2S; call from the main loop. */
void beepService();
