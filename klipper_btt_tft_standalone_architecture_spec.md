# Agent Implementation Spec: Standalone Klipper Firmware Fork & Bridge Ecosystem

## 1. Repository Topology & Source-of-Truth Roles

Do **not** merge the three existing repositories into a monorepo. They are distinct components and serve as the **sources of truth** for creating a new, dedicated **Klipper-First BTT TFT Firmware Repository** (alongside targeted RPC extensions in the bridge):

1. **New Target Firmware Repository (Direct Fork of `persano/BIGTREETECH-TFT`):**
   * **Role:** The primary target of this development effort. It **must be branched directly from `persano/BIGTREETECH-TFT`** so that all of `persano`'s existing fixes, hardware targets, and build configurations are preserved as the baseline.
   * **Goals:**
     * Cherry-pick valuable unmerged upstream PRs from `bigtreetech/BIGTREETECH-TouchScreenFirmware/pulls`.
     * Strip unusable Marlin-only EEPROM menus and adapt supported tuning menus to Klipper.
     * Add native standalone appliance capabilities: on-screen Wi-Fi setup, live Klipper macro discovery, Klipper control actions (`RESTART`, `FIRMWARE_RESTART`, `SAVE_CONFIG`), and Mainsail `virtual_sdcard` browsing.
     * Package community-ready SD card assets (`config.ini` presets, language packs, and themes) with a dedicated preset folder for the **Artillery Genius Pro** (and easily extensible to other machines).

2. **`persano/klipper-btt-tft-bridge` (Source of Truth — Klippy Host Bridge):**
   * **Role:** In-process Klippy extras module (`tftbridge.py`) running on the Raspberry Pi 5 via `reactor.register_fd`.
   * **Existing Capabilities to Preserve:** Instant local status replies from Klipper's object model (`M105`, `M114`, `M220`/`M221`, `M27`), single-`ok` synchronization, `echo:busy: processing` keepalives, `M155` temperature auto-reports, host print progress detection (`File opened:` + `M27` byte reporting), and UTF-8 garbage-byte (`0xFD`) filtering.
   * **Planned Extensions:** Add handlers for the custom `M980`–`M989` RPC command block and emit `//bridge:<topic>:<payload>` structured responses.

3. **`persano/btt-tft-postprocess` (Source of Truth — Slicer Pipeline):**
   * **Role:** Slicer post-processing pipeline (`btt_postprocess.py`) for OrcaSlicer/PrusaSlicer.
   * **Existing Capabilities to Leverage:** Generates BTT RGB565 hex thumbnails (`;simage:` / `;gimage:`), injects `M118`/`M73` progress metadata, and minifies G-code. The firmware and bridge will consume its exact thumbnail and metadata format when browsing or starting prints from Klipper's `virtual_sdcard`.

---

## 2. Unmerged Upstream PRs & Community Enhancements to Cherry-Pick

Starting from `persano/BIGTREETECH-TFT`, inspect what is already applied and cherry-pick the following unmerged Pull Requests from `bigtreetech/BIGTREETECH-TouchScreenFirmware/pulls` (plus proven Artillery TFT patches):

### 2.1 Core Performance, UART & OS Timer PRs

| Upstream Ref | Feature / Fix Summary | Why It Matters for Klipper + Pi 5 Bridge |
| :--- | :--- | :--- |
| **PR #2844** | **DMA-Based Serial Transmission (TX)** | Replaces byte-by-byte interrupt UART TX with DMA transfers. Boosts TFT serial throughput by >20% and eliminates UI micro-stutters during heavy serial traffic. |
| **PR #2846** | **Purely Interrupt-Driven Buzzer Handler** | Reworks `Buzzer.c` to run entirely on timer interrupts so acoustic alerts never block the main UI/serial loop. |
| **PR #2833** | **OS Timer IRQ Optimization & 54-Day Overflow Fix** | Streamlines `os_timer` IRQ handling and fixes the 32-bit millisecond wraparound bug that freezes the TFT after 54 days of continuous 24/7 uptime. |
| **Artillery GD32 Patch** | **Unlock 96 KB SRAM on GD32F305VCT6 (up from 64 KB) + `.cur` Bootloader Fix** | Artillery Genius Pro / Sidewinder X2 TFTs with the GD32F305 MCU have 96 KB RAM, but stock BTT linker scripts only allocate 64 KB. The extra 32 KB SRAM provides ample headroom for Wi-Fi AP tables, dynamic Klipper macro lists, and thumbnail buffers. Also fixes the `mkstft28evo.cur` infinite re-flash loop on reboot if not already present in `persano/BIGTREETECH-TFT`. |

### 2.2 UI, Leveling, Input & Parsing PRs

