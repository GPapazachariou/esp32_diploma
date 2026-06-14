# REPORT_11: Removing WiFi Entirely — Raspberry-Pi-Only Control

### Feasibility, Method, and Assessment

---

## Executive Summary

**Question:** Can WiFi be removed from `PanTilt_Firmware/` completely, leaving the
Raspberry Pi as the sole controller of the ESP32? If so, how — and is it a good
idea?

**Answer:** **Yes, it is entirely possible, and the removal is clean.** The
Raspberry Pi already has *full* control today through the GPIO UART link
(`Serial2`, GPIO 16/17). Every command the WiFi web page can send, the Pi can
already send; every piece of telemetry is already broadcast to the Pi via
`jsPrint()`. WiFi is, functionally, a **redundant second control channel** plus a
browser joystick UI — nothing the system *depends* on.

**My assessment:** For a deployed system where the Pi is the permanent
controller, **removing WiFi is a good idea** — it frees ~40–60 KB of RAM and a
large amount of flash, cuts power draw, speeds and stabilises boot, improves
real-time loop determinism, and eliminates a real security hole (an open access
point with the default password `12345678`). The main things you give up —
wireless debugging and a standalone browser UI — are better provided by the Pi
itself, which is far more capable of hosting a UI than the ESP32.

**My recommendation:** Do it, but do it behind a **compile-time switch**
(`#define ENABLE_WIFI`) rather than deleting the code outright, so you keep a
wireless build available for bench testing without maintaining two codebases.
Details and a full step-by-step follow.

---

## Part 1 — Why This Is Even Possible: The Pi Is Already a Full Controller

A crucial fact that makes this whole question easy: **the Raspberry Pi is not a
second-class input.** Look at how the firmware is wired today.

```
                    ┌──────────────────────────────────────────┐
                    │           jsonCmdReceiveHandler()         │  ← ONE dispatcher
                    │            (uart_ctrl.h switch)           │
                    └──────────────────────────────────────────┘
                        ▲              ▲                 ▲
          serialCtrl()  │  serial2Ctrl()│   webCtrlServer() /js
            (USB)       │   (RPi UART)  │      (WiFi HTTP)
                        │              │                 │
                   GPIO 0/1       GPIO 16/17         port 80
```

All three channels parse the **same** JSON into the **same** `jsonCmdReceive`
buffer and run the **same** handler. There is no command, mode, or feature that
is reachable only over WiFi. The web page (`/js`) is just a fourth way to call a
function that the UART path can call too.

Likewise on the way out: `jsPrint()` writes every response and every telemetry
packet to **both** `Serial` (USB) and `Serial2` (the Pi). The Pi already receives
base-info streams (`T:1001`), IMU data (`T:1002`), servo errors (`T:1005`), and
all command acknowledgements. **No telemetry is WiFi-only.**

**Conclusion:** Removing WiFi removes a redundant input path and a browser UI. It
removes *zero* control capability from the Raspberry Pi.

---

## Part 2 — What Exactly Depends on WiFi (Dependency Map)

I traced every WiFi-related symbol across the codebase. The dependencies are
well-isolated. Here is the complete picture.

### 2.1 Files that are 100% WiFi/web

| File | Size | Role | Action |
|------|------|------|--------|
| `wifi_ctrl.h` | ~9 KB | AP/STA state machine, config persistence, MAC utils | **Delete** |
| `http_server.h` | ~0.6 KB | `WebServer` on :80, `/js` handler | **Delete** |
| `web_page.h` | **~50 KB** | The entire HTML/JS control page as a PROGMEM string | **Delete** |

`web_page.h` alone is ~50 KB of flash — the single biggest saving.

### 2.2 Call sites in the rest of the code (the touch points to fix)

| Location | WiFi reference | Fix |
|----------|----------------|-----|
| `.ino` includes | `<WiFi.h>`, `<WebServer.h>`, `"wifi_ctrl.h"`, `"http_server.h"` | Remove the 4 includes |
| `.ino setup()` | `initWifi()`, `initHttpWebServer()`, `updateOledWifiInfo()` | Remove first two; replace OLED call (below) |
| `.ino loop()` | `server.handleClient()` | Remove |
| `uart_ctrl.h` | WiFi command cases `401–408` (`CMD_WIFI_*`) | Remove those `case` blocks |
| `json_cmd.h` | `CMD_WIFI_*` and `CMD_WIFI_INFO` defines | Remove (optional, harmless) |
| `ugv_config.h` | `String thisMacStr;`, `String jsonFeedbackWeb;` | Remove (only used by WiFi/web) |

### 2.3 The one trap to avoid

