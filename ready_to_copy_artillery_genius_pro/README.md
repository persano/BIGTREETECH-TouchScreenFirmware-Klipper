# Ready-to-Copy SD Card Pack — Artillery Genius Pro

This directory contains everything pre-packaged for **Artillery Genius Pro** (MKS GD TFT28 V1.2/1.4 with GD32F305 MCU).

### Contents
- `MKSTFT28EVO.bin` — Compiled Klipper Standalone firmware binary (built with all upstream bugfixes, GD32 bootloader hang fix, and Klipper UI).
- `config.ini` — Pre-tuned configuration:
  - 250000 baud UART on Port 1 (`P1:8`)
  - 220 x 220 x 250 mm machine volume
  - Direct-drive pause retract (`R5.0 P5.0`)
  - Bed leveling edge distance: 35 mm
  - Leveling feedrate: Z 3000 mm/min
  - Probing Z raise: 15.0 mm
  - Stock Artillery filament runout sensor (`fil_runout:1`, `fil_runout_inverted:0`, 500 ms threshold, 14 mm distance)
  - Pre-print bed heatsoak rate: 10
  - Pre-heats tuned: PLA (210/65), ABS (230/100), TPU (220/70), Nylon (250/100)
  - Klipper standalone modes: `serial_always_on:1`, `emulated_m109_m190:0`, `m27_always_active:1`, `m27_refresh_time:3`
- `language_en.ini` — Synchronized English language pack with all 353 string labels.
- `reset.txt` — Forces the TFT to reload configuration and fonts/icons on boot.
- `TFT28/bmp/` — Complete set of 143 icons sized for 2.8" display.
- `TFT28/font/` — Complete set of font files (`byte_ascii.fon`, `large_byte_ascii.fon`, `word_unicode.fon`, `8x16_byte_ascii.fon`).

---

### How to Flash Your Screen

1. Take a micro SD card (FAT32 formatted, 32GB or smaller recommended).
2. Copy **ALL files and folders from this directory directly to the root of the SD card**:
   ```
   SD_ROOT/
   ├── MKSTFT28EVO.bin
   ├── config.ini
   ├── language_en.ini
   ├── reset.txt
   └── TFT28/
       ├── bmp/
       └── font/
   ```
3. Insert the SD card into the TFT slot on the Artillery Genius Pro.
4. Power on the printer.
5. The screen will display the flashing progress bar as it writes the firmware, fonts, and icons into SPI flash.
6. When flashing finishes, the screen boots into the new Klipper Standalone UI.
7. (Optional) Remove the SD card or keep it inserted.

---

### OrcaSlicer Thumbnail Setup (No Plugin Needed!)

This firmware decodes stock Base64 PNG thumbnails natively on the GD32F305 MCU:
1. In OrcaSlicer / PrusaSlicer, open **Printer Settings** -> **Basic Information**.
2. Set **G-code thumbnails**: `300x300, 70x70`
3. Set format to: **PNG**
4. Export or upload directly — thumbnails will render without any postprocessing script!