| Upstream Ref | Feature / Fix Summary | Integration Note for Klipper Fork |
| :--- | :--- | :--- |
| **PR #2856** | **Numpad Bugfixes (`Numpad.c`)** | Fixes decimal point, negative sign, and buffer overflow glitches in the numeric keypad used for setting temperatures, Z-offset, and Pressure Advance. |
| **PR #2825 & #2855** | **Corner Leveling & Manual Leveling Rework** | Splits `Machine size` into `Motion limits` and `Bed size` in `config.ini`. Removes the useless `Disarm` submenu and `M48` button from Corner Leveling, replaces `M48` with a `Home Z` button, and shows `"---"` on probe error instead of stale values. Hook this directly to Klipper's `BED_SCREWS_ADJUST` / `SCREWS_TILT_CALCULATE`. |
| **PR #2834** | **Move Menu Cosmetic & Ergonomic Rework** | Cleans up `Move.c` layout and jogging button responsiveness. |
| **PR #2879** | **USB/SD Icon & Font Updater Fixes** | Fixes icon mismatch during updates, fixes icon updates from USB sticks, and cleans up overlapping text during font flashing. |
| **PR #2913 (Fixed)** | **`M115` `KINEMATICS` Parse Fix** | Fixes `M115` capability parsing without breaking multi-extruder or custom capability strings returned by `tftbridge.py`. |
| **Community Patch** | **M73 Remaining Time Seconds-Jump Fix** | `M73` reports remaining time in whole minutes, which causes the TFT's `HH:MM:SS` countdown to jump back to `:00` seconds every time an `M73` arrives from Klipper or `btt-tft-postprocess`. Preserve smooth second interpolation between `M73` updates. |
| **Community Patch** | **Suppress `M105` Polling While Terminal Is Open** | Stops `M105` temperature requests from flooding the on-screen G-code terminal so the user can cleanly read Klipper `//` and `echo:` responses. |
| **Community Patch** | **Smart Heating Acoustic Feedback** | Only triggers the "temperature reached" beep if the temperature change was initiated from the TFT GUI (ignores external temp changes from Mainsail/macros) and fixes cooldown beep logic. |
| **Community Patch** | **Pre-Print Automatic Bed Heatsoak UI** | Adds a configurable bed heatsoak countdown screen with live graphical progress, `+`/`-` soak time adjustment, and a `Skip` button before print start. |

---

## 3. Custom RPC Transport Protocol (`M980`–`M989` & `//bridge:`)

### 3.1 Constraints in `BIGTREETECH-TFT` Serial Queue

* Outgoing commands queued via `storeCmd()` in `TFT/src/User/API/interfaceCmd.c` set `infoHost.wait = true` and block the queue until `tftbridge.py` returns `ok\n`.
* While waiting, `tftbridge.py`'s existing `busy_interval` timer sends `echo:busy: processing\n` every 2 seconds, resetting the TFT's command timeout timer during slow operations like Wi-Fi scanning or WPA connection.
* **CRITICAL SANITIZATION WARNING:** `interfaceCmd.c` strips comments starting with `;`, trims spaces, and uppercases characters on standard M-codes. Any string payload containing case-sensitive characters, spaces, or symbols (such as Wi-Fi SSIDs, WPA passwords, or filenames) **MUST be hex-encoded** in C before calling `storeCmd()` and decoded in `tftbridge.py` with `bytes.fromhex()`.

### 3.2 Protocol Definition

