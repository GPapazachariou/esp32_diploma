# Gimbal-Only Implementation — Change Report
**Branch:** `feature/gimbal-impl`  
**Base:** `main`

---

## Overview

The original firmware was a combined codebase supporting three hardware configurations:
- **Wave Rover** — wheeled UGV with motor encoders and PID speed control
- **UGV + RoArm-M2** — wheeled UGV with a 5-DOF robot arm (inverse kinematics, joint control)
- **UGV + Gimbal** — wheeled UGV with a 2-axis pan-tilt gimbal

This refactor strips the firmware down to **gimbal-only**: USB serial JSON commands, WiFi HTTP JSON commands, GPIO 18/19 bus servo UART, and OLED display. Everything related to wheels, the robot arm, and ESP-NOW radio has been removed.

---

## Files Deleted

### `movtion_module.h`
**Why removed:** Contained all motor/wheel code — `leftCtrl()`, `rightCtrl()`, `setGoalSpeed()`, `getLeftSpeed()`, `getRightSpeed()`, PID controller init, encoder init, heartbeat watchdog, and `mm_settings()`. None of this is relevant to a gimbal-only device. Keeping it would waste ~8 KB of flash and pull in dead PID/encoder library code.

### `RoArm-M2_module.h`
**Why removed:** Contained the 5-DOF robot arm implementation — inverse kinematics (`RoArmM2_baseCoordinateCtrl`), joint angle math, arm servo init (`RoArmM2_servoInit`), servo feedback polling for 5 arm joints, dynamic torque adaptation, and the `SMS_STS st` servo bus object. The gimbal does not use any arm code. The `SMS_STS st` object and a few utility functions (`servoTorqueCtrl`, `ServoFeedback` struct, `changeID`, `setMiddlePos`, `setServosPID`) were **moved to `gimbal_module.h`** so the bus servo infrastructure is still available.

### `esp_now_ctrl.h`
**Why removed:** Contained multi-robot ESP-NOW radio — peer registration, broadcast/unicast send, flow-leader/follower mode. Gimbal-only has no need for peer-to-peer radio. The `getThisDevMacAddress()` and `macToString()` functions from this file were **moved to `wifi_ctrl.h`** because the MAC address is still useful for WiFi identification.

---

## Files Modified

### `General_Driver.ino` — Main sketch rewritten

**What changed:**

| Before | After |
|--------|-------|
| Included 8 libraries (encoders, PID, Kalman, ICM20948, ESP-NOW, etc.) | Includes only what gimbal needs (ArduinoJson, SCServo, WiFi, OLED, INA219, IMU) |
| `#include <nvs_flash.h>` duplicated | Duplicate removed |
| `#include <esp_now.h>` | Removed |
| `while(!Serial) {}` blocking guard at line 103 | **Removed** (see Bug Fix 1 below) |
| `moduleType_RoArmM2()` function | **Removed** |
| `setup()` called `mm_settings()`, `movtionPinInit()`, `initEncoders()`, `pidControllerInit()`, `RoArmM2_servoInit()`, `RoArmM2_initCheck()`, `RoArmM2_moveInit()`, `RoArmM2_dynamicAdaptation()`, `initEspNow()`, `createMission("boot")`, `missionPlay("boot")` | All removed; replaced with `gimbalServoInit()` |
| `loop()` called `getLeftSpeed()`, `getRightSpeed()`, `LeftPidControllerCompute()`, `RightPidControllerCompute()`, `heartBeatCtrl()`, ESP-NOW `runNewJsonCmd` block | All removed |
| `moduleType` switch had case 1 (arm) and case 2 (gimbal) | `moduleType_Gimbal()` called with guard `if (moduleType == 2)` |
| `mainType`/`moduleType` set via `mm_settings()` | Hard-coded to `mainType=1`, `moduleType=2` in setup |
| OLED showed "WAVE ROVER" / "UGV" | Shows "Gimbal Controller" |

**Why:** The setup was initialising hardware that doesn't exist (arm servos, encoders, motors). Removing it eliminates startup errors, reduces boot time by ~700 ms (no `delay(500)` for servos power-up, no `RoArmM2_initCheck`), and saves significant flash.

---

### `ugv_config.h` — Stripped to gimbal-relevant globals only

