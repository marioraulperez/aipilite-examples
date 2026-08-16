#include "ui/runway_overlay.h"

#include <cmath>
#include <cstdlib>

#include "data/large_airports.h"
#include "hardware/gfx_draw.h"
#include "services/radar_location.h"
#include "ui/radar_range.h"
#include "ui/radar_theme.h"

namespace ui::runway {
namespace {

constexpr float kKmPerDeg = 111.0f;
constexpr size_t kMaxAirportLabels = 16;

bool s_in_range[data::large_airports::kAirportCount];

struct BeyondAirport {
  uint16_t idx;
  float dist_km;
};

float e7ToDeg(int32_t e7) { return static_cast<float>(e7) * 1e-7f; }

void offsetKmFromCenter(float lat, float lon, float *dx_km, float *dy_km,
                        float *dist_km) {
  *dx_km = static_cast<float>(lon - services::location::lon()) * kKmPerDeg;
  *dy_km = static_cast<float>(lat - services::location::lat()) * kKmPerDeg;
  *dist_km = sqrtf((*dx_km) * (*dx_km) + (*dy_km) * (*dy_km));
}

void latLonToScreen(float lat, float lon, int *out_x, int *out_y) {
  const float outer_km = radar::rangeCurrent().outer_km;
  const float px_per_km =
      static_cast<float>(radar::kGridOuterRadius) / outer_km;

  float dx_km = 0.0f;
  float dy_km = 0.0f;
  float dist_km = 0.0f;
  offsetKmFromCenter(lat, lon, &dx_km, &dy_km, &dist_km);

  *out_x = radar::kCenterX + static_cast<int>(lroundf(dx_km * px_per_km));
  *out_y = radar::kCenterY - static_cast<int>(lroundf(dy_km * px_per_km));
}

int distSqFromCenter(int x, int y) {
  const int dx = x - radar::kCenterX;
  const int dy = y - radar::kCenterY;
  return dx * dx + dy * dy;
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

bool segmentIntersectsDisc(int x0, int y0, int x1, int y1) {
  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int r = radar::kGridOuterRadius;
  const int r_sq = r * r;

  if (distSqFromCenter(x0, y0) <= r_sq || distSqFromCenter(x1, y1) <= r_sq) {
    return true;
  }

  const int dx = x1 - x0;
  const int dy = y1 - y0;
  const int fx = x0 - cx;
  const int fy = y0 - cy;
  const int a = dx * dx + dy * dy;
  if (a == 0) {
    return false;
  }
  const int b = 2 * (fx * dx + fy * dy);
  const int c = fx * fx + fy * fy - r_sq;
  int disc = b * b - 4 * a * c;
  if (disc < 0) {
    return false;
  }
  disc = static_cast<int>(sqrtf(static_cast<float>(disc)));
  const float inv2a = 1.0f / (2.0f * static_cast<float>(a));
  const float t0 = (-static_cast<float>(b) - disc) * inv2a;
  const float t1 = (-static_cast<float>(b) + disc) * inv2a;
  return (t0 >= 0.0f && t0 <= 1.0f) || (t1 >= 0.0f && t1 <= 1.0f);
}

void drawAirportLabel(Arduino_GFX &gfx, const char *ident, int mx, int my) {
  gfxDrawText(gfx, ident, mx, my, TextAnchor::BottomCenter, radar::kColorRunwayLabel,
              radar::kColorBackground);
}

bool drawRunwayLine(Arduino_GFX &gfx, const data::large_airports::Runway &rw) {
  const float le_lat = e7ToDeg(rw.le_lat_e7);
  const float le_lon = e7ToDeg(rw.le_lon_e7);
  const float he_lat = e7ToDeg(rw.he_lat_e7);
  const float he_lon = e7ToDeg(rw.he_lon_e7);

  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;
  latLonToScreen(le_lat, le_lon, &x0, &y0);
  latLonToScreen(he_lat, he_lon, &x1, &y1);

  if (!segmentIntersectsDisc(x0, y0, x1, y1)) {
    return false;
  }

  clipPointToOuterRing(x0, y0, &x1, &y1);
  clipPointToOuterRing(x1, y1, &x0, &y0);

  gfxDrawWideLine(gfx, x0, y0, x1, y1, radar::kRunwayLineHalfWidth,
                  radar::kColorRunway);
  return true;
}

void offsetLabelFromCenter(int ax, int ay, int *lx, int *ly) {
  const int dx = ax - radar::kCenterX;
  const int dy = ay - radar::kCenterY;
  const float len = sqrtf(static_cast<float>(dx * dx + dy * dy));
  const int gap = radar::kRunwayLabelGapPx;
  if (len < 1.0f) {
    *lx = ax;
    *ly = ay - gap;
    return;
  }
  *lx = ax + static_cast<int>(lroundf(dx / len * static_cast<float>(gap)));
  *ly = ay + static_cast<int>(lroundf(dy / len * static_cast<float>(gap)));
}

void clipPointOntoOuterRing(int *x, int *y) {
  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int r = radar::kGridOuterRadius;
  const int dx = *x - cx;
  const int dy = *y - cy;
  const int d_sq = dx * dx + dy * dy;
  const int r_sq = r * r;
  if (d_sq <= r_sq || d_sq == 0) {
    return;
  }
  const float scale = static_cast<float>(r) / sqrtf(static_cast<float>(d_sq));
  *x = cx + static_cast<int>(lroundf(static_cast<float>(dx) * scale));
  *y = cy + static_cast<int>(lroundf(static_cast<float>(dy) * scale));
}

void considerBeyondAirport(BeyondAirport *beyond, size_t *count, uint16_t idx,
                           float dist_km) {
  if (*count < radar::kMaxBeyondAirportDots) {
    beyond[*count].idx = idx;
    beyond[*count].dist_km = dist_km;
    ++(*count);
    return;
  }

  size_t farthest = 0;
  for (size_t i = 1; i < *count; ++i) {
    if (beyond[i].dist_km > beyond[farthest].dist_km) {
      farthest = i;
    }
  }
  if (dist_km < beyond[farthest].dist_km) {
    beyond[farthest].idx = idx;
    beyond[farthest].dist_km = dist_km;
  }
}

void drawBeyondAirportDot(Arduino_GFX &gfx, float lat, float lon) {
  float dx_km = 0.0f;
  float dy_km = 0.0f;
  float dist_km = 0.0f;
  offsetKmFromCenter(lat, lon, &dx_km, &dy_km, &dist_km);
  if (dist_km < 0.01f) {
    return;
  }

  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int rim_r = radar::kCenterX - radar::kBeyondRingScreenMarginPx;
  const float angle_rad = atan2f(dx_km, dy_km);
  const int x = cx + static_cast<int>(lroundf(sinf(angle_rad) * rim_r));
  const int y = cy - static_cast<int>(lroundf(cosf(angle_rad) * rim_r));
  gfx.fillCircle(x, y, radar::kAirportDotRadiusPx, radar::kColorRunwayLabel);
}

void drawAirportLabelAt(Arduino_GFX &gfx, const data::large_airports::Airport &ap) {
  int ax = 0;
  int ay = 0;
  latLonToScreen(e7ToDeg(ap.lat_e7), e7ToDeg(ap.lon_e7), &ax, &ay);
  clipPointOntoOuterRing(&ax, &ay);
  gfx.fillCircle(ax, ay, radar::kAirportDotRadiusPx, radar::kColorRunwayLabel);

  int lx = 0;
  int ly = 0;
  offsetLabelFromCenter(ax, ay, &lx, &ly);
  drawAirportLabel(gfx, ap.ident, lx, ly);
}

}  // namespace

void drawLargeAirportRunways(Arduino_GFX &gfx) {
  if (!radar::showRunways()) {
    return;
  }
  const float outer_km = radar::rangeCurrent().outer_km;
  const float in_range_km = radar::fetchRadiusKm();
  const float horizon_km = outer_km * radar::kAirportHorizonScale;

  uint16_t label_airports[kMaxAirportLabels];
  size_t label_count = 0;
  BeyondAirport beyond[radar::kMaxBeyondAirportDots];
  size_t beyond_count = 0;

  for (size_t i = 0; i < data::large_airports::kAirportCount; ++i) {
    s_in_range[i] = false;

    const auto &ap = data::large_airports::kAirports[i];
    float dx_km = 0.0f;
    float dy_km = 0.0f;
    float dist_km = 0.0f;
    offsetKmFromCenter(e7ToDeg(ap.lat_e7), e7ToDeg(ap.lon_e7), &dx_km, &dy_km,
                       &dist_km);

    if (dist_km <= in_range_km) {
      s_in_range[i] = true;
      if (label_count < kMaxAirportLabels) {
        label_airports[label_count++] = static_cast<uint16_t>(i);
      }
    } else if (dist_km <= horizon_km) {
      considerBeyondAirport(beyond, &beyond_count, static_cast<uint16_t>(i),
                            dist_km);
    }
  }

  for (size_t i = 0; i < data::large_airports::kRunwayCount; ++i) {
    const auto &rw = data::large_airports::kRunways[i];
    if (!s_in_range[rw.airport_idx]) {
      continue;
    }
    drawRunwayLine(gfx, rw);
  }

  for (size_t i = 0; i < label_count; ++i) {
    drawAirportLabelAt(gfx, data::large_airports::kAirports[label_airports[i]]);
  }
  for (size_t i = 0; i < beyond_count; ++i) {
    const auto &ap = data::large_airports::kAirports[beyond[i].idx];
    drawBeyondAirportDot(gfx, e7ToDeg(ap.lat_e7), e7ToDeg(ap.lon_e7));
  }
}

}  // namespace ui::runway
