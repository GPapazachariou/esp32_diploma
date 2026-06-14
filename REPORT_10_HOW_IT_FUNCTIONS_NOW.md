# REPORT_10: How the Firmware Functions Now — Detailed Operation

---

## What This Report Is

This is a deep, end-to-end description of how `PanTilt_Firmware/` actually
runs — not a plan, not a command list, but the internal mechanics: what happens
from power-on, how a single command travels from a wire to a moving servo, how
the sensor fusion produces angles, how the safety logic behaves, and exactly
what data flows where. It covers every module and the timing relationships
between them.

It reflects the firmware as it currently stands in `PanTilt_Firmware/`, built
with PlatformIO for the `esp32dev` board.

---

## Part 0 — One-Paragraph Summary

The ESP32 runs a single-threaded `loop()` that, on every pass, drains incoming
JSON commands from three sources (USB, Raspberry-Pi UART, WiFi HTTP), dispatches
each through one shared handler, polls the two gimbal servos, fuses the IMU into
roll/pitch/yaw, optionally streams telemetry back, refreshes the OLED, and runs a
heartbeat watchdog that freezes the gimbal if commands stop arriving. All command
sources speak the identical newline-delimited JSON "T-code" protocol, so a
command behaves the same no matter where it came from. The only blocking code
paths are in `setup()` and in an explicit WiFi-connect command; the main loop
itself never calls `delay()`.

---

## Part 1 — Hardware & Pin Map

| Subsystem | Interface | Pins | Speed | Driver |
|-----------|-----------|------|-------|--------|
| USB / PC + debug | UART0 `Serial` | GPIO 0 / 1 | 115,200 | built-in |
| Raspberry Pi link | UART2 `Serial2` | GPIO 16 / 17 | 115,200 | built-in |
| Servo bus | UART1 `Serial1` | GPIO 18 / 19 | 1,000,000 | `SMS_STS` (half-duplex) |
| OLED / IMU / battery | I²C `Wire` | GPIO 32 (SDA) / 33 (SCL) | — | Adafruit / QMI8658 / AK09918 / INA219 |
| LEDs | LEDC PWM | GPIO 4 / 5 | 200 Hz, 8-bit | `ledc` |

Servo IDs: **pan = 2, tilt = 1**. Center is raw encoder step **2047** (servo has
a 0–4095, 12-bit position range over ~360°).

The pins GPIO 16/17 and the servo lines were repurposed from the original
combined firmware's motor/encoder hardware, which no longer exists in this build.

---

## Part 2 — Boot Sequence (`setup()`)

`setup()` runs once and brings subsystems up in a deliberate order, narrating
progress on the OLED. The order matters because later steps depend on earlier
globals.

```
1.  Serial.begin(115200)            USB — NO "while(!Serial)" guard (the key fix)
2.  Serial2.begin(...,16,17)        Raspberry Pi UART
3.  Wire.begin(32,33)               I²C bus
4.  ina219_init(); inaDataUpdate()  battery monitor + first reading
5.  init_oled()                     show "Gimbal Controller v1.0 starting..."
6.  imu_init()                      QMI8658 + AK09918 magnetometer, reset quaternion
7.  led_pin_init()                  LEDC PWM channels for GPIO 4/5
8.  initFS()                        mount LittleFS (format-on-fail = true)
9.  gimbalServoInit()               Serial1 @1Mbit, ping both servos, center them
10. initWifi()                      load /wifiConfig.json, start AP/STA per config
11. initHttpWebServer()             WebServer on :80
12. updateOledWifiInfo()            show AP SSID/password or STA IP + voltage
13. lastCmdRecvTime = millis()      arm the heartbeat timer
```

**The single most important detail:** the original firmware had
`while(!Serial){}` at the top of setup, which blocks forever until a USB host
enumerates. On battery power that meant the servo bus on step 9 never
initialised. That guard is **removed** — the device now boots fully headless.

`gimbalServoInit()` opens `Serial1` at 1 Mbit, attaches it to the `SMS_STS st`
object, waits 500 ms for servo power-up, pings each servo with `FeedBack()`
(logging OK / NOT FOUND), then issues a `SyncWritePosEx` to drive both axes to
center (2047) at speed 300, accel 10. After this point both servos hold center.

---

## Part 3 — The Main Loop, Step by Step

`loop()` runs continuously. Each iteration performs nine operations in order:

```c
serialCtrl();                 // 1. parse USB JSON
serial2Ctrl();                // 2. parse RPi JSON
server.handleClient();        // 3. service one HTTP request if pending
getGimbalFeedback();          // 4. poll both servos (pos/spd/load/V/I/temp/mode)
gimbalSteady(steadyGoalY);    // 5. if steady mode on, level the tilt vs IMU pitch
updateIMUData();              // 6. read+fuse IMU → roll/pitch/yaw + raw vectors
if (baseFeedbackFlow) baseInfoFeedback();  // 7. stream telemetry if enabled
oledInfoUpdate();             // 8. refresh OLED voltage line every 10 s
heartBeatCtrl();              // 9. freeze gimbal if commands went silent
```