**What removed and why:**

| Removed | Why |
|---------|-----|
| All arm servo ID defines (`BASE_SERVO_ID` 11–15, etc.) | No arm hardware |
| All arm link length constants (`ARM_L1`, `ARM_L2A`, etc.) and IK variables (`l1`, `l2`, `l3`, `radB`, `radS`, `radE`, `radG`, `goalX/Y/Z/T`, `lastX/Y/Z/T`, `base_r`, `nanIK`) | 30+ variables for inverse kinematics — none used by gimbal |
| All arm joint angle/rad variables (`BASE_JOINT_RAD`, `SHOULDER_JOINT_RAD`, etc.) | No arm |
| Arm servo arrays `servoID[5]`, `goalPos[5]`, `moveSpd[5]`, `moveAcc[5]` | No arm |
| Arm limit defines (`ARM_BASE_LIMIT_MIN_RAD`, etc.) | No arm |
| Motor pin defines (`PWMA=25`, `AIN1=21`, `AIN2=17`, `BIN1=22`, `BIN2=23`, `PWMB=26`, `AENCA=35`, `AENCB=34`, `BENCA=27`, `BENCB=16`) | No motor hardware |
| Motor PID variables (`__kp`, `__ki`, `__kd`, `windup_limits`) | No motor PID |
| Wheel parameters (`WHEEL_D`, `ONE_CIRCLE_PLUSES`, `TRACK_WIDTH`, `SET_MOTOR_DIR`) | No wheels |
| ESP-NOW variables (`espNowMode`, `ctrlByBroadcast`, `mac_whitelist_broadcast`, `runNewJsonCmd`) | ESP-NOW removed |
| `EEMode`, `emergencyStopFlag`, `RoArmM2_torqueLock`, `RoArmM2_initCheckSucceed` | No arm |
| `const_spd`, `const_mode`, `const_cmd_*`, `const_goal_*`, `MOVE_STOP/INCREASE/DECREASE`, `CONST_ANGLE/XYZT` | Constant-move feature was arm-only |
| `EoAT_A/B`, `l4A/B`, `lEA/EB/E`, `tErad`, `initX/Y/Z/T`, `delta_x/y`, `beta_x/y` | End-effector IK |
| Duplicate `#define RoArmM2_Servo_RXD 18` / `RoArmM2_Servo_TXD 19` | Renamed `S_RXD`/`S_TXD` kept |
| `freq`, `channel_A`, `channel_B` (motor PWM channels) | No motors; LED uses `IO4_CH`/`IO5_CH` instead |

**What kept:** `InfoPrint`, `mainType`, `moduleType`, `steadyMode`, `baseFeedbackFlow`, `thisMacStr`, `jsonFeedbackWeb`, `S_RXD/TXD`, `S_SCL/SDA`, all gimbal defines, all PID register address defines, all LED/IO defines, `feedbackFlowExtraDelay`, `uartCmdEcho`, `prev_time`, all IMU variables, `HEART_BEAT_DELAY`, `lastCmdRecvTime`.

---

### `gimbal_module.h` — Added servo infrastructure + fixed 5 bugs

**What added:**

- `SMS_STS st` — servo bus object, moved from `RoArm-M2_module.h`
- `struct ServoFeedback` — servo feedback struct, moved from `RoArm-M2_module.h`
- `servoTorqueCtrl()` — enable/disable servo torque, moved from `RoArm-M2_module.h`
- `gimbalServoInit()` — initialises `Serial1` at 1 Mbit/s on GPIO 18/19 and connects it to the `SMS_STS` object. **No blocking guard** (see Bug Fix 1).
- `changeID()` — change a servo's ID via EEPROM register, moved from `RoArm-M2_module.h`
- `setMiddlePos()` — calibrate servo zero point, moved from `RoArm-M2_module.h`
- `setServosPID()` — write P gain to servo EEPROM register, moved from `RoArm-M2_module.h`

**Bug fixes (see dedicated section below):** Bug 1 (compile error forward reference), Bug 2 (map/float), Bug 5 (double bus query), plus the earlier session's bugs (wrong array reference, map formula for negative angles, gimbalCtrlStop drift, delay in userCtrl).

---

### `ugv_advance.h` — Removed missions and arm functions

**What removed:**

