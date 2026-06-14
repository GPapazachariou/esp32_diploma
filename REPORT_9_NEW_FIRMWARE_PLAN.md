# REPORT_9: New Pan-Tilt Firmware — Complete Implementation Guide

---

## What This Report Is

This is a step-by-step blueprint to create a **brand new, independent firmware** for the ESP32 pan-tilt gimbal module. It covers:

1. What the new firmware does and why we are creating it fresh
2. What is wrong with the current code and what we are fixing
3. Every file to create or modify, with exact before/after code snippets
4. How to compile, flash, and verify everything works

You do not need to have read any earlier reports. Everything you need is here.

---

## Part 1 — What We Are Building and Why

### The hardware setup

We have an **ESP32 microcontroller** controlling a pan-tilt camera gimbal — two motorised joints that rotate left/right (pan) and up/down (tilt). The ESP32 is the brain. It talks to the motors, reads sensors, serves a web page, and accepts commands from external computers.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       HARDWARE OVERVIEW                                 │
│                                                                         │
│  ┌─────────────┐   USB cable          ┌────────────────┐               │
│  │  PC /       │  ──────────────────► │                │               │
│  │  Laptop     │   115200 baud        │                │   GPIO 18/19  │
│  └─────────────┘                      │   ESP32        │ ────────────► PAN SERVO
│                                       │   (the brain)  │               │
│  ┌─────────────┐   GPIO UART          │                │   GPIO 18/19  │
│  │  Raspberry  │  ──────────────────► │                │ ────────────► TILT SERVO
│  │  Pi         │   GPIO 16/17         │                │               │
│  └─────────────┘   115200 baud        └────────────────┘               │
│                                              │
│  ┌─────────────┐   WiFi               ┌─────┴──────┐
│  │  Phone /    │  ──────────────────► │  I2C Bus   │
│  │  Browser    │   192.168.4.1        │ GPIO 32/33 │
│  └─────────────┘                      └─────┬──────┘
│                                ┌─────────────┼─────────────┐
│                             ┌──┴──┐       ┌──┴──┐       ┌──┴──┐
│                             │ IMU │       │OLED │       │BAT  │
│                             └─────┘       └─────┘       └─────┘
└─────────────────────────────────────────────────────────────────────────┘
```

### The three ways to send commands

| Source | Connection | Baud | Purpose |
|--------|-----------|------|---------|
| PC / laptop | USB cable → GPIO 0/1 | 115,200 | Development, testing, debug output |
| Raspberry Pi | GPIO wires → GPIO 16/17 | 115,200 | Runtime control from RPi in deployment |
| Phone / browser | WiFi HTTP → port 80 | — | Web interface, joystick control |

All three send the same JSON format, e.g. `{"T":133,"X":45,"Y":0,"SPD":300,"ACC":0}` followed by a newline. They all reach the same command handler inside the ESP32.

### Why we are creating a new firmware instead of editing the old one

The existing code in `General_Driver/` is a **combined firmware** that supports three different robot hardware variants (wheeled rover, robot arm, gimbal). It has:

- ~3,500 lines of code across 20+ files
- 27 documented bugs, including 5 critical ones that completely halt the device
- The most important critical bug: `while(!Serial) {}` on line 103 of `General_Driver.ino` — this line waits forever for a USB cable to be plugged in. Without USB, the servo initialisation on GPIO 18/19 never runs, so the gimbal servos never respond. The device is **unusable** on battery power alone.
- No support for a second UART channel (the Raspberry Pi GPIO connection does not exist yet in any code)

The new firmware (`PanTilt_Firmware/`) will be:
- ~800 lines total
- Gimbal-only — no motor, arm, or ESP-NOW code
- Non-blocking — no `delay()` calls in the main loop
- Two JSON input channels: USB serial and RPi GPIO UART
- Boots and works without a USB cable

---

## Part 2 — The Three UART Channels Explained

The ESP32 has three independent hardware serial ports (UARTs). We use all three:

```
┌──────────────────────────────────────────────────────────────────────┐
│                    ESP32 UART ASSIGNMENTS                            │
│                                                                      │
│  UART0  │  Serial   │  GPIO 0 (RX), GPIO 1 (TX)  │  115,200 baud   │
│         │           │  connected via USB CP2102 chip to PC          │
│         │           │  Purpose: JSON commands from PC + debug output│
│                                                                      │
│  UART1  │  Serial1  │  GPIO 18 (RX), GPIO 19 (TX) │  1,000,000 baud│
│         │           │  connected directly to both servo motors      │
│         │           │  Purpose: servo bus (half-duplex SMS_STS)     │
│         │           │  *** NEVER used for text/JSON output ***      │
│                                                                      │
│  UART2  │  Serial2  │  GPIO 16 (RX), GPIO 17 (TX) │  115,200 baud  │
│         │           │  wired to Raspberry Pi GPIO pins              │
│         │           │  Purpose: JSON commands from Raspberry Pi     │
└──────────────────────────────────────────────────────────────────────┘
```

**Why GPIO 16 and 17 for the Raspberry Pi?**
These pins were previously used for motor encoder inputs. Since we are removing the wheel motor code entirely, these pins are freed. They happen to be the default Serial2 pins on the ESP32, so no special configuration is needed.

**Wiring the Raspberry Pi to the ESP32:**
```
Raspberry Pi   →   ESP32
GPIO 14 (TX)  →   GPIO 16 (RX)   ← RPi talks, ESP32 listens
GPIO 15 (RX)  →   GPIO 17 (TX)   ← ESP32 talks, RPi listens
GND           →   GND             ← common ground is mandatory
```
The voltage levels match: RPi GPIO is 3.3 V and ESP32 GPIO is also 3.3 V — direct connection is safe.

---

## Part 3 — New Firmware Directory Structure

We create a new sketch folder **inside** the existing repo. The original `General_Driver/` is left completely untouched.

```
esp32_diploma/
├── General_Driver/                 ← ORIGINAL, untouched
├── REPORT_9_NEW_FIRMWARE_PLAN.md   ← this document
│
└── PanTilt_Firmware/               ← NEW firmware
    │
    ├── PanTilt_Firmware.ino        ← main entry point (NEW, ~80 lines)
    │
    ├── ugv_config.h                ← COPIED + STRIPPED (remove arm/motor vars)
    ├── gimbal_module.h             ← COPIED + FIXED (6 bugs corrected)
    ├── uart_ctrl.h                 ← COPIED + STRIPPED + EXTENDED (add serial2Ctrl)
    ├── json_cmd.h                  ← COPIED + STRIPPED (remove unused #defines)
    ├── ugv_advance.h               ← COPIED + STRIPPED (remove mission functions)
    ├── IMU_ctrl.h                  ← COPIED + FIXED (empty stubs get responses)
    ├── wifi_ctrl.h                 ← COPIED + FIXED (2 logic bugs)
    ├── http_server.h               ← COPIED + FIXED (MIME type typo)
    │
    ├── oled_ctrl.h                 ← COPIED UNCHANGED
    ├── battery_ctrl.h              ← COPIED UNCHANGED
    ├── files_ctrl.h                ← COPIED UNCHANGED
    ├── ugv_led_ctrl.h              ← COPIED UNCHANGED
    ├── IMU.h / IMU.cpp             ← COPIED UNCHANGED
    ├── QMI8658.h / QMI8658.cpp     ← COPIED UNCHANGED
    ├── AK09918.h / AK09918.cpp     ← COPIED UNCHANGED
    ├── QMI8658reg.h                ← COPIED UNCHANGED
    ├── web_page.h                  ← COPIED UNCHANGED (existing web UI)
    │
    └── data/
        └── wifiConfig.json         ← NEW default config (AP mode, "GimbalCtrl")
```

The `SCServo/` library at `esp32_diploma/SCServo/` is shared by both firmware variants — do not copy or move it.

---

## Part 4 — File 1: `PanTilt_Firmware.ino` (create from scratch)

This is the main entry point. It replaces `General_Driver.ino`.

Two functions: `setup()` which runs once at boot, and `loop()` which runs forever.

### The `setup()` function — annotated

```cpp
#include <ArduinoJson.h>
StaticJsonDocument<512> jsonCmdReceive;
StaticJsonDocument<512> jsonInfoSend;
StaticJsonDocument<512> jsonInfoHttp;

#include <SCServo.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_SSD1306.h>
#include <INA219_WE.h>
#include <math.h>

#include "battery_ctrl.h"
#include "oled_ctrl.h"
#include "ugv_config.h"
#include "ugv_led_ctrl.h"
#include "gimbal_module.h"
#include "json_cmd.h"
#include "IMU_ctrl.h"
#include "files_ctrl.h"
#include "ugv_advance.h"
#include "wifi_ctrl.h"
#include "uart_ctrl.h"
#include "http_server.h"
#include "web_page.h"

byte mainType   = 1;   // WAVE ROVER board (hard-coded)
byte moduleType = 2;   // gimbal module (hard-coded)

void setup() {
  Serial.begin(115200);
  // NO while(!Serial){} here — that was the critical bug that prevented battery-only boot.

  Serial2.begin(115200, SERIAL_8N1, RPi_RX_PIN, RPi_TX_PIN);
  // RPi_RX_PIN = 16, RPi_TX_PIN = 17 (defined in ugv_config.h)

  Wire.begin(S_SDA, S_SCL);    // I2C for OLED, IMU, battery (GPIO 32/33)

  ina219_init();
  inaDataUpdate();

  init_oled();
  screenLine_0 = "Gimbal Controller";
  screenLine_1 = "version 1.0";
  screenLine_2 = "starting...";
  screenLine_3 = "";
  oled_update();
  delay(1000);

  imu_init();
  led_pin_init();

  screenLine_3 = "Init LittleFS";
  oled_update();
  initFS();

  screenLine_3 = "Init servo bus";
  oled_update();
  gimbalServoInit();
  // Inits Serial1 at 1 Mbit/s on GPIO 18/19 and centres both servos

  screenLine_3 = "WiFi init";
  oled_update();
  initWifi();

  initHttpWebServer();
  updateOledWifiInfo();

  if (InfoPrint == 1) { Serial.println("Gimbal firmware ready."); }
}
```

### The `loop()` function — annotated

```cpp
void loop() {
  serialCtrl();          // read JSON from USB (PC)
  serial2Ctrl();         // read JSON from GPIO UART (Raspberry Pi)
  server.handleClient(); // serve WiFi HTTP requests
  getGimbalFeedback();   // query servo positions (~1 ms, non-blocking)
  gimbalSteady(steadyGoalY); // apply stabilisation if enabled
  updateIMUData();       // read roll/pitch/yaw
  oledInfoUpdate();      // refresh OLED every 10s
  heartBeatCtrl();       // hold position if no command for 3s
}
```

**Every function in `loop()` returns immediately** — no `delay()`, no `while()` loops.

---

## Part 5 — File 2: `ugv_config.h` (copy and strip)

Copy from `General_Driver/ugv_config.h`. Remove these blocks:

| Block to remove | Why |
|-----------------|-----|
| Arm servo ID defines (`BASE_SERVO_ID` 11–15, `SHOULDER_DRIVING_SERVO_ID`, etc.) | No arm |
| All IK variables (`l1`, `l2A`, `goalX/Y/Z/T`, `radB/S/E`, `nanIK`, `delta_x/y`, `beta_x/y`, etc.) | No arm kinematics |
| All motor/encoder pin defines (`PWMA=25`, `AIN1=21`, `AIN2=17`, `BIN1=22`, `BIN2=23`, `PWMB=26`, `AENCA/B`, `BENCA/B`) | No motors |
| Motor PID variables (`__kp`, `__ki`, `__kd`, `windup_limits`, `THRESHOLD_PWM`) | No motor PID |
| Wheel parameters (`WHEEL_D`, `ONE_CIRCLE_PLUSES`, `TRACK_WIDTH`, `SET_MOTOR_DIR`) | No wheels |
| ESP-NOW variables (`espNowMode`, `ctrlByBroadcast`, `mac_whitelist_broadcast`, `runNewJsonCmd`) | No ESP-NOW |
| Arm flag variables (`RoArmM2_torqueLock`, `emergencyStopFlag`, `newCmdReceived`, `RoArmM2_initCheckSucceed`) | No arm |
| Constant-move variables (`const_spd`, `const_mode`, `const_cmd_*`, `const_goal_*`, `EEMode`) | No arm constant-move |
| Arm servo sync arrays (`servoID[5]`, `goalPos[5]`, `moveSpd[5]`, `moveAcc[5]`) | No arm |
| Arm joint limit defines (`ARM_BASE_LIMIT_MIN_RAD`, etc.) and all `ARM_L*` constants | No arm |
| Arm joint angle/rad variables (`BASE_JOINT_RAD`, `SHOULDER_JOINT_RAD`, etc.) | No arm |
| Motor PWM freq/channels (`freq`, `channel_A`, `channel_B`) | No motors |
| `BASE_JOINT`, `SHOULDER_JOINT`, `ELBOW_JOINT`, `EOAT_JOINT` defines | No arm |
| EoAT variables (`EoAT_A/B`, `l4A/B`, `lEA/EB/E`, `tErad`, `ARM_L4_*`) | No arm |
| Duplicate `#define RoArmM2_Servo_RXD 18` / `RoArmM2_Servo_TXD 19` at top | Replaced by `S_RXD`/`S_TXD` |
| `MAX_SERVO_ID` define | No arm servo scan |

**Add these lines** (new):
```cpp
#define RPi_RX_PIN 16   // Raspberry Pi TX → ESP32 RX (freed from BENCB encoder pin)
#define RPi_TX_PIN 17   // ESP32 TX → Raspberry Pi RX (freed from AIN2 motor pin)

bool heartbeatStopFlag = false;  // prevents repeated gimbalCtrlStop() calls
```

**Keep everything else**, especially: `InfoPrint`, `steadyMode`, `baseFeedbackFlow`, `S_RXD`/`S_TXD` (GPIO 18/19), `S_SCL`/`S_SDA` (GPIO 33/32), `GIMBAL_PAN_ID=2`, `GIMBAL_TILT_ID=1`, `SERVO_STOP_DELAY`, `HEART_BEAT_DELAY`, `lastCmdRecvTime`, `IO4_PIN`/`IO5_PIN`, `IO4_CH`/`IO5_CH`, `ST_PID_*_ADDR` defines, `ST_TORQUE_MAX/MIN`, `feedbackFlowExtraDelay`, `uartCmdEcho`, `prev_time`, `jsonFeedbackWeb`, `thisMacStr`, `icm_pitch/roll/yaw/temp`, `last_imu_update`, IMU quaternion/accel/mag/gyro vars, `FREQ`, `ANALOG_WRITE_BITS`, `MAX_PWM`, `MIN_PWM`.

---

## Part 6 — File 3: `gimbal_module.h` (copy and fix 6 bugs + add servo infrastructure)

Copy from `General_Driver/gimbal_module.h`. Apply all fixes below. Also add servo infrastructure (moved from deleted `RoArm-M2_module.h`) at the **top** of the file.

### Servo infrastructure (add at top of file)

```cpp
SMS_STS st;  // servo bus object — all servo communication goes through this

struct ServoFeedback {
  bool  status;    // true = servo responded, false = no response
  s16   pos;       // position: 0-4095, 2047 = centre
  s16   speed;
  s16   load;
  float voltage;
  float current;
  float temper;
  byte  mode;
};

void servoTorqueCtrl(u8 id, u8 enable) {
  st.EnableTorque(id, enable);
}

void gimbalServoInit() {
  Serial1.begin(1000000, SERIAL_8N1, S_RXD, S_TXD);
  st.pSerial = &Serial1;
  delay(500);  // servo power-up (only during setup, not in loop)

  if (st.FeedBack(GIMBAL_PAN_ID) != -1) {
    if (InfoPrint == 1) { Serial.println("PAN servo OK"); }
  } else {
    if (InfoPrint == 1) { Serial.println("PAN servo NOT FOUND"); }
  }

  if (st.FeedBack(GIMBAL_TILT_ID) != -1) {
    if (InfoPrint == 1) { Serial.println("TILT servo OK"); }
  } else {
    if (InfoPrint == 1) { Serial.println("TILT servo NOT FOUND"); }
  }

  s16 pos[2] = {2047, 2047};
  u16 spd[2] = {300, 300};
  u8  acc[2] = {10, 10};
  u8  ids[2] = {GIMBAL_PAN_ID, GIMBAL_TILT_ID};
  st.SyncWritePosEx(ids, 2, pos, spd, acc);
}

void changeID(byte rawID, byte newID) {
  st.unLockEprom(rawID);
  st.writeByte(rawID, SMS_STS_ID, newID);
  st.LockEprom(newID);
}

void setMiddlePos(byte id) {
  st.CalibrationOfs(id);
}

void setServosPID(byte id, byte p) {
  st.unLockEprom(id);
  st.writeByte(id, ST_PID_P_ADDR, p);
  st.LockEprom(id);
}
```

### Bug Fix 1 — Forward reference compile error

`gimbalCtrlStop()` calls `getGimbalFeedback()` but `getGimbalFeedback()` was defined 50 lines later — compile error in `.h` files.

**Fix:** Move `getGimbalFeedback()` definition to appear **before** `gimbalCtrlStop()`.

### Bug Fix 2 — Wrong array on servo failure (Report 2, Bug #19)

In `getGimbalFeedback()` failure branches:
```cpp
// BEFORE (writes to arm servo array — corrupts unrelated data):
servoFeedback[0].status = false;   // PAN failure path
servoFeedback[1].status = false;   // TILT failure path

// AFTER (writes to gimbal array — correct):
gimbalFeedback[0].status = false;
gimbalFeedback[1].status = false;
```

### Bug Fix 3 — Map formula wrong for negative angles (Report 2, Bug #5)

In `gimbalCtrlSimple()` and `gimbalCtrlMove()`:
```cpp
// BEFORE (range 0-360, input is -180 to +180 → negative inputs produce wrong positions):
gimbalPos[0] = 2047 + (int)round(map(Xinput, 0, 360, 0, 4095));
gimbalPos[1] = 2047 - (int)round(map(Yinput, 0, 360, 0, 4095));
gimbalSpd[0] = (int)round(map(spdInput, 0, 360, 0, 4095));

// AFTER (correct signed range, float-safe, speed passed directly):
gimbalPos[0] = 2047 + (int)round(mapFloat(Xinput, -180.0f, 180.0f, -2047.0f, 2047.0f));
gimbalPos[1] = 2047 - (int)round(mapFloat(Yinput,  -30.0f,  90.0f,  -341.0f, 1024.0f));
gimbalSpd[0] = (u16)constrain((int)spdInput, 0, 4095);
gimbalSpd[1] = (u16)constrain((int)spdInput, 0, 4095);
gimbalAcc[0] = (u8)constrain((int)accInput, 0, 254);
gimbalAcc[1] = (u8)constrain((int)accInput, 0, 254);
```

Apply the same position formula fix to `gimbalCtrlMove()`.

### Bug Fix 4 — `gimbalCtrlStop()` releases torque instead of holding (Report 2, Bug #18)

```cpp
// BEFORE (3ms torque-off causes drift, then re-engage to wrong position):
void gimbalCtrlStop() {
  st.EnableTorque(GIMBAL_PAN_ID, 0);
  st.EnableTorque(GIMBAL_TILT_ID, 0);
  delay(SERVO_STOP_DELAY);
  st.EnableTorque(GIMBAL_PAN_ID, 1);
  st.EnableTorque(GIMBAL_TILT_ID, 1);
}

// AFTER (read current position, command it back at speed=0 → holds in place):
void gimbalCtrlStop() {
  getGimbalFeedback();
  gimbalPos[0] = gimbalFeedback[0].status ? gimbalFeedback[0].pos : 2047;
  gimbalPos[1] = gimbalFeedback[1].status ? gimbalFeedback[1].pos : 2047;
  gimbalSpd[0] = 0;
  gimbalSpd[1] = 0;
  gimbalAcc[0] = 0;
  gimbalAcc[1] = 0;
  st.SyncWritePosEx(gimbalID, 2, gimbalPos, gimbalSpd, gimbalAcc);
}
```

### Bug Fix 5 — `delay(5)` in `gimbalUserCtrl()` (Report 2, Bug #17)

```cpp
// BEFORE (up to 10ms blocking per joystick release — freezes serial + WiFi):
if (inputX == 0) {
  servoTorqueCtrl(GIMBAL_PAN_ID, 0);
  delay(5);
  servoTorqueCtrl(GIMBAL_PAN_ID, 1);
  getGimbalFeedback();
  goalX = panAngleCompute(gimbalFeedback[0].pos);
}
if (inputY == 0) {
  servoTorqueCtrl(GIMBAL_TILT_ID, 0);
  delay(5);
  servoTorqueCtrl(GIMBAL_TILT_ID, 1);
  getGimbalFeedback();
  goalY = tiltAngleCompute(gimbalFeedback[1].pos);
}

// AFTER (single non-blocking read, no torque toggle):
if (inputX == 0 || inputY == 0) {
  getGimbalFeedback();
}
if (inputX == 0) { goalX = panAngleCompute(gimbalFeedback[0].pos); }
if (inputY == 0) { goalY = tiltAngleCompute(gimbalFeedback[1].pos); }
```

### Bug Fix 6 — `panAngleCompute()` / `tiltAngleCompute()` wrong formula

These must be the exact inverse of the fixed map in Bug Fix 3:

```cpp
// BEFORE (inconsistent with corrected gimbalCtrlSimple):
float panAngleCompute(int inputPos) {
  return mapFloat((inputPos - 2047), 0, 4095, 0, 360);
}
float tiltAngleCompute(int inputPos) {
  return mapFloat((2047 - inputPos), 0, 4095, 0, 360);
}

// AFTER (correct inverse):
float panAngleCompute(int inputPos) {
  return mapFloat((float)(inputPos - 2047), -2047.0f, 2047.0f, -180.0f, 180.0f);
}
float tiltAngleCompute(int inputPos) {
  return mapFloat((float)(2047 - inputPos), -1024.0f, 341.0f, -90.0f, 30.0f);
}
```

---

## Part 7 — File 4: `uart_ctrl.h` (copy, strip, and extend)

Copy from `General_Driver/uart_ctrl.h`. Make four sets of changes.

### Change 1 — Add `jsPrint()` helper at the top

```cpp
void jsPrint(const String& msg) {
  Serial.println(msg);
  Serial2.println(msg);
}
```

Use `jsPrint()` instead of `Serial.println()` wherever JSON responses are sent (feedback functions, status replies).

### Change 2 — Add `serial2Ctrl()` at the bottom (mirrors `serialCtrl()`)

```cpp
void serial2Ctrl() {
  static String receivedData2;
  while (Serial2.available() > 0) {
    char c = Serial2.read();
    receivedData2 += c;
    if (c == '\n') {
      DeserializationError err = deserializeJson(jsonCmdReceive, receivedData2);
      if (err == DeserializationError::Ok) {
        if (InfoPrint == 1 && uartCmdEcho) { Serial2.print(receivedData2); }
        jsonCmdReceiveHandler();
      }
      receivedData2 = "";
    }
  }
}
```

### Change 3 — Strip command switch (remove dead cases)

Delete these case groups entirely:

| T codes | What they controlled | Why removed |
|---------|---------------------|-------------|
| T:1, T:2, T:11, T:13 | Motor speed, PWM, PID, ROS | No motors |
| T:100–125 | Robot arm (IK, joints, end-effector) | No arm |
| T:123 | Constant/continuous arm movement | No arm |
| T:138–140 | Motor speed rate | No motors |
| T:144 | Arm UI control | No arm |
| T:115 (`CMD_EOAT_TYPE`), T:116 (`CMD_CONFIG_EOAT`) | End-effector type | No arm |
| T:220–242 | Mission scripting (create, play, edit steps) | No missions |
| T:300–306 | ESP-NOW multi-robot radio | No ESP-NOW |
| T:602, T:603 | Boot mission info / reset | No missions |
| T:900 | Robot type config (`mm_settings`) | Hard-coded in ino |

**Keep** all gimbal, IMU, feedback, OLED, LED, file ops, WiFi, servo settings, and system commands (see plan for full list).

### Change 4 — Fix `heartBeatCtrl()` (Bug #27 — undefined `currentTimeMillis`)

```cpp
// BEFORE (undefined variable — undefined behavior):
if (currentTimeMillis - lastCmdRecvTime > HEART_BEAT_DELAY) { ... }

// AFTER (correct):
void heartBeatCtrl() {
  if (millis() - lastCmdRecvTime > (unsigned long)HEART_BEAT_DELAY) {
    if (!heartbeatStopFlag) {
      gimbalCtrlStop();
      heartbeatStopFlag = true;
    }
  }
}
```

Add `heartbeatStopFlag = false; lastCmdRecvTime = millis();` at the start of gimbal movement cases (T:133, T:134, T:137, T:141) to reset the timer on each command.

---

## Part 8 — File 5: `json_cmd.h` (copy and strip)

Copy from `General_Driver/json_cmd.h`. Delete all `#define` entries for command T-codes removed from `uart_ctrl.h`. Keep only defines that have a matching `case` (~25 remain from ~90).

---

## Part 9 — File 6: `ugv_advance.h` (copy and strip)

Copy from `General_Driver/ugv_advance.h`. Remove:
- All mission functions: `createMission`, `missionContent`, `appendStepJson/FB`, `appendDelayCmd`, `insertStepJson/FB`, `insertDelayCmd`, `replaceStepJson/FB`, `replaceDelayCmd`, `deleteStep`, `moveToStep`, `missionPlay`, `serialMissionAbort`
- `configEEmodeType`, `configEoAT`, `saveSpdRate`, `constantHandle`, `constantCtrl`
- Motor/arm data fields from `baseInfoFeedback()` (`speedGetA/B`, `radB/S/E/G`, `lastX/Y/Z/T`, arm torque)

Simplify `baseInfoFeedback()` to gimbal-only output:
```json
{"T":1001,"r":0.0,"p":-2.3,"y":180.5,"temp":32.1,"v":12.4,"pan":45.0,"tilt":10.0}
```

Change `Serial.println(getInfoJsonString)` in `baseInfoFeedback()` to `jsPrint(getInfoJsonString)`.

**Keep**: `configInfoPrint`, `setBaseInfoFeedbackMode`, `baseInfoFeedback`, `changeModuleType`, `setFeedbackFlowInterval`, `setCmdEcho`, `changeHeartBeatDelay`.

---

## Part 10 — File 7: `wifi_ctrl.h` (copy and fix 2 bugs + add MAC utilities)

Copy from `General_Driver/wifi_ctrl.h`.

### Bug Fix A — Config save condition always true (Report 2, Bug #11)

```cpp
// BEFORE (always true for any integer value):
if (WIFI_MODE_ON_BOOT != 0 || WIFI_MODE_ON_BOOT != -1) {

// AFTER:
if (WIFI_MODE_ON_BOOT != 0 && WIFI_MODE_ON_BOOT != -1) {
```

### Bug Fix B — Unsigned type used as signed sentinel (Report 6, Bug 9)

```cpp
// BEFORE (byte is 0-255, -1 becomes 255):
byte WIFI_CURRENT_MODE = -1;

// AFTER:
int WIFI_CURRENT_MODE = -1;
```

### Add MAC utilities (moved from deleted `esp_now_ctrl.h`)

Add these three items (needed by `wifiStatusFeedback()`):
```cpp
uint8_t thisDevMac[6];

void macToString(uint8_t* mac, String& outStr) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  outStr = String(buf);
}

void getThisDevMacAddress() {
  WiFi.macAddress(thisDevMac);
  macToString(thisDevMac, thisMacStr);
}
```

Change `Serial.println(getInfoJsonString)` in `wifiStatusFeedback()` to `jsPrint(getInfoJsonString)`.

---

## Part 11 — File 8: `http_server.h` (copy and fix 1 bug)

Copy from `General_Driver/http_server.h`.

```cpp
// BEFORE (invalid MIME type):
server.send(200, "text/plane", response);

// AFTER:
server.send(200, "text/plain", response);
```

---

## Part 12 — File 9: `IMU_ctrl.h` (copy and fix empty stubs)

Copy from `General_Driver/IMU_ctrl.h`.

```cpp
// BEFORE (empty — caller gets no feedback, looks like success):
void imuCalibration() {}
void getIMUOffset() {}
void setIMUOffset(int16_t x, int16_t y, int16_t z) {}

// AFTER (returns JSON error so caller knows):
void imuCalibration() {
  Serial.println("{\"T\":1002,\"info\":\"IMU calibration not implemented\"}");
}
void getIMUOffset() {
  Serial.println("{\"T\":1002,\"info\":\"IMU offset get not implemented\"}");
}
void setIMUOffset(int16_t x, int16_t y, int16_t z) {
  Serial.println("{\"T\":1002,\"info\":\"IMU offset set not implemented\"}");
}
```

Also change `Serial.println(getInfoJsonString)` in `getIMUData()` to `jsPrint(getInfoJsonString)`.

---

## Part 13 — File 10: `data/wifiConfig.json`

```json
{
  "wifi_mode_on_boot": 1,
  "ap_ssid": "GimbalCtrl",
  "ap_password": "12345678",
  "sta_ssid": "",
  "sta_password": ""
}
```

Mode 1 = AP-only. ESP32 creates hotspot `GimbalCtrl` / `12345678`. Browse to `http://192.168.4.1`.
To connect to a router: `{"T":403,"ssid":"YourWiFi","password":"YourPass"}`.

---

## Part 14 — Phase 3: OLED and WiFi Joystick (later stage)

### OLED enhancement

In `wifi_ctrl.h`, change `updateOledWifiInfo()` to show gimbal-relevant info:
```cpp
screenLine_0 = String("AP:") + AP_SSID;
screenLine_1 = String("PWD:") + AP_PASS;
screenLine_2 = String("IP:") + WiFi.softAPIP().toString();
screenLine_3 = String("V:") + String(loadVoltage_V, 2);
```

Result on OLED:
```
AP:GimbalCtrl
PWD:12345678
IP:192.168.4.1
V:12.34
```

### WiFi joystick

The existing `web_page.h` already has 8-directional buttons sending `T:141` commands — works out of the box. Future enhancement: add analog `<input type="range">` sliders mapped to `T:133`/`T:134`.

---

## Part 15 — Arduino IDE Setup

### Libraries needed (reduced from original)

**Install via Library Manager (Tools → Manage Libraries):**
| Library | Note |
|---------|------|
| ArduinoJson by Benoit Blanchon (6.x) | required |
| Adafruit SSD1306 | required |
| Adafruit BusIO | auto-installed with SSD1306 |
| INA219_WE by Wolfgang Ewald | required |

**No longer needed — do NOT install:**
- `ESP32Encoder` — encoders removed
- `PID_v2` — motor PID removed
- `SimpleKalmanFilter` — motor filter removed
- `Adafruit ICM20X` / `Adafruit ICM20948` — replaced by native QMI8658/AK09918 drivers
- `Adafruit Unified Sensor` — only needed for ICM

**Install manually:**
Copy `esp32_diploma/SCServo/` to `Documents/Arduino/libraries/SCServo/`

### Board settings

| Setting | Value |
|---------|-------|
| Board | ESP32 Dev Module |
| Upload Speed | 921600 |
| CPU Frequency | 240 MHz (WiFi/BT) |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Default 4MB with spiffs |
| PSRAM | Disabled |

Open sketch: **File → Open → `esp32_diploma/PanTilt_Firmware/PanTilt_Firmware.ino`**

---

## Part 16 — Verification Checklist

### Test 1 — Boot without USB (critical fix)

Disconnect USB. Power from battery. Wait 5 seconds.

**Pass:** OLED shows startup screen, then WiFi info. Servos move to centre.
**Fail:** Freezes on first OLED screen → `while(!Serial){}` was not removed.

### Test 2 — USB JSON moves gimbal

Send over serial monitor (115200 baud):
```
{"T":133,"X":45,"Y":0,"SPD":300,"ACC":0}
```
**Pass:** PAN servo rotates to 45° right.

### Test 3 — Negative angles work

Send: `{"T":133,"X":-90,"Y":-15,"SPD":300,"ACC":0}`
**Pass:** PAN goes 90° LEFT, TILT goes 15° DOWN.
**Fail:** Servo slams to extreme → map formula not fixed.

### Test 4 — Stop holds position

1. Send `{"T":133,"X":60,"Y":30,"SPD":200,"ACC":0}`
2. Send `{"T":135}` while moving

**Pass:** Holds exactly where it stopped, no drift.
**Fail:** Drifts then jerks → `gimbalCtrlStop()` not fixed.

### Test 5 — Raspberry Pi UART commands

Wire: RPi GPIO 14 (TX) → ESP32 GPIO 16, RPi GPIO 15 (RX) → ESP32 GPIO 17, GND → GND.

On RPi (enable serial port first in `raspi-config`):
```bash
echo '{"T":133,"X":-45,"Y":0,"SPD":300,"ACC":0}' > /dev/serial0
```
**Pass:** Gimbal moves to -45° without USB connected.
**Fail:** Check wiring (TX↔RX must cross), check `Serial2.begin()` in setup(), check RPi serial enabled.

### Test 6 — WiFi web control

Connect to WiFi `GimbalCtrl` (password: `12345678`). Browse `http://192.168.4.1`. Click direction buttons.
**Pass:** Buttons move gimbal.
**Fail (no page):** Check `wifiConfig.json` was flashed to filesystem (LittleFS upload).
**Fail (buttons do nothing):** Check MIME type fix in `http_server.h`.

### Test 7 — Both USB and RPi simultaneously

Alternate commands from USB and RPi. Each should execute correctly.

### Test 8 — Feedback on both ports

Enable stream: `{"T":131,"cmd":1}` from USB.
Monitor RPi: `cat /dev/serial0`

**Pass:** `{"T":1001,...}` lines appear on both USB monitor and RPi serial.

---

## Part 17 — Implementation Order

```
Step  1  Create PanTilt_Firmware/ directory and data/ subdirectory
Step  2  Copy unchanged files (oled_ctrl.h, battery_ctrl.h, IMU.h/.cpp, etc.)
Step  3  Create data/wifiConfig.json
Step  4  Write ugv_config.h (stripped + RPi pin defines + heartbeatStopFlag)
Step  5  Write gimbal_module.h (servo infra + 6 bug fixes)
Step  6  Write uart_ctrl.h (jsPrint + serial2Ctrl + trimmed switch + heartBeatCtrl fix)
Step  7  Write json_cmd.h (trimmed defines)
Step  8  Write ugv_advance.h (stripped missions + jsPrint in baseInfoFeedback)
Step  9  Write wifi_ctrl.h (2 bug fixes + MAC utilities + jsPrint)
Step 10  Write http_server.h (MIME type fix)
Step 11  Write IMU_ctrl.h (stub responses + jsPrint in getIMUData)
Step 12  Write PanTilt_Firmware.ino (main sketch)
Step 13  Compile — fix errors
Step 14  Flash firmware (Upload button)
Step 15  Flash filesystem (Tools → ESP32 Sketch Data Upload)
Step 16  Run all 8 verification tests
```

---

## Summary of All Bugs Fixed

| # | File | Bug | Fix |
|---|------|-----|-----|
| 1 | `PanTilt_Firmware.ino` | `while(!Serial){}` hangs without USB | Removed entirely |
| 2 | `gimbal_module.h` | Forward reference compile error | Moved `getGimbalFeedback()` before `gimbalCtrlStop()` |
| 3 | `gimbal_module.h` | Wrong array on servo failure | `servoFeedback[]` → `gimbalFeedback[]` |
| 4 | `gimbal_module.h` | Map formula broken for negative angles | `mapFloat(X, -180, 180, -2047, 2047)` |
| 5 | `gimbal_module.h` | Stop releases then re-engages (drifts) | Read current pos, command it back at speed=0 |
| 6 | `gimbal_module.h` | 10ms blocking in joystick control | Removed torque toggle and `delay(5)` |
| 7 | `uart_ctrl.h` | `currentTimeMillis` undefined in heartbeat | Replaced with `millis()` |
| 8 | `wifi_ctrl.h` | Config save condition always true | `\|\|` → `&&` |
| 9 | `wifi_ctrl.h` | `byte WIFI_CURRENT_MODE = -1` wraps to 255 | Changed to `int` |
| 10 | `http_server.h` | MIME type `"text/plane"` | Corrected to `"text/plain"` |
| 11 | `IMU_ctrl.h` | Calibration commands silently do nothing | Added JSON error responses |

---

*End of REPORT_9.*
