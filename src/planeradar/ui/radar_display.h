#pragma once

#include <cstdint>

namespace ui {

void radarDisplayDraw();
void radarDisplayRefreshAircraft();
/** Sweep-beam hits since the last take; each hit should play one ping. */
uint8_t radarDisplayTakeSweepHits();

}  // namespace ui