| Removed | Why |
|---------|-----|
| `createMission()`, `missionContent()`, `appendStepJson/FB()`, `appendDelayCmd()`, `insertStepJson/FB()`, `insertDelayCmd()`, `replaceStepJson/FB()`, `replaceDelayCmd()`, `deleteStep()`, `moveToStep()`, `missionPlay()` | Mission system was arm-specific (stored arm joint positions). No missions needed for gimbal. |
| `configEEmodeType()`, `configEoAT()` | End-effector type config — arm only |
| `saveSpdRate()` | Motor speed rate — no motors |
| `serialMissionAbort()` | Only used by `missionPlay()` |
| Motor speed / arm data fields in `baseInfoFeedback()` | `speedGetA/B`, `radB/S/E/G`, `lastX/Y/Z/T`, arm torque fields |

**`baseInfoFeedback()` simplified** to output only what a gimbal device produces:
```json
{"T":1001, "r":0.0, "p":0.0, "y":0.0, "temp":25.0, "v":12.4, "pan":0.0, "tilt":0.0}
```

**`changeHeartBeatDelay()`** rewritten without motor reference (one line: sets `HEART_BEAT_DELAY`).

**What kept:** `configInfoPrint()`, `setBaseInfoFeedbackMode()`, `baseInfoFeedback()`, `changeModuleType()`, `setFeedbackFlowInterval()`, `setCmdEcho()`, `changeHeartBeatDelay()`.

---

### `uart_ctrl.h` — Removed ~60 command cases, kept ~30

**Commands removed:**

| T code | Command | Why removed |
|--------|---------|-------------|
| T:1 | `CMD_SPEED_CTRL` | Motor speed |
| T:2 | `CMD_SET_MOTOR_PID` | Motor PID |
| T:11 | `CMD_PWM_INPUT` | Motor PWM |
| T:13 | `CMD_ROS_CTRL` | ROS wheel control |
| T:100–125 | All arm commands | RoArm-M2 joints, IK, EoAT |
| T:138–140 | Speed rate commands | Motor speed rate |
| T:144 | `CMD_ARM_CTRL_UI` | Arm UI control |
| T:210–242 | Torque ctrl + all mission commands | Mission system removed |
| T:300–306 | All ESP-NOW commands | ESP-NOW removed |
| T:900 | `CMD_MM_TYPE_SET` | Called `mm_settings()` which is removed |
| T:602, T:603 | Boot mission info/reset | Mission system removed |

**Commands kept:** OLED (3, -3), module type (4), all IMU (126–129), all feedback (130–131, 142–143), LED (132), all gimbal (133–137, 141), torque (210), all file ops (200–208), all WiFi (401–408), servo settings (501–503), ESP32 system (600, 601, 604, 605).

---

### `json_cmd.h` — Matching defines only

Removed all `#define` entries for commands that were removed from `uart_ctrl.h`. Kept only defines that have a matching `case` in the trimmed switch statement. This eliminates ~250 lines of dead defines.

---

### `IMU_ctrl.h` — Fixed empty stubs

Three functions were empty (no body, no response):

```cpp
// Before — silent black hole:
void imuCalibration() {}
void getIMUOffset() {}
void setIMUOffset(int16_t x, int16_t y, int16_t z) {}
```

**Why this matters:** If a user sends `{"T":127}` (calibrate IMU), the firmware would receive it, dispatch it, and return nothing — not even an error. The user would have no way of knowing whether the command was received or silently dropped.

**Fix:** Each stub now returns a JSON response:
```cpp
Serial.println("{\"info\":\"IMU calibration not implemented\"}");
```

---

### `wifi_ctrl.h` — Added MAC utilities + fixed 2 bugs

**Added:**
- `thisDevMac[6]` — MAC address storage, moved from deleted `esp_now_ctrl.h`
- `macToString()` — formats `uint8_t[6]` as `"XX:XX:XX:XX:XX:XX"` string
- `getThisDevMacAddress()` — reads WiFi MAC and stores in `thisMacStr`

These were previously in `esp_now_ctrl.h`. They are still needed for the WiFi status feedback which includes the device MAC address.

**Bug fixes:** Bug 3 (always-true condition) and Bug 4 (unsigned WIFI_CURRENT_MODE).

---

