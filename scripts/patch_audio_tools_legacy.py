# Patches audio-tools I2SESP32.h: legacy I2S driver uses ChannelReducerT without
# including BaseConverter.h (broken on ESP-IDF 4.x / Arduino core 2.x).
Import("env")

import os

PATCH_MARKER = "BaseConverter.h"
NEEDLE = '#include "esp_system.h"'
INSERT = (
    '#include "esp_system.h"\n'
    '#include "AudioTools/CoreAudio/AudioBasic/Collections.h"\n'
    '#include "AudioTools/CoreAudio/BaseConverter.h"'
)

path = os.path.join(
    env.subst("$PROJECT_LIBDEPS_DIR"),
    env.subst("$PIOENV"),
    "audio-tools",
    "src",
    "AudioTools",
    "CoreAudio",
    "AudioI2S",
    "I2SESP32.h",
)

if os.path.isfile(path):
    with open(path, encoding="utf-8") as f:
        content = f.read()
    if PATCH_MARKER not in content and NEEDLE in content:
        with open(path, "w", encoding="utf-8") as f:
            f.write(content.replace(NEEDLE, INSERT, 1))
        print("patch_audio_tools_legacy: patched I2SESP32.h")
