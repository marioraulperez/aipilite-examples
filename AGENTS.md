# AGENTS.md

Notas para agentes/desarrolladores sobre cómo compilar y flashear el firmware de este repo (PlatformIO + ESP32-S3), y qué hacer si el entorno en Windows está roto.

## Requisitos previos (Windows)

- PlatformIO Core instalado (`pio --version`).
- `esptool` 5.x accesible en el PATH del sistema (`esptool version` debe mostrar `5.x`). El `upload_protocol = custom` de `platformio.ini` invoca `esptool` directamente porque el esptool 4.x que trae la plataforma no maneja bien el USB-JTAG del ESP32-S3.
- Soporte de **rutas largas de Windows habilitado** (`LongPathsEnabled = 1` en `HKLM\SYSTEM\CurrentControlSet\Control\FileSystem`). Sin esto, la instalación de paquetes de PlatformIO (p. ej. `framework-arduinoespressif32-libs`, que incluye árboles de Matter/connectedhomeip con rutas de +200 caracteres) falla con `FileNotFoundError` al descomprimir. Requiere permisos de administrador (UAC):

  ```powershell
  Start-Process powershell -Verb RunAs -Wait -ArgumentList '-NoProfile -Command "New-ItemProperty -Path ''HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem'' -Name LongPathsEnabled -Value 1 -PropertyType DWord -Force | Out-Null"'
  ```

## Compilar

```powershell
pio run -e planeradar
```

Otros entornos disponibles: `basic`, `sound`, `rgb`, `battery`, `pocketjoke`, `roboeyes`, `radio` (ver `platformio.ini`).

## Flashear (upload)

1. Conectar el ESP32-S3 por USB y verificar el puerto:

   ```powershell
   pio device list
   ```

2. Flashear (ajustar el puerto COM según corresponda):

   ```powershell
   pio run -e planeradar -t upload --upload-port COM5
   ```

   El `upload_command` (en `platformio.ini`) escribe `bootloader.bin` en `0x0`, `partitions.bin` en `0x8000` y el firmware en `0x10000` usando `esptool write-flash` directamente.

3. (Opcional) Monitor serie para ver el log de arranque:

   ```powershell
   pio device monitor -p COM5 -b 115200
   ```

## Problemas de entorno ya resueltos (referencia)

- **Rutas largas de Windows deshabilitadas** → build fallaba al instalar `framework-arduinoespressif32-libs` (tarball con paths de Matter/connectedhomeip >200 caracteres). Fix: habilitar `LongPathsEnabled` (ver arriba).
- **`GFX Library for Arduino` (Arduino_GFX) pinneada en `v1.5.9`** era incompatible con el core `arduino-esp32` 3.3.11 (la firma de `spiFrequencyToClockDiv` cambió a `spiFrequencyToClockDiv(spi_t*, uint32_t)`). Fix aplicado: bump del pin a `v1.6.7` en todos los entornos que usan Arduino_GFX (`basic`, `sound`, `battery`, `pocketjoke`, `roboeyes`, `planeradar`) en `platformio.ini`. El entorno `radio` no se tocó porque fija su propia versión de plataforma (`espressif32@7.0.1`), previa al cambio incompatible.
- **`src/planeradar/hardware/beep.cpp`** y **`src/sound/main.cpp`**: usaban `cfg.fixed_mclk`, campo que no existe en `arduino-audio-tools` `v1.2.4` (la versión pinneada). Se eliminó esa línea en ambos archivos.
- **`src/planeradar/services/adsb_client.cpp`**: usaba el tipo `WiFiClient` pero solo incluía `WiFiClientSecure.h`. En el core actual, `WiFiClientSecure.h` es solo un alias (`typedef NetworkClientSecure WiFiClientSecure;`) y ya no trae `WiFiClient` transitivamente. Se agregó `#include <WiFiClient.h>`.
- **`[env:radio]`** tenía su propia plataforma fijada (`platform = espressif32@7.0.1`) y `Arduino_GFX#v1.5.9`, quedando desalineado del resto. Se quitó el pin de plataforma (hereda la del `[env]` base) y se subió `Arduino_GFX` a `v1.6.7` igual que los demás entornos. El código de `radio` no necesitó cambios, pero al mezclar metadata de la plataforma vieja con la nueva quedó un estado de paquetes inconsistente que rompía el build (`TypeError` en `piobuild.py` por `FRAMEWORK_DIR` en `None`); se resolvió limpiando el build cacheado del entorno:

  ```powershell
  Remove-Item -Recurse -Force ".pio\build\radio"
  pio run -e radio
  ```

## Estado verificado

- Build y flasheo de `planeradar` confirmados funcionando end-to-end (compilación exitosa + `esptool write-flash` con verificación de hash OK) sobre un ESP32-S3 conectado por USB.
- Todos los entornos de `platformio.ini` compilan correctamente y ahora comparten la misma plataforma y versión de `Arduino_GFX`: `basic`, `sound`, `rgb`, `battery`, `pocketjoke`, `roboeyes`, `planeradar`, `radio` (`pio run -e <env>` para cada uno).