| Direction | Command / Prefix | Wire Format | Description |
| :--- | :--- | :--- | :--- |
| **TFT -> Bridge** | `M980` | `M980\n` | Query current Wi-Fi status, active SSID, and IPv4 address. |
| **Bridge -> TFT** | `//bridge:wifi_status:` | `//bridge:wifi_status:<0\|1>\|<ip>\|<ssid>\n` | `1` = connected, `0` = disconnected, followed by IP and raw SSID, then `ok\n`. |
| **TFT -> Bridge** | `M981` | `M981\n` | Trigger an asynchronous Wi-Fi access point scan via `nmcli`. |
| **Bridge -> TFT** | `//bridge:wifi_ap:` | `//bridge:wifi_ap:<signal>\|<0\|1>\|<ssid>\n` | Sent once per discovered AP (up to 16). `<signal>` is 0–100, `<0\|1>` indicates security (`0` = Open, `1` = WPA/WPA2/WPA3). |
| **Bridge -> TFT** | `//bridge:wifi_scan_done` | `//bridge:wifi_scan_done\n` | Signals scan completion, followed immediately by `ok\n`. |
| **TFT -> Bridge** | `M982` | `M982 S<hex_ssid> P<hex_pass>\n` | Connect to a Wi-Fi AP using hex-encoded SSID and password (`P` omitted if open network). |
| **Bridge -> TFT** | `//bridge:wifi_conn:` | `//bridge:wifi_conn:<0\|1>\|<msg>\n` | `1` = connected (with new IP in `<msg>`), `0` = failed (with error summary in `<msg>`), then `ok\n`. |
| **TFT -> Bridge** | `M985` | `M985\n` | Request dynamic list of public `[gcode_macro ...]` names from Klipper. |
| **Bridge -> TFT** | `//bridge:macro:` | `//bridge:macro:<MACRO_NAME>\n` | Sent once per macro (excluding `_` internal macros), ending with `//bridge:macro_done\n` and `ok\n`. |
| **TFT -> Bridge** | `M986` | `M986\n` | Request current Klipper tuning parameters (Pressure Advance, Accel, Square Corner Velocity, Z-Offset). |
| **Bridge -> TFT** | `//bridge:klipper_params:` | `//bridge:klipper_params:PA=<f>\|ACCEL=<d>\|SCV=<f>\|ZOFF=<f>\n` | Updates TFT parameter structs with live Klipper values, followed by `ok\n`. |
| **TFT -> Bridge** | `M987` | `M987 S<hex_filename>\n` | Request embedded RGB565 thumbnail header generated by `btt-tft-postprocess` for a file in Klipper's `virtual_sdcard`. |

### 3.3 Firmware Hook in `Mainboard_AckHandler.c`

In the new firmware fork (`TFT/src/User/API/Mainboard_AckHandler.c`):

1. Add a routing branch near the top of the ACK parsing chain:
```c
else if (ack_cmp("//bridge:"))
{
  parseBridgeMessage(ack_cur_src + strlen("//bridge:"));
}
```
2. Create `TFT/src/User/API/BridgeParser.c` and `BridgeParser.h` to implement `parseBridgeMessage(char *msg)` and dispatch `wifi_status:`, `wifi_ap:`, `wifi_scan_done`, `wifi_conn:`, `macro:`, and `klipper_params:` to their respective state structs without printing them to the terminal or triggering unwanted popup modals.

---

## 4. Standalone Wi-Fi Provisioning (`WifiSettings.c` + `tftbridge.py`)

### 4.1 Firmware Side (New Firmware Fork)

#### Data Structures (`TFT/src/User/Menu/WifiSettings.h`)

```c
#ifndef _WIFI_SETTINGS_H_
#define _WIFI_SETTINGS_H_

#include <stdbool.h>
#include <stdint.h>

#define MAX_WIFI_APS 16
#define MAX_SSID_LEN 33
#define MAX_PASS_LEN 64

typedef struct {
  char ssid[MAX_SSID_LEN];
  uint8_t signal;   // 0 - 100
  uint8_t secured;  // 0 = Open, 1 = WPA/WPA2/WPA3
} WIFI_AP_ITEM;

typedef struct {
  bool connected;
  bool scanning;
  bool connecting;
  char current_ssid[MAX_SSID_LEN];
  char ip_addr[16];
  char status_msg[48];
  uint8_t ap_count;
  WIFI_AP_ITEM aps[MAX_WIFI_APS];
} WIFI_MANAGER;

extern WIFI_MANAGER wifiManager;

void menuWifiSettings(void);
void wifiParseMessage(char *payload);

#endif
```

#### UI Implementation (`TFT/src/User/Menu/WifiSettings.c`)

Reuse the existing TFT widgets rather than building new graphics primitives:

1. **Paginated List (`ListItem.c` / `LISTITEMS`):**
   * Use `LISTITEMS` (the same layout used in `Print.c` and `RRFMacros.c`) to display 5 networks per page with `KEY_ICON_5` (Page Up), `KEY_ICON_6` (Page Down), and `KEY_ICON_7` (Back / Rescan).
   * Display the current IP address and connection status in the menu title bar (e.g., `"Wi-Fi: 192.168.1.50 (MyNet)"` or `"Wi-Fi: Disconnected"`).
   * On menu entry, clear `wifiManager.ap_count`, set `wifiManager.scanning = true`, and queue `storeCmd("M980\n")` followed by `storeCmd("M981\n")`.
2. **Password Entry (`Keyboard.c`):**
   * When a user taps an AP row in the list, inspect `wifiManager.aps[selected_index]`.
   * If `secured == 1`, invoke the existing on-screen ASCII keyboard widget (used by `Terminal.c` / `SendGcode.c`) to capture the user's password string.
   * Convert both `ssid` and `password` into hex strings using a helper `void toHex(const char *in, char *out, size_t max_len)`.
   * Queue `storeCmd("M982 S%s P%s\n", hex_ssid, hex_pass);` and display a `"Connecting..."` status banner until `//bridge:wifi_conn:` arrives.