### `http_server.h` — Fixed MIME type typo

`"text/plane"` → `"text/plain"`. (Bug 7)

---

## Bug Fixes

### Bug 1 — `while(!Serial){}` blocked GPIO 18/19 forever on battery power
**File:** `General_Driver.ino` (setup, line 103 in original)  
**Symptom:** Gimbal servos never responded when powered from battery (USB not connected). USB serial commands worked, WiFi commands did not move the servos.  
**Root cause:** `while(!Serial){}` waits until a USB serial connection is detected. Without USB, `Serial` never becomes `true`, so the loop never exits. Every line of `setup()` after it — including `gimbalServoInit()` which initialises GPIO 18/19 — never ran.  
**Fix:** Remove the blocking guard entirely. `Serial.begin()` is still called; the port initialises regardless of whether a host is connected.

---

### Bug 2 (from previous session) — `gimbalFeedback` vs `servoFeedback` array confusion
**File:** `gimbal_module.h` — `getGimbalFeedback()`  
**Symptom:** When a gimbal servo failed to respond, the firmware wrote the failure status to `servoFeedback[0]` and `servoFeedback[1]` — the **arm** servo array — instead of `gimbalFeedback[0]` and `gimbalFeedback[1]`.  
**Root cause:** Copy-paste error from the arm module.  
**Fix:** Changed array references to `gimbalFeedback[]`.

---

### Bug 3 (from previous session) — `gimbalCtrlStop()` did not stop the gimbal
**File:** `gimbal_module.h`  
**Symptom:** Sending `{"T":135}` (stop) caused the gimbal to briefly release torque and drift before re-engaging, rather than holding position.  
**Root cause:** Original implementation disabled torque for 3 ms (`delay(SERVO_STOP_DELAY)`) then re-enabled it. The servo has no memory of where it should hold; it just drifts under gravity for those 3 ms then applies torque from wherever it ended up.  
**Fix:** Read the current servo position via `getGimbalFeedback()`, then command that exact position as the goal. The servo holds the current angle with active torque.

---

### Bug 4 (from previous session) — `delay(5)` in `gimbalUserCtrl()` blocked the main loop
**File:** `gimbal_module.h`  
**Symptom:** Every time the user released a joystick axis (sent X=0 or Y=0), the main loop stalled for 5 ms. At 60 commands/second this accumulates to 300 ms/s of dead time — no serial processing, no WiFi serving, no IMU updates.  
**Root cause:** Original code did `servoTorqueCtrl(ID, 0); delay(5); servoTorqueCtrl(ID, 1);` to briefly release torque before reading position. The `delay()` is a hard block on the single-threaded ESP32.  
**Fix:** Removed the torque-toggle pattern entirely. Just call `getGimbalFeedback()` to read the current position directly.

---

### Bug 5 (from previous session) — Map formula wrong for negative pan angles
**File:** `gimbal_module.h` — `gimbalCtrlSimple()` and `gimbalCtrlMove()`  
**Symptom:** Commands like `{"T":133,"X":-45,"Y":0}` (pan left 45°) produced a large incorrect servo position, causing the gimbal to slam to an extreme instead of moving to -45°.  
**Root cause:** `map(Xinput, 0, 360, 0, 4095)` — the input range started at 0. For negative `Xinput` values (left pan), the Arduino `long` cast produced large negative numbers (e.g. `-511`) which became `2047 + (-511) = 1536`. For values near -180 the result could underflow into nonsense servo positions.  
**Fix:** Use the correct signed range: `mapFloat(Xinput, -180, 180, -2047, 2047)`. For tilt: `mapFloat(Yinput, -30, 90, -341, 1024)` — centred at Y=0 (servo position 2047).

---

### Bug 6 — `gimbalCtrlStop()` forward reference (compile error)
**File:** `gimbal_module.h`  
**Symptom:** Would not compile. Arduino's auto-prototype pass only applies to functions defined directly in `.ino` files. Functions defined in included `.h` files are processed strictly in order. `gimbalCtrlStop()` called `getGimbalFeedback()` which was defined 50+ lines later in the same file.  
**Fix:** Moved `getGimbalFeedback()` above `gimbalCtrlStop()`.

---

