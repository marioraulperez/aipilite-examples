#pragma once

#include <cstdint>

namespace config {

constexpr char kFirmwareVersion[] = "v0.1.0";

// --- Wi-Fi portal ---
constexpr char kPortalApName[] = "AIPi-Radio";
constexpr char kPortalIp[] = "192.168.4.1";
constexpr char kPortalHostname[] = "aipi-radio";
constexpr char kPortalHostUrl[] = "aipi-radio.local";

constexpr unsigned long kWifiConnectAttemptMs = 15000;
constexpr uint8_t kWifiConnectAttempts = 3;
constexpr unsigned long kWifiPortalTimeoutSec = 0;
constexpr unsigned long kWifiConnectingFrameMs = 50;
constexpr unsigned long kWifiDownGraceMs = 4000;
constexpr unsigned long kWifiReconnectIntervalMs = 15000;
constexpr char kNtpServer[] = "pool.ntp.org";
/** POSIX TZ: ART is UTC-3 (offset hours west of UTC). */
constexpr char kLocalTimeZone[] = "ART3";

// --- Buttons ---
constexpr unsigned long kWifiResetHoldMs = 3000UL;
constexpr unsigned long kPowerHoldOffMs = 5000UL;
constexpr unsigned long kIdleOffMs = 180000UL;  // 3 minutes
constexpr unsigned int kClickMs = 400;          // room for double-click

// --- Display ---
constexpr int kDisplayWidth = 128;
constexpr int kDisplayHeight = 128;

// --- Audio ---
constexpr uint8_t kVolumeMin = 0;
constexpr uint8_t kVolumeMax = 21;
constexpr uint8_t kVolumeDefault = 3;
constexpr uint8_t kCodecVolume = 50;  // ES8311 0...100

// --- UI ---
constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorYellow = 0xFFE0;
constexpr uint16_t kTextOnYellow = kColorBlack;
constexpr uint16_t kTextOnBlack = 0xFFFF;
constexpr unsigned long kClockRefreshMs = 1000;
constexpr unsigned long kScrollMs = 80;

}  // namespace config
