CIRA SLIDER tiles on the 128×128 panel. One PNG at a time, scaled with PNGdec, then discarded.

```sh
# build & upload
pio run -e weather-sats -t upload
```

Hold **B** (GPIO42) at boot for the Wi-Fi AP `Sat-Config` (http://192.168.4.1). Short **B** next satellite, short **A** previous. Ignore a hold on **A** — that pin is power. Refresh every 10 minutes.