3. **Menu Registration:**
   * Surface a dedicated **Wi-Fi** button in `SettingsMenu.c` (and show the active IP in `InfoSettings.c`) that pushes `menuWifiSettings` onto `infoMenu.menu[++infoMenu.cur]`.

### 4.2 Python Bridge Side (`persano/klipper-btt-tft-bridge`)

#### Critical Rule: Non-Blocking Reactor Execution

`tftbridge.py` runs inside Klippy's single-threaded event loop (`reactor`). **Never call `subprocess.run()` or `subprocess.check_output()` synchronously on the reactor thread.** Wi-Fi scanning and WPA handshakes take 3 to 10 seconds and will crash Klipper with `MCU 'mcu' shutdown: Timer too close`.

#### Threaded `nmcli` Implementation Pattern

In `tftbridge.py`, add a non-blocking `WifiBridgeManager` that uses `self.bridge.start_busy_keepalive()` while the background thread runs `nmcli`, and hands the result back to the reactor thread via `self.reactor.register_async_callback()`:

```python
import subprocess
import threading

class WifiBridgeManager:
    def __init__(self, bridge):
        self.bridge = bridge
        self.reactor = bridge.reactor
        self.busy = False

    def handle_m980_status(self):
        if self.busy:
            self.bridge.send_line("ok")
            return
        self.busy = True
        threading.Thread(target=self._status_worker, daemon=True).start()

    def _status_worker(self):
        lines = []
        try:
            out = subprocess.check_output(
                ["nmcli", "-t", "-f", "ACTIVE,SSID", "dev", "wifi"],
                text=True, timeout=5
            )
            ssid = ""
            for row in out.splitlines():
                if row.startswith("yes:"):
                    ssid = row.split(":", 1)[1].replace("\\:", ":")
                    break

            ip_out = subprocess.check_output(
                ["hostname", "-I"], text=True, timeout=3
            ).strip()
            ip_addr = ip_out.split()[0] if ip_out else "0.0.0.0"

            connected = "1" if ssid else "0"
            lines.append(f"//bridge:wifi_status:{connected}|{ip_addr}|{ssid}")
        except Exception:
            lines.append("//bridge:wifi_status:0|0.0.0.0|")

        self.reactor.register_async_callback(lambda e: self._finish(lines))

    def handle_m981_scan(self):
        if self.busy:
            self.bridge.send_line("ok")
            return
        self.busy = True
        self.bridge.start_busy_keepalive()
        threading.Thread(target=self._scan_worker, daemon=True).start()

    def _scan_worker(self):
        lines = []
        try:
            cmd = [
                "nmcli", "-t", "-f", "SIGNAL,SECURITY,SSID",
                "dev", "wifi", "list", "--rescan", "yes"
            ]
            out = subprocess.check_output(cmd, text=True, timeout=12)
            seen = set()
            for row in out.splitlines():
                parts = row.split(":", 2)
                if len(parts) == 3:
                    sig = parts[0].strip()
                    sec = parts[1].strip()
                    ssid = parts[2].replace("\\:", ":").strip()
                    if ssid and ssid not in seen:
                        seen.add(ssid)
                        sec_flag = "1" if sec and sec != "--" else "0"
                        lines.append(f"//bridge:wifi_ap:{sig}|{sec_flag}|{ssid}")
                        if len(seen) >= 16:
                            break
        except Exception as exc:
            lines.append(f"//bridge:wifi_err:{str(exc)[:32]}")
        lines.append("//bridge:wifi_scan_done")
        self.reactor.register_async_callback(lambda e: self._finish(lines))

    def handle_m982_connect(self, params_str):
        if self.busy:
            self.bridge.send_line("ok")
            return
        hex_ssid = ""
        hex_pass = ""
        for token in params_str.split():
            if token.upper().startswith("S"):
                hex_ssid = token[1:]
            elif token.upper().startswith("P"):
                hex_pass = token[1:]
        try:
            ssid = bytes.fromhex(hex_ssid).decode("utf-8")
            password = bytes.fromhex(hex_pass).decode("utf-8") if hex_pass else ""
        except Exception:
            self.bridge.send_line("//bridge:wifi_conn:0|Invalid Hex")
            self.bridge.send_line("ok")
            return

        self.busy = True
        self.bridge.start_busy_keepalive()
        threading.Thread(
            target=self._connect_worker, args=(ssid, password), daemon=True
        ).start()

    def _connect_worker(self, ssid, password):
        lines = []
        try:
            cmd = ["nmcli", "dev", "wifi", "connect", ssid]
            if password:
                cmd += ["password", password]
            subprocess.check_output(cmd, stderr=subprocess.STDOUT, text=True, timeout=25)
            ip_out = subprocess.check_output(["hostname", "-I"], text=True, timeout=3).strip()
            ip_addr = ip_out.split()[0] if ip_out else "Connected"
            lines.append(f"//bridge:wifi_conn:1|{ip_addr}")
        except subprocess.CalledProcessError as exc:
            err_msg = (exc.output or "Connect failed").strip().splitlines()[-1][:36]
            lines.append(f"//bridge:wifi_conn:0|{err_msg}")
        except Exception as exc:
            lines.append(f"//bridge:wifi_conn:0|{str(exc)[:36]}")

        self.reactor.register_async_callback(lambda e: self._finish(lines))

    def _finish(self, lines):
        self.bridge.stop_busy_keepalive()
        for line in lines:
            self.bridge.send_line(line)
        self.bridge.send_line("ok")
        self.busy = False
```

