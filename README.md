These are various exmaples for AIPI-Lite AI Robot. If you want micropython on it, check out [this](https://github.com/bcarroll/aipi-lite).

## hardware

You can read more [here](https://www.robertlipe.com/449-2/) and I [documented](HARDWARE.md) it


## purchasing

- [Amazon](https://www.amazon.com/dp/B0FQNK543G)
- [AIPI](https://aipi.com/products/aipi-lite)


## dump

You might want to keep the original firmware:

```sh
# install tools
pip install esptool

# rip firmware
esptool -b 460800 read-flash 0 0x400000 aipi_lite_backup.bin

# erase
esptool erase_flash

# write firmware
esptool -b 460800 write-flash 0 aipi_lite_backup.bin
```

## bootloader mode

This is sometimes needed to program it, and it's a bit tricky. The screen will stay black, when it's working.

- Sometimes you can get it into this mode by just unplugging it, and quickly plugging and programming it (`pio run -t upload -e display`)
- If not, remove 4 screws on the back of the device, then press the button under the display while plugging the device into a USB port.


## using this repo

All the demos are setup in [platformio.ini](./platformio.ini).

```sh
# install platformio
pip install platformio

# compile & upload basic demo
pip run -t upload -e basic
```

## toolchain versions (last verified working)

- Python: 3.13.14
- PlatformIO Core: 6.1.19
- esptool: 5.3.0 (must be in PATH — see comment in `platformio.ini`)
- platform (espressif32 / pioarduino): 55.3.311
  - framework-arduinoespressif32: 3.3.11
  - framework-arduinoespressif32-libs: 5.5.5+sha.b774170ff46
  - toolchain-xtensa-esp-elf: 14.2.0+20260121

## windows: fixing "long paths" build errors

On Windows, installing PlatformIO packages (e.g. `framework-arduinoespressif32-libs`, which ships deeply nested Matter/connectedhomeip paths well over 200 characters) can fail with a `FileNotFoundError` while unpacking, unless Windows' long path support is enabled.

Fix (requires admin — this will prompt a UAC dialog):

```powershell
Start-Process powershell -Verb RunAs -Wait -ArgumentList '-NoProfile -Command "New-ItemProperty -Path ''HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem'' -Name LongPathsEnabled -Value 1 -PropertyType DWord -Force | Out-Null"'
```

Verify it took effect:

```powershell
reg query "HKLM\SYSTEM\CurrentControlSet\Control\FileSystem" /v LongPathsEnabled
```

It should print `LongPathsEnabled    REG_DWORD    0x1`. No reboot needed — just re-run the `pio run` command.
