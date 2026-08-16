> **WIP DRAFT — not a working radio yet.** Playback is unreliable: many
> stations fail to start (HTTP 401, redirects/JWT, timeouts), streams drop,
> and the UI/audio path is not stable. Treat this as an experiment, not
> something to ship or hand to kids. Station URLs in `stations.h` are
> candidates, not a verified playlist.

Kids internet radio on the AIPI-Lite 128×128 ST7735, using
[ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) 3.0.12 into the
onboard **ES8311** codec (I2S + MCLK). First-time Wi-Fi uses the WiFiManager
captive portal.

## Buttons

| Button | Action |
|--------|--------|
| **A** (left) | Tap: next country (plays its first station). Double-tap: volume down. Hold **5 s** to turn off. |
| **B** (right) | Station presets. The **first tap after boot is ignored**; later taps cycle stations in the current country (no-op if there is only one). Double-tap: volume up. Hold **3 s** to clear Wi-Fi and reopen the setup portal. |

`PWR_CTL` (GPIO 10) is driven HIGH at boot so the power latch stays on. The
device also powers off after **3 minutes** with no button activity *while nothing
is playing*. Listening keeps it awake.

## Screen

Clock, a small drawn flag, country, station name, scrolling ICY title, and a
volume bar.

## Wi-Fi setup

1. Join AP **`AIPi-Radio`**
2. Open **http://aipi-radio.local** or **http://192.168.4.1**
3. Set home Wi-Fi (optional POSIX timezone, default `ART3`)

After that, the same portal stays available on the LAN. Hold **B** for 3 seconds
anytime to wipe credentials.

## Stations

Hard-coded kids / family **stream URLs** (not TuneIn pages). Spain does not yet
have a verified kids mount; Chile (Pirinola) is the Spanish-language stand-in.
Radio UPA is listed under Argentina as in the v0.1 spec; the brand is Salvadoran.

Edit `stations.h` to add or replace URLs. Screen labels are shortened for 128×128;
full directory names are in the table below.

### How the list was built

1. **[Radio Browser](https://www.radio-browser.info/)** JSON API  
   `https://de1.api.radio-browser.info/json/stations/search?...`  
   Filters: `hidebroken=true`, prefer `lastcheckok=1`.  
   **Name on screen** comes from the API `name` field, truncated to fit.  
   **URL stored** is `url` (or a simpler sibling mount, e.g. MP3 instead of AAC+).
2. **v0.1 spec / public pages** for UPA and JunyTony (not in Radio Browser, or
   listed under a different country).
3. **Sanity GET** from the build machine (2026-08-16): prefer Icecast/Shoutcast
   that return `200` + `audio/mpeg` without a short-lived token.

**Avoid Zeno.FM on this firmware.** `stream.zeno.fm/...` 302s to
`*.surfernetwork.com/...&zt=<JWT>` that expires in ~60s. ESP32-audioI2S **3.0.12**
often surfaces that as **HTTP 401 Unauthorized** (Panda NG). Prefer Icecast.

### Station catalog

| Screen name | Full / directory name | Country | URL | Source |
|-------------|----------------------|---------|-----|--------|
| Radio UPA | Radio UPA — *La radio de los niños y las niñas* (ICY) | AR* | `http://streaming.mundoclasicaupa.com:8000/radioupa` | v0.1 spec; Icecast `icy-description`; site [radioupa.com.sv](http://radioupa.com.sv/). *Brand is El Salvador; directories often tag AR.* GET 200 `audio/mpeg`. |
| Radio Disney | Radio Disney 94.3 - Buenos Aires, Argentina | AR | `https://playerservices.streamtheworld.com/api/livestream-redirect/DISNEY_ARG_BA.mp3` | Radio Browser `name` + MP3 `url` (not the AAC+ `_ADP` mount). Family pop, not preschool. |
| Radio Infantil | Radio Infantil | MX | `http://s01.digitalserver.org:9268/stream` | Radio Browser `name`, countrycode MX, MP3 48 kbps, `lastcheckok=1`. Icecast-style (no Zeno JWT). |
| JunyTony | JunyTony - Canciones Infantiles | MX | `https://stream.zeno.fm/j78j6y7mii9vv` | v0.1 spec + [Zeno page](https://zeno.fm/radio/junytony-canciones-infantiles/); ICY `icy-name`. **Zeno JWT — may 401 on device.** |
| Radio Chiveo | Radio Chiveo | UY | `https://comodin.uy/listen/chiveo/128.mp3` | Radio Browser tag `infantil`, `name` Radio Chiveo. 128 kbps (not 320) for the ESP32. |
| Pirinola | Pirinola / Radio Pirinola (ICY) | CL | `http://curiosity.shoutca.st:6451/autodj` | Radio Browser tag `infantil`, `name` Pirinola. GET 200 Icecast. |
| Kids Music | Australian Digital Radio Network - Kids Music (MP3) | AU | `http://abm21.com.au:8000/CONTAINER21` | Radio Browser `name` (shortened). Icecast GET 200. |
| Classical Kids | YourClassical Children | US | `https://classicalkids.stream.publicradio.org/classicalkids.aac` | Radio Browser tag `children`, `name` YourClassical Children (MPR). GET 200 AAC. |
| Fun Kids | Fun Kids | GB | `https://listen-funkids.sharp-stream.com/funkids.mp3` | Radio Browser `name` Fun Kids ([funkidslive.com](https://www.funkidslive.com/)). GET 200. |
| Fun Kids Jr | Fun Kids Junior | GB | `http://listen-funkids.sharp-stream.com/funkidsjunior.mp3` | Radio Browser `name` Fun Kids Junior. GET 200. |
| Radio Teddy | Radio Teddy - Livestream | DE | `http://streamtdy.ir-media-tec.com/live/mp3-128/web/play.mp3` | Radio Browser tag `children`, `name` shortened. 302 → streamabc, then 200. |
| TOGGO Radio | TOGGO Radio | DE | `https://radio.toggo.de/live/mp3-192` | Radio Browser `name` TOGGO Radio. 302 → streamabc, then 200. |

Dropped: **Panda NG** (`https://stream.zeno.fm/61ux2pwbuuhvv`, Radio Browser Argentina / tag children) — 401 on device. Dropped AU Zeno “All Time Hits Radio Kids” for the same reason.


## Build pins

This example is pinned so it compiles with the **same Arduino-ESP32 2.x** as the other sketches in this repo.

| Pin | Why |
|-----|-----|
| **`espressif32@7.0.1`** | Arduino-ESP32 **2.0.17**, gcc 8.4. ESP32-audioI2S **3.4.7** needs Arduino-ESP32 **3.x** (C++20 `<span>`, `NetworkClient.h`, IDF 5 I2S). Switching this env to [pioarduino](https://github.com/pioarduino/platform-espressif32) failed here (Python 3.13 / `uv` install) and replaced the default `espressif32` package, so radio is pinned back to **7.0.1**. |
| **ESP32-audioI2S `#3.0.12`** | Last 3.0.x that still builds on Arduino 2.x (legacy I2S + `WiFiClientSecure`, weak callbacks like `audio_showstreamtitle`). 3.1+ dropped Arduino 2; 3.4.7 does not compile on this toolchain. |
| **`-std=gnu++17`** | Matches planeradar; 3.0.12 does not need C++20. |

To move to 3.4.7 later: Arduino-ESP32 3.x (pioarduino or equivalent), `-std=gnu++20`, and the 3.4 `Audio::msg_t` callback API.

```sh
# build & upload
pio run -e radio -t upload
```