`jsonInfoHttp` **looks** WiFi-related (the name says "Http") but it is the
**universal response scratch buffer used by 106 call sites** across IMU, gimbal,
battery, files, and telemetry. **It must stay.** Only `jsonFeedbackWeb` (the
HTTP-body string, used solely in `http_server.h`) is web-specific.

### 2.4 The OLED coupling

`updateOledWifiInfo()` (in `wifi_ctrl.h`) is what currently draws the status
screen, keyed on `WIFI_CURRENT_MODE`. When WiFi is removed, the setup call to it
must be replaced with a couple of plain lines, e.g.:

```c
screenLine_0 = "Gimbal Controller";
screenLine_1 = "RPi UART ready";
screenLine_3 = "V:" + String(loadVoltage_V);
oled_update();
```

`WIFI_CURRENT_MODE`, `thisDevMac[]`, `macToString()`, `getThisDevMacAddress()` are
all self-contained inside `wifi_ctrl.h` and disappear with it.

### 2.5 LittleFS / config

`/wifiConfig.json` and the load/save logic live in `wifi_ctrl.h` and go away.
**`files_ctrl.h` (LittleFS itself and the generic file API) is independent of
WiFi and should be kept** — it may still be useful, and `initFS()` is harmless.

---

## Part 3 — How To Do It (Step by Step)

The change is mechanical and low-risk because the cut lines are clean.

### Step 1 — `PanTilt_Firmware.ino`
- Delete includes: `#include <WiFi.h>`, `#include <WebServer.h>`,
  `#include "wifi_ctrl.h"`, `#include "http_server.h"`.
- In `setup()`: delete `initWifi();` and `initHttpWebServer();`. Replace
  `updateOledWifiInfo();` with the static OLED block shown in §2.4.
- In `loop()`: delete `server.handleClient();`.

### Step 2 — `uart_ctrl.h`
- Delete the `case CMD_WIFI_ON_BOOT … CMD_WIFI_STOP` blocks (T-codes 401–408).
- Leave everything else (gimbal, IMU, files, system) untouched.
- `jsPrint()` stays exactly as is — it still serves USB + Pi.

### Step 3 — `json_cmd.h` (optional cleanup)
- Remove the `CMD_WIFI_*` `#define`s. Harmless if left, but tidy to drop.

### Step 4 — `ugv_config.h` (optional cleanup)
- Remove `String thisMacStr;` and `String jsonFeedbackWeb;` (now unused).
- **Keep `jsonInfoHttp` and `jsonInfoSend`.**

### Step 5 — Delete files
- Delete `wifi_ctrl.h`, `http_server.h`, `web_page.h`.
- Optionally delete `data/wifiConfig.json`.

### Step 6 — `platformio.ini`
- **No `lib_deps` change is required.** `WiFi` and `WebServer` are part of the
  arduino-esp32 **core**, not external libraries. Once nothing references them,
  the linker garbage-collects the unused code. The Adafruit/INA219/ArduinoJson
  deps stay (OLED, battery, JSON are still used).

### Step 7 — Verify it still builds and runs
- Compile. Expect a noticeably smaller binary (see Part 4).
- On boot, OLED should show "RPi UART ready"; the Pi should be able to drive the
  gimbal over GPIO 16/17 exactly as before; USB debug still works.

### Optional: keep the MAC for identification
If the Pi still wants the ESP32's MAC for device identification, you don't need
the WiFi stack — read it from eFuse:
```c
#include "esp_mac.h"
uint8_t mac[6];
esp_read_mac(mac, ESP_MAC_WIFI_STA);   // reads eFuse, radio not required
```

---

## Part 4 — Resource & Benefit Analysis

### 4.1 Flash (program size)

| Item removed | Approx. flash saved |
|--------------|---------------------|
| `web_page.h` PROGMEM HTML/JS string | **~50 KB** |
| `WebServer` + HTTP parsing | ~20–40 KB |
| WiFi/lwIP stack code (once unreferenced) | ~100–200 KB |
| **Total** | **~170–290 KB** |

On a 4 MB flash part this is not about fitting — it's headroom, faster OTA/upload,
and a smaller attack/maintenance surface.

### 4.2 RAM (the more valuable saving)

The ESP32 WiFi stack reserves large dynamic buffers. Bringing WiFi up typically
consumes **~40–60 KB of heap** that never comes back while the radio is active.
On a part with ~300 KB usable DRAM, reclaiming this is significant — it directly
reduces fragmentation and brown-out/allocation-failure risk, which are the most
common ESP32 field crashes.

### 4.3 Power

The WiFi radio (AP or STA) draws on the order of **+100–240 mA** with transmit
spikes higher. For a battery-powered gimbal this is often the largest single
consumer after the servos. Removing it meaningfully extends runtime and, just as
importantly, removes the **current-spike brown-outs** that WiFi TX bursts can
cause on marginal power supplies — a frequent cause of random ESP32 resets.

