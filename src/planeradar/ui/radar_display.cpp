#include "ui/radar_display.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

#include "hardware/beep.h"
#include "hardware/display.h"
#include "hardware/gfx_draw.h"
#include "services/adsb_client.h"
#include "services/radar_location.h"
#include "ui/radar_animation_policy.h"
#include "ui/radar_range.h"
#include "ui/radar_theme.h"
#include "ui/runway_overlay.h"

namespace ui {
namespace radar {

uint16_t kColorBackground = 0x0000;
uint16_t kColorGrid = 0x0320;
uint16_t kColorLabel = 0xFFFF;
uint16_t kColorCenter = 0xFFFF;
uint16_t kColorAircraft = 0xF800;
uint16_t kColorTrackVector = 0xF81F;
uint16_t kColorTagType = 0xFFE0;
uint16_t kColorTagAltitude = 0x5DFF;
uint16_t kColorRunway = 0x4D5F;
uint16_t kColorRunwayLabel = 0x7DFF;

}  // namespace radar

namespace {

uint16_t s_sweep_bright = 0;
uint16_t s_sweep_mid = 0;
uint16_t s_sweep_dim = 0;
bool s_palette_ready = false;
float s_prev_sweep_angle = -1.0f;
uint8_t s_sweep_hits = 0;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return gfx->color565(r, g, b);
}

void initPalette() {
  if (s_palette_ready) {
    return;
  }
  radar::kColorBackground = rgb565(radar::kBgR, radar::kBgG, radar::kBgB);
  radar::kColorGrid = rgb565(radar::kGridR, radar::kGridG, radar::kGridB);
  radar::kColorLabel = rgb565(255, 255, 255);
  radar::kColorCenter = rgb565(255, 255, 255);
  radar::kColorAircraft = rgb565(radar::kAircraftR, radar::kAircraftG, radar::kAircraftB);
  radar::kColorTrackVector = rgb565(radar::kTrackR, radar::kTrackG, radar::kTrackB);
  radar::kColorTagType = rgb565(radar::kTagTypeR, radar::kTagTypeG, radar::kTagTypeB);
  radar::kColorTagAltitude = rgb565(radar::kTagAltR, radar::kTagAltG, radar::kTagAltB);
  radar::kColorRunway = rgb565(radar::kRunwayR, radar::kRunwayG, radar::kRunwayB);
  radar::kColorRunwayLabel =
      rgb565(radar::kRunwayLabelR, radar::kRunwayLabelG, radar::kRunwayLabelB);
  s_sweep_bright = rgb565(50, 220, 105);
  s_sweep_mid = rgb565(25, 120, 60);
  s_sweep_dim = rgb565(12, 65, 34);
  s_palette_ready = true;
}

constexpr float kKmPerDeg = 111.0f;

void offsetKmFromCenter(float lat, float lon, float *dx_km, float *dy_km,
                        float *dist_km) {
  *dx_km = static_cast<float>(lon - services::location::lon()) * kKmPerDeg;
  *dy_km = static_cast<float>(lat - services::location::lat()) * kKmPerDeg;
  *dist_km = sqrtf((*dx_km) * (*dx_km) + (*dy_km) * (*dy_km));
}

float innerRingMaxKm() {
  const float outer_km = radar::rangeCurrent().outer_km;
  return outer_km * (static_cast<float>(radar::kGridOuterRadius -
                                       radar::kAircraftInsideRingInsetPx) /
                     static_cast<float>(radar::kGridOuterRadius));
}

void latLonToScreen(float lat, float lon, int *out_x, int *out_y) {
  const float outer_km = radar::rangeCurrent().outer_km;
  const float px_per_km = static_cast<float>(radar::kGridOuterRadius) / outer_km;

  float dx_km = 0.0f;
  float dy_km = 0.0f;
  float dist_km = 0.0f;
  offsetKmFromCenter(lat, lon, &dx_km, &dy_km, &dist_km);

  *out_x = radar::kCenterX + static_cast<int>(lroundf(dx_km * px_per_km));
  *out_y = radar::kCenterY - static_cast<int>(lroundf(dy_km * px_per_km));
}

bool isInsideOuterRingKm(float dist_km) { return dist_km <= innerRingMaxKm(); }

int distSqFromCenter(int x, int y) {
  const int dx = x - radar::kCenterX;
  const int dy = y - radar::kCenterY;
  return dx * dx + dy * dy;
}

bool beyondRingEdgeDotFromLatLon(float lat, float lon, int *out_x, int *out_y) {
  float dx_km = 0.0f;
  float dy_km = 0.0f;
  float dist_km = 0.0f;
  offsetKmFromCenter(lat, lon, &dx_km, &dy_km, &dist_km);
  if (dist_km < 0.01f || isInsideOuterRingKm(dist_km)) {
    return false;
  }

  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int rim_r = radar::kCenterX - radar::kBeyondRingScreenMarginPx;
  const float angle_rad = atan2f(dx_km, dy_km);

  *out_x = cx + static_cast<int>(lroundf(sinf(angle_rad) * rim_r));
  *out_y = cy - static_cast<int>(lroundf(cosf(angle_rad) * rim_r));
  return true;
}

void clipPointToOuterRing(int x0, int y0, int *x1, int *y1) {
  const int max_r = radar::kGridOuterRadius;
  const int max_r_sq = max_r * max_r;
  if (distSqFromCenter(*x1, *y1) <= max_r_sq) {
    return;
  }

  const int dx = *x1 - x0;
  const int dy = *y1 - y0;
  float t = 1.0f;
  for (int step = 0; step < 20; ++step) {
    const int px = x0 + static_cast<int>(lroundf(dx * t));
    const int py = y0 + static_cast<int>(lroundf(dy * t));
    if (distSqFromCenter(px, py) <= max_r_sq) {
      *x1 = px;
      *y1 = py;
      return;
    }
    t -= 0.05f;
    if (t <= 0.0f) {
      *x1 = x0;
      *y1 = y0;
      return;
    }
  }
}

int speedLineLengthPx(float gs_knots) {
  if (gs_knots <= 0.0f) {
    return 0;
  }
  constexpr float kKmPerKnotPerHorizon =
      1.852f * radar::kAircraftTrackHorizonSec / 3600.0f;
  const float px = gs_knots * kKmPerKnotPerHorizon * radar::kGridOuterRadius /
                   radar::kAircraftTrackRefOuterKm * radar::kAircraftTrackLengthScale;
  const int len = static_cast<int>(px + 0.5f);
  return len < radar::kAircraftSpeedLineMinPx ? radar::kAircraftSpeedLineMinPx : len;
}

void noseTip(int cx, int cy, float heading_deg, int *tip_x, int *tip_y) {
  constexpr float kDegToRad = 0.01745329252f;
  const float rad = heading_deg * kDegToRad;
  *tip_x = cx + static_cast<int>(lroundf(sinf(rad) * radar::kAircraftNoseLenPx));
  *tip_y = cy - static_cast<int>(lroundf(cosf(rad) * radar::kAircraftNoseLenPx));
}

void drawHeadingTriangle(int cx, int cy, float heading_deg, uint16_t color) {
  constexpr float kDegToRad = 0.01745329252f;
  const float rad = heading_deg * kDegToRad;
  const float sin_h = sinf(rad);
  const float cos_h = cosf(rad);

  int tip_x = 0;
  int tip_y = 0;
  noseTip(cx, cy, heading_deg, &tip_x, &tip_y);

  const int base_x =
      cx - static_cast<int>(lroundf(sin_h * static_cast<float>(radar::kAircraftTailLenPx)));
  const int base_y =
      cy + static_cast<int>(lroundf(cos_h * static_cast<float>(radar::kAircraftTailLenPx)));
  const int wing_x = static_cast<int>(lroundf(cos_h * radar::kAircraftTailHalfPx));
  const int wing_y = static_cast<int>(lroundf(sin_h * radar::kAircraftTailHalfPx));

  gfx->fillTriangle(tip_x, tip_y, base_x + wing_x, base_y + wing_y,
                    base_x - wing_x, base_y - wing_y, color);
}

void drawSpeedVector(int cx, int cy, float heading_deg, float track_deg,
                     float gs_knots, uint16_t color) {
  const int len = speedLineLengthPx(gs_knots);
  if (len <= 0) {
    return;
  }

  int tip_x = 0;
  int tip_y = 0;
  noseTip(cx, cy, heading_deg, &tip_x, &tip_y);

  constexpr float kDegToRad = 0.01745329252f;
  const float rad = track_deg * kDegToRad;
  int ex = tip_x + static_cast<int>(lroundf(sinf(rad) * len));
  int ey = tip_y - static_cast<int>(lroundf(cosf(rad) * len));
  clipPointToOuterRing(tip_x, tip_y, &ex, &ey);
  if (ex == tip_x && ey == tip_y) {
    return;
  }
  gfxDrawWideLine(*gfx, tip_x, tip_y, ex, ey, radar::kAircraftTrackLineHalfWidth,
                  color);
}

struct AircraftDrawItem {
  size_t index = 0;
  int x = 0;
  int y = 0;
  int dist_sq = 0;
};

void sortDrawItemsFarFirst(AircraftDrawItem *items, size_t count) {
  for (size_t i = 1; i < count; ++i) {
    const AircraftDrawItem key = items[i];
    size_t j = i;
    while (j > 0 && items[j - 1].dist_sq < key.dist_sq) {
      items[j] = items[j - 1];
      --j;
    }
    items[j] = key;
  }
}

void drawAircraftTag(int x, int y, const services::adsb::Aircraft &plane) {
  const char *label = plane.callsign[0] != '\0' ? plane.callsign : plane.id;
  if (label[0] == '\0') {
    return;
  }
  const int w = gfxTextWidth(*gfx, label);
  const int h = gfxTextHeight(*gfx);
  const int gap = radar::kAircraftNoseLenPx + radar::kAircraftLabelGapPx;
  int left = (x < radar::kCenterX) ? (x + gap) : (x - gap - w);
  int top = y - h / 2;
  if (left < 0) {
    left = 0;
  }
  if (left + w > radar::kSize) {
    left = radar::kSize - w;
  }
  if (top < 0) {
    top = 0;
  }
  if (top + h > radar::kSize) {
    top = radar::kSize - h;
  }
  gfxDrawText(*gfx, label, left, top, TextAnchor::TopLeft, radar::kColorLabel,
              radar::kColorBackground);
}

void drawAircraft(unsigned long now_ms) {
  size_t n = 0;
  const services::adsb::Aircraft *planes =
      services::adsb::aircraftList(now_ms, &n);

  AircraftDrawItem items[services::adsb::kMaxAircraft];
  size_t draw_count = 0;

  for (size_t i = 0; i < n; ++i) {
    float dx_km = 0.0f;
    float dy_km = 0.0f;
    float dist_km = 0.0f;
    offsetKmFromCenter(planes[i].lat, planes[i].lon, &dx_km, &dy_km, &dist_km);

    if (isInsideOuterRingKm(dist_km)) {
      int x = 0;
      int y = 0;
      latLonToScreen(planes[i].lat, planes[i].lon, &x, &y);
      items[draw_count].index = i;
      items[draw_count].x = x;
      items[draw_count].y = y;
      items[draw_count].dist_sq = distSqFromCenter(x, y);
      ++draw_count;
      continue;
    }

    int dot_x = 0;
    int dot_y = 0;
    if (beyondRingEdgeDotFromLatLon(planes[i].lat, planes[i].lon, &dot_x,
                                    &dot_y)) {
      gfx->fillCircle(dot_x, dot_y, radar::kBeyondRingDotRadiusPx,
                      radar::kColorAircraft);
    }
  }

  sortDrawItemsFarFirst(items, draw_count);
  for (size_t d = 0; d < draw_count; ++d) {
    const size_t i = items[d].index;
    drawSpeedVector(items[d].x, items[d].y, planes[i].nose_deg, planes[i].track_deg,
                    planes[i].gs_knots, radar::kColorTrackVector);
    drawHeadingTriangle(items[d].x, items[d].y, planes[i].nose_deg,
                        radar::kColorAircraft);
  }

  gfx->setTextSize(1);
  const size_t tag_limit = draw_count > 6 ? 6 : draw_count;
  for (size_t d = 0; d < tag_limit; ++d) {
    const AircraftDrawItem &item = items[d];
    drawAircraftTag(item.x, item.y, planes[item.index]);
  }
}

void drawGridRing(int cx, int cy, int r, uint16_t color) {
  if (r <= 0) {
    return;
  }
  gfx->drawCircle(cx, cy, r, color);
}

void drawStaticGrid() {
  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int grid_r = radar::kGridOuterRadius;

  gfx->fillScreen(radar::kColorBackground);
  for (int i = 1; i <= radar::kRingCount; ++i) {
    drawGridRing(cx, cy, (grid_r * i) / radar::kRingCount, radar::kColorGrid);
  }
  gfxDrawWideLine(*gfx, cx, cy - grid_r, cx, cy + grid_r,
                  radar::kGridStrokeHalfWidth, radar::kColorGrid);
  gfxDrawWideLine(*gfx, cx - grid_r, cy, cx + grid_r, cy,
                  radar::kGridStrokeHalfWidth, radar::kColorGrid);

  runway::drawLargeAirportRunways(*gfx);

  gfx->fillCircle(cx, cy, radar::kCenterDotRadius, radar::kColorCenter);

  gfx->setTextSize(1);
  gfxDrawText(*gfx, "N", cx, radar::kCardinalNorthOffsetY, TextAnchor::TopCenter,
              radar::kColorLabel, radar::kColorBackground);
  gfxDrawText(*gfx, "S", cx, radar::kSize - 1 + radar::kCardinalSouthOffsetY,
              TextAnchor::BottomCenter, radar::kColorLabel, radar::kColorBackground);
  gfxDrawText(*gfx, "W", 1, cy, TextAnchor::MiddleLeft, radar::kColorLabel,
              radar::kColorBackground);
  gfxDrawText(*gfx, "E", radar::kSize - 2, cy, TextAnchor::MiddleRight,
              radar::kColorLabel, radar::kColorBackground);

  char scale_label[12];
  radar::formatCurrentRing3Label(scale_label, sizeof(scale_label));
  gfxDrawText(*gfx, scale_label, cx + grid_r - radar::kScaleGapFromOuterRing, cy,
              TextAnchor::MiddleRight, radar::kColorGrid, radar::kColorBackground);
}

void drawSpeakerIcon() {
  if (!beepEnabled()) {
    return;
  }

  // 6x6: 2px body + cone pointing right.
  static constexpr uint8_t kBits[] = {0x30, 0x38, 0x3F, 0x3F, 0x38, 0x30};
  const int x0 = radar::kSpeakerIconX;
  const int y0 = radar::kSpeakerIconY;
  gfx->fillRect(x0, y0, radar::kSpeakerIconSize, radar::kSpeakerIconSize,
                radar::kColorBackground);
  for (int y = 0; y < radar::kSpeakerIconSize; ++y) {
    for (int x = 0; x < radar::kSpeakerIconSize; ++x) {
      if ((kBits[y] & (0x20 >> x)) != 0) {
        gfx->drawPixel(x0 + x, y0 + y, radar::kColorLabel);
      }
    }
  }
}

void drawSweep(unsigned long now_ms) {
  if (!radar::showSweep()) {
    return;
  }
  constexpr float kTwoPi = 6.28318530718f;
  constexpr float kTrailStepRad = 0.06981317008f;
  const float angle = static_cast<float>(radar::sweepPhaseMs(now_ms)) /
                      static_cast<float>(radar::kSweepPeriodMs) * kTwoPi;
  const uint16_t colors[] = {s_sweep_bright, s_sweep_mid, s_sweep_dim};

  for (size_t i = 0; i < 3; ++i) {
    const float ray = angle - static_cast<float>(i) * kTrailStepRad;
    const int x = radar::kCenterX +
                  static_cast<int>(lroundf(sinf(ray) * radar::kGridOuterRadius));
    const int y = radar::kCenterY -
                  static_cast<int>(lroundf(cosf(ray) * radar::kGridOuterRadius));
    gfx->drawLine(radar::kCenterX, radar::kCenterY, x, y, colors[i]);
  }
}

bool sweepCrossedBearing(float from_angle, float to_angle, float bearing) {
  if (to_angle >= from_angle) {
    return bearing > from_angle && bearing <= to_angle;
  }
  return bearing > from_angle || bearing <= to_angle;
}

void collectSweepHits(unsigned long now_ms) {
  if (!radar::showSweep()) {
    s_prev_sweep_angle = -1.0f;
    return;
  }

  constexpr float kTwoPi = 6.28318530718f;
  const float angle = static_cast<float>(radar::sweepPhaseMs(now_ms)) /
                      static_cast<float>(radar::kSweepPeriodMs) * kTwoPi;
  if (s_prev_sweep_angle < 0.0f) {
    s_prev_sweep_angle = angle;
    return;
  }

  size_t n = 0;
  const services::adsb::Aircraft *planes =
      services::adsb::aircraftList(now_ms, &n);
  for (size_t i = 0; i < n; ++i) {
    float dx_km = 0.0f;
    float dy_km = 0.0f;
    float dist_km = 0.0f;
    offsetKmFromCenter(planes[i].lat, planes[i].lon, &dx_km, &dy_km, &dist_km);
    if (!isInsideOuterRingKm(dist_km)) {
      continue;
    }
    float bearing = atan2f(dx_km, dy_km);
    if (bearing < 0.0f) {
      bearing += kTwoPi;
    }
    if (sweepCrossedBearing(s_prev_sweep_angle, angle, bearing)) {
      if (s_sweep_hits < 8) {
        ++s_sweep_hits;
      }
    }
  }
  s_prev_sweep_angle = angle;
}

void renderFrame(unsigned long now_ms) {
  initPalette();
  gfx->setTextSize(1);
  drawStaticGrid();
  drawSweep(now_ms);
  drawAircraft(now_ms);
  drawSpeakerIcon();
  collectSweepHits(now_ms);
  displayFlush();
}

}  // namespace

void radarDisplayDraw() { renderFrame(millis()); }

void radarDisplayRefreshAircraft() { renderFrame(millis()); }

uint8_t radarDisplayTakeSweepHits() {
  const uint8_t hits = s_sweep_hits;
  s_sweep_hits = 0;
  return hits;
}

}  // namespace ui
