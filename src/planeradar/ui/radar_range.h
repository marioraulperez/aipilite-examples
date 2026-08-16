#pragma once

#include <cstddef>
#include <cstdint>

namespace ui::radar {

/**
 * Range presets (label on ring 3 = ¾ of outer radius).
 *
 *   5 km  — default; pattern / very local
 *  10 km  — neighborhood spotting
 *  15 km  — wider local area
 *  25 km  — metro / regional picture
 *
 * Outer radius (for aircraft math) is ring-3 distance ÷ 0.75.
 */
struct RangePreset {
  float ring3_km;
  float outer_km;
};

constexpr float kRing3ToOuterKm = 4.0f / 3.0f;

constexpr RangePreset kRangePresets[] = {
    {5.0f, 5.0f * kRing3ToOuterKm},
    {10.0f, 10.0f * kRing3ToOuterKm},
    {15.0f, 15.0f * kRing3ToOuterKm},
    {25.0f, 25.0f * kRing3ToOuterKm},
};

constexpr size_t kRangePresetCount =
    sizeof(kRangePresets) / sizeof(kRangePresets[0]);

void rangeInit();
void rangeNext();
const RangePreset &rangeCurrent();
uint8_t rangeIndex();
float fetchRadiusKm();

bool useMiles();
bool showRunways();
bool showSweep();
void saveMilesFromPortal(const char *checkbox_value);
void saveRunwaysFromPortal(const char *checkbox_value);
void saveSweepFromPortal(const char *checkbox_value);
void formatRing3Label(char *buf, size_t len, float ring3_km, bool use_miles);
void formatCurrentRing3Label(char *buf, size_t len);
void unitsReset();

}  // namespace ui::radar