#### Host OS Permissions (Raspberry Pi 5 / MainsailOS)

Update `install.sh` in `klipper-btt-tft-bridge` so the Klipper user can invoke `nmcli` without `sudo`:

* Add user to `netdev`: `sudo usermod -aG netdev $USER`
* Create `/etc/polkit-1/rules.d/10-klipper-nmcli.rules`:
```javascript
polkit.addRule(function(action, subject) {
    if (action.id.indexOf("org.freedesktop.NetworkManager.") == 0 && subject.isInGroup("netdev")) {
        return polkit.Result.YES;
    }
});
```

---

## 5. Pruning Marlin Bloat & Adapting Menus for Standalone Klipper

### 5.1 Remove Unsupported Marlin Settings in the Firmware Fork

In the new Klipper firmware fork, strip settings that cannot be changed at runtime via Marlin G-codes in Klipper:

1. **`ParameterSettings.c`:**
   * **REMOVE:** Steps per mm (`M92`), TMC Stepper Current (`M906`), TMC Bump Sensitivity (`M914`), TMC Hybrid Threshold (`M913`), Filament Diameter (`M200`), Home Offset (`M206`), and Restore/Reset EEPROM (`M501` / `M502`).
   * **KEEP & ADAPT FOR KLIPPER:**
     * Rename **Linear Advance (`M900`)** -> **Pressure Advance** (query initial state via `M986`).
     * Keep **Max Acceleration (`M204`)**.
     * Rename **Jerk (`M205`)** -> **Square Corner Velocity**.
     * Keep **Probe Z-Offset (`M851 Z`)**.
     * Keep **Firmware Retraction (`M207` / `M208`)** for printers with `[firmware_retraction]` enabled.
2. **`MachineSettings.c` & `FeatureSettings.c`:**
   * Replace Marlin EEPROM buttons (`Save to EEPROM`, `Restore Defaults`, `Reset EEPROM`) with:
     * **Save Config (`SAVE_CONFIG`):** Opens a confirmation popup (`"Save Z-offset/PID to printer.cfg and restart Klipper?"`) before sending `M500\n`.
     * **Restart Klipper (`RESTART`):** Sends `RESTART\n` to reload Klippy configuration.
     * **Firmware Restart (`FIRMWARE_RESTART`):** Sends `FIRMWARE_RESTART\n` to recover from a Klipper MCU shutdown state directly from the screen without needing a browser or phone.

### 5.2 Translate Retained M-Codes in `tftbridge.py`

Extend the existing Marlin-to-Klipper translation table in `tftbridge.py`:

| TFT Command Sent | Klipper Translation in `tftbridge.py` |
| :--- | :--- |
| `M900 K<val>` | `SET_PRESSURE_ADVANCE ADVANCE=<val>` |
| `M204 P<val>` or `M204 S<val>` | `SET_VELOCITY_LIMIT ACCEL=<val>` |
| `M205 X<val>` | `SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=<val>` |
| `M207 S<len> F<spd> Z<zhop>` | `SET_RETRACTION RETRACT_LENGTH=<len> RETRACT_SPEED=<spd/60> Z_HOP=<zhop>` |
| `M851 Z<val>` | Adjust via `SET_GCODE_OFFSET Z=<delta> MOVE=1` and stage for `Z_OFFSET_APPLY_PROBE` |
| `M500` | `Z_OFFSET_APPLY_PROBE` followed by `SAVE_CONFIG` |

---

## 6. Dynamic Klipper Macros & `virtual_sdcard` + Thumbnail Integration

### 6.1 Auto-Discovered Klipper Macros (`M985`)

Instead of relying only on hardcoded `custom_gcode_1`..`15` in `config.ini`:

1. **In `tftbridge.py`:** When `M985` is received, inspect Klipper's registered `gcode_macro` objects:
```python
macros = [
    name.split()[1]
    for name, _ in self.printer.lookup_objects("gcode_macro")
    if not name.split()[1].startswith("_")
]
macros.sort()
for m in macros:
    self.send_line(f"//bridge:macro:{m}")
self.send_line("//bridge:macro_done")
self.send_line("ok")
```
2. **In the Firmware Fork:** Repurpose `TFT/src/User/Menu/RRFMacros.c` into `KlipperMacros.c` so opening the Macros menu sends `M985\n`, populates `LISTITEMS` with all public Klipper macros, and dispatches `storeCmd("%s\n", selected_macro_name)` on tap. (Retain the `Custom` menu from `config.ini` as well if the user wants curated quick-buttons with custom names).

### 6.2 Browsing Klipper's `virtual_sdcard` with `btt-tft-postprocess` Thumbnails

Make the TFT's Onboard SD option browse Mainsail's `~/printer_data/gcodes` directory directly:

1. **File Listing (`M20`) & Print Start (`M23` / `M24`) in `tftbridge.py`:**
   * Query the `[virtual_sdcard]` root directory from `self.printer.lookup_object('virtual_sdcard')`.
   * On `M20`, list `.gcode` files wrapped in `Begin file list\n` ... `End file list\nok\n`.
   * On `M23 <filename>` + `M24`, call `SDCARD_PRINT_FILE FILENAME="<filename>"`.
2. **Thumbnail Streaming (`M987`):**
   * Because G-code files processed by `persano/btt-tft-postprocess` already contain BTT-formatted `;simage:` / `;gimage:` RGB565 hex blocks in the file header, `tftbridge.py` can extract those lines on demand (`M987`) so the TFT displays full model previews even for prints stored on the Raspberry Pi's SD/SSD.

---

## 7. `config.ini`, Themes/Icons, Languages & Community Preset Architecture

To make this firmware both **turnkey for the Artillery Genius Pro** and **reusable by anyone in the Klipper + BTT TFT community**, the repository must carefully handle `config.ini`, icons/themes, and language packs.

### 7.1 `config.ini` Customizations & Preset Folder Structure

1. **Bake Klipper-First Defaults into `Configuration.h`:**
   Update the compiled fallback defaults in `TFT/src/User/Configuration.h` so that even before a user flashes a `config.ini`, the firmware defaults to Klipper-safe behavior:
   * `SERIAL_ALWAYS_ON` -> `1` (`enabled`)
   * `EMULATED_M109_M190` -> `0` (let Klipper/Bridge manage heating waits properly or use controlled heatsoak)
   * `M27_ALWAYS_ACTIVE` -> `1` (so external prints started from Mainsail/OrcaSlicer are detected immediately via `tftbridge.py`)
   * `M27_REFRESH_TIME` -> `3`
   * `PROJECTION_LIGHT` / `NEOPIXEL_PIXELS` -> matched to Artillery Genius Pro extruder LED if desired.

2. **Create a Dedicated `config_presets/` Directory in the Repository:**
   Keep the generic template in `Copy to SD Card root directory to update/config.ini`, and add a curated `config_presets/` folder at the repository root:
   ```text
   btt-tft-klipper-edition/
   ├── Copy to SD Card root directory to update/
   │   ├── config.ini                         # Generic Klipper-ready baseline config.ini
   │   ├── Language Packs/                    # Updated language_*.ini files
   │   └── Theme Packs/                       # Standard & custom theme packs
   └── config_presets/
       ├── artillery_genius_pro/
       │   ├── config.ini                     # Exact Artillery Genius Pro + Klipper Bridge config
       │   └── README.md                      # Wiring & baudrate notes for Genius Pro + Pi 5
       ├── artillery_sidewinder_x2/
       │   └── config.ini                     # 300x300x400 variant for Sidewinder X2 users
       └── generic_klipper/
           └── config.ini                     # Clean starting point for Ender/Voron/CoreXY users
   ```

