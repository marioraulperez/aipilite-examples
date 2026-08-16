#pragma once

#include <cstdint>

namespace ui::radar {

constexpr int kSize = 128;
constexpr int kCenterX = kSize / 2;
constexpr int kCenterY = kSize / 2;

/** Outermost grid ring (inside edge labels). */
constexpr int kGridOuterRadius = 52;

constexpr int kCardinalNorthOffsetY = 1;
constexpr int kCardinalSouthOffsetY = -1;

constexpr int kSpeakerIconSize = 6;
constexpr int kSpeakerIconMarginPx = 1;
constexpr int kSpeakerIconX = kSize - kSpeakerIconSize - kSpeakerIconMarginPx;
constexpr int kSpeakerIconY = kSpeakerIconMarginPx;

/** Gap between scale label right edge and outer ring on the east spoke (px). */
constexpr int kScaleGapFromOuterRing = 3;

constexpr int kRingCount = 4;

constexpr float kGridStrokeHalfWidth = 0.5f;

constexpr int kCenterDotRadius = 2;

constexpr int kAircraftNoseLenPx = 5;
constexpr int kAircraftTailLenPx = 2;
constexpr int kAircraftTailHalfPx = 3;
constexpr float kAircraftTrackHorizonSec = 60.0f;
constexpr int kAircraftSpeedLineMinPx = 2;
constexpr float kAircraftTrackRefOuterKm = 13.3f;
constexpr float kAircraftTrackLengthScale = 1.5f / 5.0f;
constexpr float kAircraftTrackLineHalfWidth = 0.5f;

constexpr float kRunwayLineWidthPx = 1.0f;
constexpr float kRunwayLineHalfWidth = kRunwayLineWidthPx * 0.5f;
constexpr int kRunwayLabelGapPx = 2;
constexpr int kAircraftLabelGapPx = 1;
constexpr int kAircraftInsideRingInsetPx =
    kAircraftNoseLenPx + kAircraftTailHalfPx + 1;

constexpr int kBeyondRingDotRadiusPx = 2;
constexpr int kBeyondRingScreenMarginPx = 2;
constexpr int kAirportDotRadiusPx = 2;
/** Nearby airports outside the ring still get a rim dot (not a label). */
constexpr float kAirportHorizonScale = 2.5f;
constexpr size_t kMaxBeyondAirportDots = 8;

constexpr uint8_t kBgR = 4;
constexpr uint8_t kBgG = 10;
constexpr uint8_t kBgB = 28;
constexpr uint8_t kGridR = 16;
constexpr uint8_t kGridG = 100;
constexpr uint8_t kGridB = 32;
constexpr uint8_t kAircraftR = 255;
constexpr uint8_t kAircraftG = 0;
constexpr uint8_t kAircraftB = 0;
constexpr uint8_t kTrackR = 255;
constexpr uint8_t kTrackG = 0;
constexpr uint8_t kTrackB = 255;
constexpr uint8_t kTagTypeR = 255;
constexpr uint8_t kTagTypeG = 200;
constexpr uint8_t kTagTypeB = 0;
constexpr uint8_t kTagAltR = 90;
constexpr uint8_t kTagAltG = 200;
constexpr uint8_t kTagAltB = 255;
constexpr uint8_t kRunwayR = 56;
constexpr uint8_t kRunwayG = 150;
constexpr uint8_t kRunwayB = 170;
constexpr uint8_t kRunwayLabelR = 255;
constexpr uint8_t kRunwayLabelG = 220;
constexpr uint8_t kRunwayLabelB = 0;

extern uint16_t kColorBackground;
extern uint16_t kColorGrid;
extern uint16_t kColorLabel;
extern uint16_t kColorCenter;
extern uint16_t kColorAircraft;
extern uint16_t kColorTrackVector;
extern uint16_t kColorTagType;
extern uint16_t kColorTagAltitude;
extern uint16_t kColorRunway;
extern uint16_t kColorRunwayLabel;

}  // namespace ui::radar