There are **no `delay()` calls** anywhere in this loop. Steps 7 and 8 are
internally rate-limited by `millis()` comparisons (100 ms and 10 s respectively),
so they do real work only occasionally; every other step runs every pass. Loop
cadence is therefore governed mainly by the half-duplex servo reads in step 4
(~1 ms each) plus whatever parsing/IMU work is pending.

---

## Part 4 — Command Path: From Wire to Servo

This is the heart of the system. All three input channels funnel into the same
dispatcher, `jsonCmdReceiveHandler()` in `uart_ctrl.h`.

### 4.1 The three readers

- **`serialCtrl()`** accumulates USB bytes into a `static String` until `'\n'`,
  then `deserializeJson()` into `jsonCmdReceive`. On success it optionally echoes
  the raw line (if `InfoPrint==1 && uartCmdEcho`) and calls the handler.
- **`serial2Ctrl()`** does the identical thing for the RPi UART, echoing back to
  `Serial2` instead.
- **`webCtrlServer()`** registers `/js`: it deserializes `server.arg(0)` directly
  into `jsonCmdReceive`, calls the handler, then serializes `jsonInfoHttp` into
  the HTTP response body (`text/plain`) and clears the buffers.

All three parse into the **same** `StaticJsonDocument<512> jsonCmdReceive`, so
only one command is in flight at a time within a loop pass — there is no queue.

### 4.2 The dispatcher

`jsonCmdReceiveHandler()` reads the integer field `"T"` and runs a giant
`switch`. Each case pulls its parameters straight out of `jsonCmdReceive` and
calls the matching function. Motion commands (133/134/137/141) additionally do:

```c
heartbeatStopFlag = false;
lastCmdRecvTime   = millis();   // pet the watchdog
```

### 4.3 Responses

Two output mechanisms exist:

- **`jsPrint(msg)`** writes the same string to **both** `Serial` and `Serial2`,
  so a command arriving on USB still produces a reply the RPi can see, and vice
  versa. Used by telemetry, IMU data, WiFi status, etc.
- **HTTP** responses go back only in the `/js` reply body, built from
  `jsonInfoHttp`.

So a single `jsonInfoHttp` document is the shared scratch buffer that every
command fills in; serial/RPi commands stringify it through `jsPrint`, HTTP
commands return it in the response.

### 4.4 Worked example

`{"T":133,"X":45,"Y":0,"SPD":300,"ACC":0}` arriving on the RPi UART:

1. `serial2Ctrl()` reads it byte-by-byte until `\n`, parses it.
2. Handler sees `T=133` → resets heartbeat, calls
   `gimbalCtrlSimple(45, 0, 300, 0)`.
3. `gimbalCtrlSimple` clamps X to [−180,180] and Y to [−30,90], converts angles
   to raw steps (`pan = 2047 + map(45,…)`, `tilt = 2047 − map(0,…)`), clamps
   speed/accel, and issues `st.SyncWritePosEx(...)` over the 1 Mbit bus.
4. Both servos begin moving simultaneously to the new target.

---

## Part 5 — Gimbal Control Logic (`gimbal_module.h`)

### 5.1 Angle ↔ step conversion

```
pan  step = 2047 + map(X°, −180..+180  →  −2047..+2047)   # symmetric, full range
tilt step = 2047 − map(Y°,  −30..+90   →  −341..+1024)    # asymmetric, limited
```

`panAngleCompute()` / `tiltAngleCompute()` are the exact inverse maps, used when
the firmware needs to **report** the current angle from a raw encoder reading
(e.g. in telemetry and joystick hold). All conversions use `mapFloat()` (a float
version of Arduino `map`) so negative angles convert correctly — the original
code used integer `map()` over a 0–360 range, which mis-handled negatives.

### 5.2 The control functions

| Function | Behaviour |
|----------|-----------|
| `gimbalCtrlSimple(X,Y,SPD,ACC)` | Absolute move, both axes share one speed + accel. |
| `gimbalCtrlMove(X,Y,SX,SY)` | Absolute move with **independent per-axis raw speeds**, accel 0. |
| `gimbalCtrlStop()` | Reads current position, re-commands it at speed 0 → servo holds exactly where it is. No torque toggle, so no drift-then-jerk. |
| `gimbalSteadySet(s,y)` / `gimbalSteady(bias)` | Stabilisation: see below. |
| `gimbalUserCtrl(X,Y,SPD)` | Joystick stepping: see below. |

