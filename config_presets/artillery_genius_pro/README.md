# Artillery Genius Pro — Klipper + BTT TFT Standalone Preset

## Overview
This preset configures the BigTreeTech TouchScreen Firmware (Klipper Standalone Edition) for the **Artillery Genius Pro** running Klipper via `klipper-btt-tft-bridge` on a host SBC (e.g. Raspberry Pi 5).

## Hardware Details
- **TFT Screen:** MKS TFT28 V1.2.4 with GD32F305VCT6 MCU.
- **SRAM:** 96 KB SRAM unlocked (custom linker script provides ample buffer space for dynamic macros, thumbnail parsing, and Wi-Fi scan tables).
- **Geometry:** 220 mm x 220 mm x 250 mm.
- **Extruder:** Direct drive Titan Aero style.
- **Stock Probe Offset:** X: +27.25 mm, Y: -12.8 mm.

## Connection & Wiring (Raspberry Pi 5 / Host SBC)
Connect the TFT 4-pin or 8-pin AUX serial cable to the Raspberry Pi GPIO header:

| TFT Pin | Function | Raspberry Pi 5 Pin | Header Pin # |
| :--- | :--- | :--- | :--- |
| **5V / VCC** | 5V Power | 5V Power | Pin 2 or 4 |
| **GND** | Ground | Ground | Pin 6, 9, or 14 |
| **TX** | Serial Transmit | GPIO 15 (UART RXD0) | Pin 10 |
| **RX** | Serial Receive | GPIO 14 (UART TXD0) | Pin 8 |

*Configure Klipper host serial port to `/dev/ttyAMA0` (or your configured UART alias) with baud rate `250000`.*

## Flashing Instructions
1. Compile firmware target `MKS_GD_TFT28_V1_2_4` via PlatformIO:
   ```bash
   pio run -e MKS_GD_TFT28_V1_2_4
   ```
2. Format a standard micro SD card (<= 32 GB) to **FAT32** with 4096-byte allocation unit size.
3. Copy the compiled binary:
   - Rename `.pio/build/MKS_GD_TFT28_V1_2_4/MKS_GD_TFT28_V1_2_4.28.x.bin` to `mkstft28evo.bin`
   - Copy `mkstft28evo.bin` to SD card root.
4. Copy assets from `Copy to SD Card root directory to update/`:
   - `TFT28/` (font and icon bitmaps)
   - `Language Packs/`
   - `Theme Packs/`
   - Copy `config_presets/artillery_genius_pro/config.ini` to the SD card root as `config.ini`.
5. Insert the SD card into the TFT's side slot and power cycle the printer.
6. The screen will display flashing progress and reboot into the standalone Klipper interface upon completion.
