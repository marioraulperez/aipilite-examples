Live ADS-B plane radar on the AIPI-Lite 128×128 ST7735, ported from
[ESP32-Plane-Radar](https://github.com/RockBase-iot/ESP32-Plane-Radar).

Aircraft around your location are drawn as a sonar-style grid. First-time Wi-Fi
and location setup use the built-in WiFiManager captive portal.

## Buttons

| Button | Action |
|--------|--------|
| **A** (left) | Tap: sound on/off (ping when the sweep hits an **in-range** aircraft). Hold **5 s** to turn off. |
| **B** (right) | Range presets. The **first tap after boot is ignored**; later taps cycle **5 → 10 → 15 → 25 km**. Hold **3 s** to clear Wi-Fi and reopen the setup portal. |

The device also powers off after **3 minutes** with no button activity.

`PWR_CTL` (GPIO 10) is driven HIGH at boot so the power latch stays on.

## Wi-Fi setup

1. Join AP **`PlaneRadar-Setup`**
2. Open **http://plane-radar.local** or **http://192.168.4.1**
3. Set home Wi-Fi, radar lat/lon (or pick a city), units, runways, sweep

After that, the same portal stays available on the LAN at `http://plane-radar.local`
or the device IP. Hold **B** for 3 seconds anytime to wipe credentials.

ADS-B data comes from [adsb.fi](https://opendata.adsb.fi/).

```sh
# build & upload
pio run -e planeradar -t upload
```