### Bug 7 — `map()` used with `float` inputs (precision loss in stabilisation)
**File:** `gimbal_module.h` — `gimbalCtrlSimple()` and `gimbalCtrlMove()`  
**Symptom:** Stabilisation mode (`{"T":137,"s":1,"y":0}`) produced jerky quantised movement — corrections only happened in 1° steps even though the IMU reports in fractions of a degree.  
**Root cause:** Arduino's `map(value, ...)` takes `long` arguments. When `float` values (like `icm_pitch = 12.7`) were passed, they were truncated to `long` (→ `12`) before the mapping calculation. All sub-degree IMU data was discarded.  
**Fix:** Replaced `map()` with `mapFloat()` — the float-safe version already defined in the same file.

---

### Bug 8 — `createWifiConfigFileByStatus()` condition always true
**File:** `wifi_ctrl.h`  
**Symptom:** Calling `configWifiModeOnBoot(0)` to disable WiFi on next boot appeared to succeed but the config file was always written (even for mode=0), meaning the user could never actually disable WiFi via command.  
**Root cause:** `if (WIFI_MODE_ON_BOOT != 0 || WIFI_MODE_ON_BOOT != -1)` — for any integer value, at least one side is true. The `||` should have been `&&`.  
**Fix:** Changed `||` to `&&`.

---

### Bug 9 — `WIFI_CURRENT_MODE` unsigned type initialised to -1
**File:** `wifi_ctrl.h`  
**Symptom:** `WIFI_CURRENT_MODE` was used as a sentinel (value `-1` = "unknown/error") but declared as `byte` (unsigned 8-bit). The value `-1` silently became `255`. Any code comparing it to `-1` always failed.  
**Fix:** Changed declaration to `int WIFI_CURRENT_MODE = -1`.

---

### Bug 10 — Double bus query in `gimbalUserCtrl()` when X=0 and Y=0
**File:** `gimbal_module.h`  
**Symptom:** Sending `{"T":141,"X":0,"Y":0,"SPD":300}` (full joystick release) triggered two consecutive half-duplex UART bus queries, each blocking the loop for ~1–2 ms.  
**Root cause:** The `if(inputX == 0)` and `if(inputY == 0)` branches were independent, each calling `getGimbalFeedback()` separately.  
**Fix:** Single `getGimbalFeedback()` call before both axis checks: `if(inputX == 0 || inputY == 0) { getGimbalFeedback(); ... }`.

---

### Bug 11 — HTTP response MIME type typo
**File:** `http_server.h`  
**Symptom:** HTTP clients that strictly validate `Content-Type` (some browsers, REST clients) may reject the response or not parse the JSON body.  
**Root cause:** `server.send(200, "text/plane", ...)` — `"plane"` is not a registered MIME type.  
**Fix:** Corrected to `"text/plain"`.

---

## Summary Table

| # | File | Change type | Description |
|---|------|------------|-------------|
| — | `movtion_module.h` | **Deleted** | All motor/wheel/encoder/PID/heartbeat code |
| — | `RoArm-M2_module.h` | **Deleted** | All arm IK, arm servos, arm feedback |
| — | `esp_now_ctrl.h` | **Deleted** | All ESP-NOW radio code |
| 1 | `General_Driver.ino` | **Rewritten** | Gimbal-only setup/loop; removed 10+ dead init calls |
| 2 | `ugv_config.h` | **Stripped** | Removed 60+ motor/arm/IK variables; kept gimbal globals |
| 3 | `gimbal_module.h` | **Extended + fixed** | Added SMS_STS, ServoFeedback, servo utilities; fixed 5 bugs |
| 4 | `ugv_advance.h` | **Stripped** | Removed missions, arm functions; simplified baseInfoFeedback |
| 5 | `uart_ctrl.h` | **Stripped** | Removed ~60 command cases; kept ~30 gimbal/WiFi/file/system |
| 6 | `json_cmd.h` | **Stripped** | Matching defines only; removed ~250 lines of dead defines |
| 7 | `IMU_ctrl.h` | **Fixed** | Empty stubs now return JSON error responses |
| 8 | `wifi_ctrl.h` | **Extended + fixed** | Added MAC utilities from deleted esp_now_ctrl.h; fixed 2 bugs |
| 9 | `http_server.h` | **Fixed** | MIME type typo corrected |
