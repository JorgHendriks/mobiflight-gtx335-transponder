# GTX335 Firmware

This directory contains the configured MobiFlight community firmware for the GTX335 flight-simulator transponder. It targets the original RP2040-based Raspberry Pi Pico v1 and builds the `jh_gtx335` PlatformIO environment.

> **Disclaimer:** This firmware was created as an AI experiment, and its entire codebase was AI-generated; though it works, it does not meet the author's usual coding standards and should be reviewed accordingly.

End users normally install the precompiled community package from a GitHub release as described in the [project quick start](../README.md#quick-start). The instructions below are for developers building the firmware from source.

## Prerequisites

- [Git](https://git-scm.com/)
- [Visual Studio Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode), or the standalone [PlatformIO Core CLI](https://docs.platformio.org/en/latest/core/installation/index.html)
- Internet access for the first build

The first build downloads the pinned MobiFlight Firmware Source core and the required PlatformIO platforms and libraries. The downloaded `src`, `.pio`, `_build`, and `_dist` directories are generated locally and ignored by Git.

## Compile

Open the `firmware` directory—not the repository root—as the PlatformIO project. From a terminal in that directory, run:

```console
pio run -e jh_gtx335
```

In VS Code, the equivalent is **PlatformIO > Project Tasks > jh_gtx335 > General > Build**.

The build uses:

- MobiFlight Firmware Source core `3.0.0`
- Raspberry Pi Pico / RP2040 with the Arduino-Pico core
- U8g2 for the SSD1322 display

## Build artifacts

A successful local build with no explicit version produces:

```text
.pio/build/jh_gtx335/jh_gtx335_1_0_0.uf2
_dist/GTX335_1.0.0.zip
```

The UF2 file is the compiled firmware. The ZIP file is the complete MobiFlight community package and includes the board definition, custom-device definition, reset firmware, and compiled UF2 in the directory layout expected by MobiFlight.

The `_build` directory is an intermediate staging area.

## Set a build version

Set the `VERSION` environment variable before building to create versioned firmware and package filenames. In PowerShell:

```powershell
$env:VERSION = "1.0.0"
pio run -e jh_gtx335
```

On macOS or Linux:

```sh
VERSION=1.0.0 pio run -e jh_gtx335
```

GitHub release builds take the version from the release tag automatically. Prefer tags such as `v1.0.0`.

## Flash a development build

For normal use, install the generated community ZIP through MobiFlight as described in the [project quick start](../README.md#quick-start).

To flash the UF2 directly during development:

1. Disconnect the Pico.
2. Hold **BOOTSEL** while reconnecting its USB cable.
3. Release **BOOTSEL** when the `RPI-RP2` drive appears.
4. Copy the generated `.uf2` file to that drive.

The Pico restarts automatically after accepting the file. Direct UF2 flashing installs the firmware but does not install the board and device definitions into MobiFlight; use the generated community ZIP as well if MobiFlight has not already been configured for this board.

## Clean and rebuild

```console
pio run -e jh_gtx335 -t clean
pio run -e jh_gtx335
```

## Source layout

| Path | Purpose |
|---|---|
| `GTX335/GTX335State.*` | Panel state, key behavior, timers, message handling, and local settings |
| `GTX335/GTX335Renderer.*` | SSD1322 screen rendering and power/brightness control |
| `GTX335/GTX335Input.*` | Button polling, debouncing, long presses, and repeat behavior |
| `GTX335/GTX335Transport.*` | Named button events sent to MobiFlight |
| `GTX335/MFCustomDevice.*` | Integration with the MobiFlight custom-device API |
| `GTX335/Community` | Board, device, reset-firmware, and release-package metadata |
| `MF_Configs` | MobiFlight board backup and example MSFS 2024 project |

Hardware pin assignments are centralized in `GTX335/GTX335Pins.h`; the matching fixed MobiFlight configuration is in `GTX335/MFCustomDevicesConfig.h`.