### 5.3 Servo feedback polling

`getGimbalFeedback()` runs every loop. For each servo it calls `st.FeedBack(id)`
(one half-duplex transaction) and, on success, fills a `ServoFeedback` struct:
`status, pos, speed, load, voltage, current, temper, mode`. On failure it sets
`status=false` and (if `InfoPrint`) emits a `T:1005` servo-error JSON. The cached
`gimbalFeedback[0]` (pan) and `[1]` (tilt) feed telemetry and the stop/hold
logic.

### 5.4 Stabilisation mode

When enabled via `{"T":137,"s":1,"y":<bias>}`, the loop calls
`gimbalSteady(steadyGoalY)` every pass, which commands:

```c
gimbalCtrlSimple(0, steadyGoalY - icm_pitch, 200, 10);
```

i.e. the tilt axis continuously counter-rotates against the IMU's measured body
pitch, holding the camera at `steadyGoalY` degrees relative to level. Pan is held
at 0 while steady mode is active. The bias is clamped to [−45, 90].

### 5.5 Joystick stepping (`gimbalUserCtrl`)

Takes `X,Y ∈ {−1,0,1,2}`. Combinations of −1/0/1 drive `goalX`/`goalY` toward the
axis limits (±180 pan, +90/−45 tilt); a `0` on an axis means "hold current," for
which it reads the live encoder position back through `panAngleCompute`/
`tiltAngleCompute` so the held angle matches reality. `X=2,Y=2` recenters. This
gives a press-and-hold directional control suitable for a UI joystick.

---

## Part 6 — IMU & Sensor Fusion (`IMU_ctrl.h` + `IMU.cpp`)

### 6.1 Sensors

- **QMI8658** — 6-axis accelerometer + gyroscope (I²C).
- **AK09918** — 3-axis magnetometer (I²C), run in continuous 100 Hz mode, with
  hard-iron offsets subtracted (`offset_x/y/z`, default `-12,0,0`).

### 6.2 Fusion

`imuDataGet()` reads accel, gyro, and (offset-corrected) magnetometer, then runs
`imuAHRSupdate()` — a **Mahony complementary filter** that maintains a unit
quaternion `(q0..q3)`. Gyro rates are converted from deg/s to rad/s (`×0.0175`),
accelerometer and magnetometer vectors are normalised, the estimated vs. measured
gravity/flux error drives a PI correction (`Kp=4.5, Ki=1.0`), and the quaternion
is integrated with `halfT=0.024` (≈48 ms assumed sample period) and renormalised
each step (using the classic `invSqrt` fast inverse-square-root). The quaternion
is then converted to Euler angles:

```
pitch = asin(...)·57.3      roll = atan2(...)·57.3      yaw = atan2(...)·57.3
```

These become `icm_pitch / icm_roll / icm_yaw`, consumed by stabilisation and
telemetry. `temp` is the ESP32 internal die temperature (`temperatureRead()`),
**not** an external sensor.

### 6.3 Known gap

The command-level calibration/offset functions exposed over JSON
(`T:127` calibrate, `T:128` get offset, `T:129` set offset) are **stubs** — they
reply `"... not implemented"`. A real magnetometer calibration routine
(`calibrateMagn()`) exists in `IMU.cpp` but is **not wired to any command**; the
offsets stay at their compiled-in defaults. So heading (yaw) is uncalibrated.

---

## Part 7 — Telemetry & Display

### 7.1 Feedback packets

| T-code | Source | Fields |
|--------|--------|--------|
| `1001` base info | `{"T":130}` once, `{"T":131,"cmd":1}` stream | `r,p,y, temp, v, pan, tilt` |
| `1002` IMU data | `{"T":126}` | `r,p,y, ax,ay,az, gx,gy,gz, mx,my,mz, temp` |
| `1005` servo error | automatic | `id, status` |

The continuous `1001` stream is gated by `feedbackFlowExtraDelay` (default 100 ms,
set via `{"T":142,"cmd":N}`), so it emits at ~10 Hz regardless of loop speed.
`pan`/`tilt` in the packet are computed from live encoder readings, so telemetry
reflects actual position, not the last command.

### 7.2 OLED (`oled_ctrl.h`)

128×32 SSD1306. Default view shows WiFi mode/SSID/IP on the top lines and a
voltage line refreshed every 10 s (`oledInfoUpdate()` reads INA219 then redraws).
`{"T":3,"lineNum":n,"Text":...}` switches to a custom 4-line view; `{"T":-3}`
restores default.

### 7.3 Battery (`battery_ctrl.h`)

INA219 at `0x42` provides shunt/bus voltage, current, power. `loadVoltage_V`
(bus + shunt) is the value shown on the OLED and reported as `v` in telemetry.
Missing chip → warning only, firmware continues.