3. **Specific Settings for `config_presets/artillery_genius_pro/config.ini`:**
   * **Serial & Bridge Sync:** `baudrate:250000` (matching `tftbridge.py`), `serial_always_on:1`, `keep_alive_interval:2`, `m27_always_active:1`, `tx_slots:2`, `tx_delay:0`, `tx_prefetch:0`.
   * **Kinematics & Geometry:** `size_min:X0 Y0 Z0`, `size_max:X220 Y220 Z250` (and if PR #2825 is merged, `bed_size:X220 Y220`).
   * **Extruder & Direct Drive Filament Load/Unload:** Short Titan Aero direct-drive lengths (`filament_load_limit_length:100`, `filament_load_length:65`, `filament_unload_length:60`, `filament_load_speed:300`).
   * **ABL & Tramming Points:** `level_edge_distance:30`, `probe_offset:X27.25 Y-12.8` (standard Artillery Genius Pro inductive/BLTouch mount offset so probing never drives the nozzle off the bed).
   * **Pre-Configured Klipper Custom G-Codes:** Pre-populate `custom_gcode_1`..`15` with standard Klipper commands (`BED_MESH_CALIBRATE`, `SCREWS_TILT_CALCULATE`, `PID_CALIBRATE HEATER=extruder TARGET=210`, `PID_CALIBRATE HEATER=heater_bed TARGET=60`, `SHAPER_CALIBRATE`, `FIRMWARE_RESTART`, `SAVE_CONFIG`) so both the static Custom G-code menu and the dynamic `M985` Macro Browser work out of the box.

### 7.2 Themes & Icons Strategy (Zero Broken Themes)

In `BIGTREETECH-TFT`, full-tile menu buttons (`ICON_*` in `TFT/src/User/API/UI/icon_list.inc`) are loaded from external SPI Flash by index. If a new enum is inserted in the middle of `icon_list.inc`, every subsequent icon shifts by 1 and **breaks every existing theme** unless the user re-flashes custom `.bmp` files for their exact screen resolution (`TFT28`, `TFT35`, `TFT43`, etc.).

To make this firmware work cleanly for everyone across all existing themes (`Unified Menu Material`, `Classic`, `Round Miracle`, etc.):

1. **Rule 1 — Never Insert in the Middle of `icon_list.inc`:**
   * Do not shift existing `ICON_*` indices.
2. **Rule 2 — Reuse Existing Stock Tile Icons for New Top-Level Buttons by Default:**
   * The stock BTT icon pack already includes rich icons that can be bound to our new menus without flashing any new `.bmp` files:
     * **Wi-Fi Menu Button:** Use `ICON_CONNECTION_SETTINGS` or `ICON_GCODE` / `ICON_CLOUD` (or check if `ICON_WIFI` is present in the target theme pack).
     * **Klipper Macros Button:** Use `ICON_GCODE` or `ICON_CUSTOM`.
     * **Save Config (`SAVE_CONFIG`) Button:** Use `ICON_EEPROM_SAVE`.
     * **Firmware Restart / Restart Klipper Button:** Use `ICON_RESET_VALUE` or `ICON_SHUT_DOWN`.
3. **Rule 3 — Use Font Glyphs (`ICONCHAR_*`) & Native 2D Canvas Primitives Inside Submenus:**
   * Inside `WifiSettings.c` and `KlipperMacros.c` (`LISTITEMS` menus), rows do **not** use `.bmp` files from SPI flash. They use built-in font glyphs (`ICONCHAR_WIFI`, `ICONCHAR_LOCK`, `ICONCHAR_OK`, `ICONCHAR_FOLDER`, etc.) and native `GUI_FillRect()` / `GUI_DrawPixel()` calls.
   * We can even draw a crisp 4-bar Wi-Fi signal strength indicator and lock badge in `WifiSettings.c` using `GUI_FillRect()` based on `aps[i].signal`, which works at any resolution and on any theme with zero external `.bmp` dependencies!
4. **Rule 4 — Optional Custom `.bmp` Icons Appended at the End of `icon_list.inc`:**
   * If you want to provide custom-designed Wi-Fi or Klipper logo `.bmp` tiles for the Artillery Genius Pro (`TFT28`), **append** them at the very bottom of `icon_list.inc` (right before `ICON_PREVIEW`) and provide the `.bmp` files in `Copy to SD Card root directory to update/Theme Packs/`.

### 7.3 Language Packs & String Table Architecture (`Language.inc` & `utf8_*.h`)

The BTT TFT language system uses an X-Macro list in `TFT/src/User/API/Language/Language.inc` mapped to compiled C headers (`utf8_en.h`, `utf8_es.h`, etc.) and optional SD card `language_*.ini` files.

To ensure translations work cleanly without mismatched strings:

1. **Append New String Keys at the Bottom of `Language.inc`:**
   Add new keys at the very end of `TFT/src/User/API/Language/Language.inc`:
   * `X_WORD(WIFI_SETTINGS)`
   * `X_WORD(WIFI_SCANNING)`
   * `X_WORD(WIFI_CONNECTING)`
   * `X_WORD(WIFI_CONNECTED)`
   * `X_WORD(WIFI_DISCONNECTED)`
   * `X_WORD(KLIPPER_MACROS)`
   * `X_WORD(PRESSURE_ADVANCE)`
   * `X_WORD(SQ_CORNER_VELOCITY)`
   * `X_WORD(RESTART_KLIPPER)`
   * `X_WORD(FIRMWARE_RESTART)`
   * `X_WORD(SAVE_KLIPPER_CONFIG)`
2. **Update All `TFT/src/User/API/Language/utf8_*.h` Header Files:**
   * Every `utf8_*.h` file in `TFT/src/User/API/Language/` **must** have matching entries for the new `X_WORD` macros so the C array length matches `LABEL_NUM` at compile time.
   * Provide full native translations in `utf8_en.h` (English) and `utf8_es.h` (Spanish), and populate the remaining `utf8_*.h` files with English fallback strings so every language target compiles with zero warnings.
3. **Sync `Copy to SD Card root directory to update/Language Packs/language_*.ini`:**
   * Append the matching `label_wifi_settings:...` keys to the `.ini` files in the repository so users who load secondary languages from the SD card get the new labels automatically.

---

## 8. Multi-Repo Agent Execution Plan

When running your local coding agent, provide it with a workspace containing your three reference repos and initialize the new firmware repository directly from `persano/BIGTREETECH-TFT`:

```text
workspace/
├── BIGTREETECH-TFT/              # Reference source of truth (persano fork)
├── klipper-btt-tft-bridge/       # Reference & target for M980-M989 bridge extensions
├── btt-tft-postprocess/          # Reference source of truth for RGB565 thumbnail/M73 format
└── btt-tft-klipper-edition/      # NEW REPO: Cloned directly from persano/BIGTREETECH-TFT
```

### Ordered Implementation Checklist

* [ ] **Step 1 — Bootstrap `btt-tft-klipper-edition` from `persano/BIGTREETECH-TFT` & Cherry-Pick Upstream PRs:**
  * Clone `persano/BIGTREETECH-TFT` as the base of the new firmware repository.
  * Inspect commits already present in `persano/BIGTREETECH-TFT`, then cherry-pick missing upstream PRs from Section 2: DMA UART TX (**PR #2844**), Interrupt Buzzer (**PR #2846**), OS Timer 54-day overflow fix (**PR #2833**), Numpad fixes (**PR #2856**), Corner Leveling rework (**PR #2825/#2855**), Move menu cleanup (**PR #2834**), `M73` seconds-jump fix, and Terminal `M105` suppression.
  * Verify the Artillery Genius Pro target (`MKS_TFT28_V3_0` / GD32F305 96 KB SRAM linker script and `.cur` bootloader handling).
* [ ] **Step 2 — Extend `persano/klipper-btt-tft-bridge` with RPC Handlers (`M980`–`M987`):**
  * Add `WifiBridgeManager` (`M980`, `M981`, `M982`) using worker threads + `reactor.register_async_callback` + `echo:busy: processing` keepalives.
  * Add `M985` (macro discovery), `M986` (live Klipper parameter query), and extended Marlin-to-Klipper translations (`M900`, `M204`, `M205`, `M207`, `M500`).
  * Update `install.sh` with `netdev` group and Polkit rules for passwordless `nmcli`.
* [ ] **Step 3 — Update `Language.inc`, `utf8_*.h`, and `BridgeParser.c` in Firmware:**
  * Append the new Klipper and Wi-Fi labels to `Language.inc`, `utf8_en.h`, `utf8_es.h`, and fallback `utf8_*.h` files (plus `language_*.ini`).
  * Add `BridgeParser.c` / `BridgeParser.h` and hook `ack_cmp("//bridge:")` in `Mainboard_AckHandler.c`.
* [ ] **Step 4 — Build Standalone Wi-Fi UI & Klipper Menus:**
  * Implement `WifiSettings.c` / `WifiSettings.h` using `LISTITEMS`, `ICONCHAR_*` / `GUI_FillRect` signal indicators, and `Keyboard.c` (with hex-encoded SSID and password for `M982`).
  * Prune unsupported Marlin EEPROM items from `ParameterSettings.c` and `MachineSettings.c`.
  * Add **Restart Klipper (`RESTART`)**, **Firmware Restart (`FIRMWARE_RESTART`)**, **Save Config (`SAVE_CONFIG`)**, and **Klipper Macros (`KlipperMacros.c` via `M985`)** using existing `icon_list.inc` icons so all stock themes work without broken bitmaps.
* [ ] **Step 5 — Package `config_presets/` (Artillery Genius Pro & Generic Klipper) + `virtual_sdcard` Support:**
  * Create `config_presets/artillery_genius_pro/config.ini`, `config_presets/artillery_sidewinder_x2/config.ini`, and `config_presets/generic_klipper/config.ini`.
  * Implement `M20`/`M23`/`M24` against Klipper's `virtual_sdcard` in `tftbridge.py` and verify end-to-end printing and thumbnail parsing with G-code generated through `btt-tft-postprocess`.