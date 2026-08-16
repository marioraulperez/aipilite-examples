#pragma once

#include <cstdint>

namespace config {

constexpr char kFirmwareVersion[] = "v1.0.0-aipilite";

// --- Wi-Fi portal ---
constexpr char kPortalApName[] = "PlaneRadar-Setup";
constexpr char kPortalIp[] = "192.168.4.1";
/** mDNS host (no ".local" suffix); browser: http://plane-radar.local */
constexpr char kPortalHostname[] = "plane-radar";
constexpr char kPortalHostUrl[] = "plane-radar.local";

/** Per-attempt STA connect wait (ms); retried kWifiConnectAttempts times. */
constexpr unsigned long kWifiConnectAttemptMs = 15000;
constexpr uint8_t kWifiConnectAttempts = 3;
constexpr unsigned long kWifiPortalTimeoutSec = 0;  // 0 = no timeout while configuring
constexpr unsigned long kWifiConnectingFrameMs = 50;
/** Wait after disconnect before reconnecting (avoids portal on brief drops). */
constexpr unsigned long kWifiDownGraceMs = 4000;
/** Minimum interval between background reconnect tries. */
constexpr unsigned long kWifiReconnectIntervalMs = 15000;
/** Pacific Time, matching the default radar location; includes DST. */
constexpr char kLocalTimeZone[] = "PST8PDT,M3.2.0,M11.1.0";
constexpr char kNtpServer[] = "pool.ntp.org";

// --- Buttons (AipiLite: A = power / sound, B = range / Wi-Fi reset) ---
constexpr unsigned long kWifiResetHoldMs = 3000UL;
constexpr unsigned long kPowerHoldOffMs = 5000UL;
constexpr unsigned long kIdleOffMs = 180000UL;  // 3 minutes

// --- Display: ST7735 128×128 (AipiLite) ---
constexpr int kDisplayWidth = 128;
constexpr int kDisplayHeight = 128;

// --- Radar center defaults (overridden via WiFi setup portal) ---
// 33°04'37.9"N 117°14'07.5"W (Encinitas, CA)
constexpr double kDefaultRadarLat = 33.077194;
constexpr double kDefaultRadarLon = -117.235417;

/** Poll adsb.fi (API public limit: 1 req/s). */
constexpr unsigned long kAdsbFetchIntervalMs = 3000;
/** false = hide aircraft with alt_baro "ground"; true = show them too. */
constexpr bool kAdsbShowGroundAircraft = false;

// --- UI colors (RGB565) — status screens ---
constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorYellow = 0xFFE0;
constexpr uint16_t kTextOnYellow = kColorBlack;
constexpr uint16_t kTextOnBlack = 0xFFFF;

}  // namespace config