---

## Part 8 — Safety: The Heartbeat Watchdog

`heartBeatCtrl()` runs every loop:

```c
if (millis() - lastCmdRecvTime > HEART_BEAT_DELAY) {   // default 3000 ms
    if (!heartbeatStopFlag) {
        gimbalCtrlStop();          // hold current position
        heartbeatStopFlag = true;  // do it once, not every pass
    }
}
```

Every motion command resets `lastCmdRecvTime` and clears `heartbeatStopFlag`. So
if the controlling host stops sending (link dropped, process crashed), the gimbal
**freezes in place** after the timeout rather than drifting or continuing a stale
motion. `HEART_BEAT_DELAY` is adjustable via `{"T":136,"cmd":<ms>}`.

---

## Part 9 — WiFi State Machine (`wifi_ctrl.h`)

On boot, `initWifi()` loads `/wifiConfig.json` from LittleFS and starts the
configured mode (defaulting to **AP** if the file is absent/invalid).

| `WIFI_MODE_ON_BOOT` | Action |
|---------------------|--------|
| 0 | Radio idle |
| 1 | AP only (`GimbalCtrl` / `12345678` @ 192.168.4.1) |
| 2 | STA — join a network; on 15 s timeout, fall back to AP |
| 3 | AP + STA simultaneously |

The first successful STA/AP+STA connection (when no config file existed yet)
auto-persists boot mode 3, so the device reconnects automatically next time.
`wifiStatusFeedback()` (`{"T":405}`) reports IP, RSSI, mode, SSIDs, and MAC over
`jsPrint`. **Note:** the connect path uses a blocking `while(WiFi.status()!=...)`
with `delay(500)` for up to 15 s — this is the one place outside `setup()` that
blocks, and only during an explicit WiFi command.

`WIFI_CURRENT_MODE` is an `int` (not `byte`) so the "−1 = connecting/failed"
sentinel works; as an unsigned byte it would have wrapped to 255 and broken the
OLED status switch.

---

## Part 10 — Flash Filesystem (`files_ctrl.h`)

LittleFS (`initFS()` mounts with format-on-fail). Beyond storing `wifiConfig.json`,
it exposes a generic line-oriented file API over JSON commands 200–208: scan,
create, read, delete, append line, insert line, replace line, read single line,
delete single line. The line-editing functions read the whole file into a
`String[]`, mutate, and rewrite — fine for small config files. `{"T":601}` reports
total/free flash bytes.

---

## Part 11 — Memory & Concurrency Model

- **Single-threaded.** Everything happens in `loop()` on one core; there are no
  tasks, ISRs, or queues for command handling. Order is deterministic.
- **Shared JSON buffers.** `jsonCmdReceive` (input) and `jsonInfoHttp` (output)
  are global `StaticJsonDocument<512>`. Because processing is synchronous within a
  pass, there is no contention, but it also means commands are handled strictly
  one at a time.
- **Cached state.** `gimbalFeedback[2]`, the `icm_*` angles, and `loadVoltage_V`
  are refreshed each loop and read by multiple consumers (telemetry, OLED, steady
  mode).

---

## Part 12 — What's Solid vs. What's Missing

**Solid / working:**
- Headless boot (no USB required), correct servo init.
- Three-channel unified command protocol with dual-broadcast replies.
- Full gimbal motion set + per-axis speed + hold + joystick + IMU stabilisation.
- Heartbeat freeze-on-silence safety.
- Mahony AHRS producing roll/pitch/yaw; battery, OLED, LED, file ops, WiFi.
- Web UI reaching every command.

**Missing / caveats:**
- IMU calibration/offset commands are stubs; `calibrateMagn()` exists but is
  unbound, so yaw is uncalibrated.
- WiFi STA connect blocks up to 15 s (by design, command-time only).
- No persistence of gimbal/PID/heartbeat settings across reboot (only WiFi
  config is saved).
- `mainType`/`moduleType` are hard-coded; `{"T":4}` changes only the reported
  value.
- Single 512-byte JSON buffer caps command/response size and prevents queuing.

---

## Part 13 — Build & Flash (reference)

PlatformIO env `esp32dev`, Arduino framework, 240 MHz, 4 MB flash, **LittleFS**
filesystem. Registry deps: ArduinoJson 6.21+, Adafruit SSD1306/GFX/BusIO,
INA219_WE. SCServo is vendored in `PanTilt_Firmware/lib/SCServo/`. Upload 230,400;
monitor 115,200. The `data/` folder (containing `wifiConfig.json`) is flashed as
the LittleFS image. See REPORT_8 (PlatformIO) and REPORT_3 (Arduino IDE) for
step-by-step flashing.

---

*Generated 2026-06-14. Describes `PanTilt_Firmware/` as currently committed.*
