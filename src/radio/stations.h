#pragma once

#include <cstdint>

/** Curated kids / family streams. Direct stream URLs only (not TuneIn wrappers).
 *
 *  `name` is the short label drawn on the 128×128 screen.
 *  Full directory names, APIs, and why each URL was chosen: README.md
 *  "Station catalog".
 */
struct RadioStation {
  const char *name;
  const char *url;
};

struct RadioCountry {
  const char *code;
  const char *name;
  const RadioStation *stations;
  uint8_t station_count;
};

// Argentina
// UPA: Icecast mount from the v0.1 spec; ICY says "La Radio de los Ninos y las
// Ninas". Brand is Radio UPA (radioupa.com.sv, El Salvador); directories often
// file it under AR. GET 200 audio/mpeg (2026-08-16).
// Disney: Radio Browser `name` "Radio Disney 94.3 - Buenos Aires, Argentina";
// MP3 redirect (not AAC+) so ESP32-audioI2S 3.0.12 can decode it.
// Dropped "Panda NG" (RB/Zeno `https://stream.zeno.fm/61ux2pwbuuhvv`): Zeno 302s
// to surfernetwork.com with a ~60s JWT; the device returns HTTP 401.
inline const RadioStation kStationsAR[] = {
    {"Radio UPA", "http://streaming.mundoclasicaupa.com:8000/radioupa"},
    {"Radio Disney",
     "https://playerservices.streamtheworld.com/api/livestream-redirect/"
     "DISNEY_ARG_BA.mp3"},
};

// Mexico
// JunyTony: spec candidate + Zeno page "JunyTony - Canciones Infantiles"
// (icy-name matches). Same Zeno JWT redirect as Panda NG — may 401 on device.
// Radio Infantil: Radio Browser `name` "Radio Infantil", Icecast-style MP3,
// countrycode MX, lastcheckok=1 (Jan 2026). Prefer this first (no JWT).
inline const RadioStation kStationsMX[] = {
    {"Radio Infantil", "http://s01.digitalserver.org:9268/stream"},
    {"JunyTony", "https://stream.zeno.fm/j78j6y7mii9vv"},
};

// Uruguay — Radio Browser tag=infantil, `name` "Radio Chiveo", 128 kbps mount
// (not 320) for the ESP32. Homepage comodin.uy.
inline const RadioStation kStationsUY[] = {
    {"Radio Chiveo", "https://comodin.uy/listen/chiveo/128.mp3"},
};

// Chile — Radio Browser tag=infantil, `name` "Pirinola"; ICY description
// "Radio Pirinola". Icecast GET 200 (2026-08-16). Spanish-language stand-in
// until a verified Spain kids mount exists.
inline const RadioStation kStationsCL[] = {
    {"Pirinola", "http://curiosity.shoutca.st:6451/autodj"},
};

// Australia — Radio Browser countrycode=AU name=kids,
// `name` "Australian Digital Radio Network - Kids Music (MP3)". Icecast GET 200.
// Dropped Zeno "All Time Hits Radio Kids" (same JWT 401 risk as Panda NG).
inline const RadioStation kStationsAU[] = {
    {"Kids Music", "http://abm21.com.au:8000/CONTAINER21"},
};

// USA — Radio Browser tag=children, `name` "YourClassical Children"
// (YourClassical / Minnesota Public Radio). AAC GET 200 (2026-08-16).
inline const RadioStation kStationsUS[] = {
    {"Classical Kids",
     "https://classicalkids.stream.publicradio.org/classicalkids.aac"},
};

// UK — Radio Browser name="Fun Kids" / "Fun Kids Junior" (funkidslive.com).
inline const RadioStation kStationsGB[] = {
    {"Fun Kids", "https://listen-funkids.sharp-stream.com/funkids.mp3"},
    {"Fun Kids Jr", "http://listen-funkids.sharp-stream.com/funkidsjunior.mp3"},
};

// Germany — Radio Browser tag=children: `name` "Radio Teddy - Livestream" and
// "TOGGO Radio". Both 302 to streamabc then 200 (2026-08-16).
inline const RadioStation kStationsDE[] = {
    {"Radio Teddy", "http://streamtdy.ir-media-tec.com/live/mp3-128/web/play.mp3"},
    {"TOGGO Radio", "https://radio.toggo.de/live/mp3-192"},
};

inline const RadioCountry kCountries[] = {
    {"AR", "ARGENTINA", kStationsAR, 2},
    {"MX", "MEXICO", kStationsMX, 2},
    {"UY", "URUGUAY", kStationsUY, 1},
    {"CL", "CHILE", kStationsCL, 1},
    {"AU", "AUSTRALIA", kStationsAU, 1},
    {"US", "USA", kStationsUS, 1},
    {"GB", "UK", kStationsGB, 2},
    {"DE", "GERMANY", kStationsDE, 2},
};

inline constexpr uint8_t kCountryCount =
    static_cast<uint8_t>(sizeof(kCountries) / sizeof(kCountries[0]));