### 4.4 Real-time determinism

Today `loop()` calls `server.handleClient()` every pass, and the WiFi driver runs
its own RTOS tasks and interrupts that preempt the loop. Removing WiFi makes the
single-threaded control loop **more deterministic** — steadier servo update
timing and IMU sampling, which matters for the stabilisation mode (the Mahony
filter assumes a roughly fixed sample period).

### 4.5 Boot time & reliability

- No risk of the **up-to-15 s blocking STA connect timeout** at boot.
- No AP setup, no DHCP, no config-file parsing for WiFi.
- Fewer code paths = fewer failure modes. Boot becomes fast and predictable.

### 4.6 Security

The current default is an **open-ish AP with the well-known password
`12345678`** at `192.168.4.1`, exposing the full command set (including reboot,
NVS erase, file write, servo reconfiguration) to anyone in radio range. Removing
WiFi **eliminates this remote attack surface entirely.** The UART link is
physically scoped to the wires between the Pi and ESP32.

---

## Part 5 — What You Give Up (Honest Trade-offs)

| Lost capability | Severity | Mitigation |
|-----------------|----------|------------|
| Browser joystick UI (`web_page.h`) | Low–Medium | The Raspberry Pi can host a far better web UI itself and relay commands over UART. This is strictly more capable. |
| Wireless monitoring / debugging | Medium (dev convenience) | Use USB serial, or SSH into the Pi which already receives all telemetry. |
| Standalone operation with **no Pi** | Medium | Only matters if you ever run the gimbal without the Pi (demos, bench). Keep a WiFi build for that (see Part 6). |
| Future OTA-over-WiFi | Low | Not implemented today anyway; flash over USB, or have the Pi flash the ESP32 over UART/`esptool`. |
| Wireless fallback if the UART link fails | Low–Medium | The UART link becomes a single point of control. In practice it's far more reliable than WiFi, but there's no radio backup. |

None of these affect the **deployed Pi-controlled use case**. They are all
development conveniences or no-Pi scenarios.

---

## Part 6 — Recommendation: Compile-Time Switch, Not Deletion

Rather than permanently deleting the WiFi code, my recommended engineering
approach is a **single compile-time flag** so one codebase produces both a lean
production build and a WiFi-enabled bench build:

```c
// ugv_config.h
#define ENABLE_WIFI 0      // 0 = Pi-only (production), 1 = WiFi + web (bench)
```

Then guard the touch points:

```c
// .ino
#if ENABLE_WIFI
  #include <WiFi.h>
  #include <WebServer.h>
  #include "wifi_ctrl.h"
  #include "http_server.h"
#endif
...
#if ENABLE_WIFI
  initWifi();
  initHttpWebServer();
  updateOledWifiInfo();
#else
  // static OLED status
#endif
...
#if ENABLE_WIFI
  server.handleClient();
#endif
```

and wrap the WiFi `case`s in `uart_ctrl.h` the same way.

**Why this is better than hard deletion:**
- Production firmware is lean (all the §4 benefits) when `ENABLE_WIFI 0`.
- You keep the ability to flip to a wireless build for field debugging without a
  Pi, with zero code divergence.
- The cut points are already clean (Part 2), so the `#if` guards are few and
  readable.
- It documents *intent*: WiFi is optional, not load-bearing.

If you are certain WiFi will never be wanted again, full deletion (Part 3) is
also perfectly safe — the dependency isolation supports it. But the flag costs
almost nothing and preserves optionality.

---

## Part 7 — Verdict

| Criterion | Verdict |
|-----------|---------|
| **Is it possible?** | **Yes — cleanly.** WiFi is isolated; the Pi already has full control over UART. |
| **Is it a good idea (deployed, Pi-controlled)?** | **Yes.** Saves RAM/flash/power, improves determinism and reliability, removes a real security hole. |
| **Best way to do it** | Compile-time `ENABLE_WIFI` flag (keep optionality), defaulting to OFF for production. |
| **Caveat** | Keep a WiFi-enabled build path if you ever run the gimbal without the Pi or want wireless bench debugging. |

The Raspberry Pi is the natural place for *all* higher-level concerns — UI,
networking, remote access, logging — because it is a full Linux computer. The
ESP32's job is hard real-time servo + sensor control. Removing WiFi pushes each
device toward what it's good at: the ESP32 becomes a lean, deterministic,
low-power motion controller, and the Pi owns connectivity. That is a sound
architectural direction.

---

*Generated 2026-06-14. Analysis of `PanTilt_Firmware/` as currently committed.
This report is analysis only — no code was modified.*
