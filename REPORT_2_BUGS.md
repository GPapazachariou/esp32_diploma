# Report 2: Bug Report — ESP32 UGV/RoArm Firmware
### All Blocking States, Code Issues, Communication Problems, and Command Issues Found

---

## Severity Legend

| Level | Meaning |
|-------|---------|
| 🔴 CRITICAL | Can crash the system, cause hardware damage, or completely block operation |
| 🟠 HIGH | Causes incorrect behavior, data loss, or significant reliability issues |
| 🟡 MEDIUM | Causes degraded functionality, edge-case failures, or subtle wrong behavior |
| 🟢 LOW | Code quality / style issues that could cause problems under specific conditions |

---

## Bug #1 🔴 CRITICAL — Blocking Motion in the Main Loop

**File:** `RoArm-M2_module.h:823-853`
**Function:** `RoArmM2_movePosGoalfromLast()`

### What happens

The smooth arm movement function contains a `for` loop with `delay(2)` calls inside it:

```cpp
for(double i=0; i<=1; i+=(1/(deltaSteps*1))*spdInput){
    bufferX = besselCtrl(lastX, goalX, i);
    ...
    RoArmM2_goalPosMove();
    delay(2);   // ← BLOCKS HERE
}
```

Each step of the movement takes 2 ms. A large movement (e.g. 200 mm) with `spdInput=0.25` produces roughly `200 / 0.25 = 800` iterations × 2 ms = **1.6 seconds** of complete blocking.

### Why it is a problem

The ESP32 has **no operating system** and no background threads in this code. While `delay()` is running, **nothing else executes**:
- HTTP web server cannot respond to requests (web page freezes)
- UART serial commands are buffered but not processed
- ESP-NOW packets queue up or are lost
- Encoder readings accumulate without being processed → wrong speed measurements after the delay
- Heartbeat watchdog (`heartBeatCtrl`) cannot fire → robot may lurch unexpectedly after the arm move finishes

### Root cause

`delay()` on Arduino/ESP32 is a **busy-wait spin** that blocks the single-core execution path. The architecture assumes `loop()` cycles very fast (< 1 ms per iteration ideally).

### Fix direction

Replace `delay(2)` with a time-based state machine using `millis()` or use `yield()` / `server.handleClient()` inside the loop to at least service the web server. Better: move arm movement to a FreeRTOS task on Core 0 (the ESP32 has two cores).

---

## Bug #2 🔴 CRITICAL — Blocking WiFi Connection During Setup

**File:** `wifi_ctrl.h:207-224` and `wifi_ctrl.h:274-291`
**Functions:** `wifiModeSTA()`, `wifiModeAPSTA()`

### What happens

```cpp
while (WiFi.status() != WL_CONNECTED) {
    unsigned long currentTime = millis();
    if (InfoPrint == 1) {Serial.print(".");}
    delay(500);   // ← BLOCKS for up to 15 seconds

    if (currentTime - connectionStartTime >= connectionTimeout) {
        ...
        return false;
        break;  // ← unreachable code after return
    }
}
```

If the WiFi router is unavailable, startup is **blocked for 15 seconds**. During this time the OLED shows "WiFi init" and nothing responds.

### Secondary issue

The `break` statement after `return false` on line `wifi_ctrl.h:222` is **unreachable dead code** — `return` exits the function before `break` can ever execute.

### Fix direction

Use a non-blocking WiFi connect with `WiFi.begin()` and check status in the loop, or at minimum remove the `break` after `return`.

---

## Bug #3 🔴 CRITICAL — Blocking `while(!Serial)` in Setup

**File:** `General_Driver.ino:103`

```cpp
Serial.begin(115200);
Wire.begin(S_SDA, S_SCL);
while(!Serial) {}   // ← BLOCKS FOREVER if no USB connected
```

On an ESP32, `Serial` (USB CDC) may never become `true` if the device is not connected to a computer via USB. This means the robot will **hang forever at startup** when deployed without a USB cable (e.g., powered from battery alone).

Similarly at `RoArm-M2_module.h:172`:
```cpp
while(!Serial1) {}  // ← may block if servo bus has problems
```

### Fix direction

Remove both `while(!Serial)` and `while(!Serial1)` guards or add a timeout.

---

## Bug #4 🔴 CRITICAL — Blocking `waitMove2Goal()` Has No Timeout

**File:** `RoArm-M2_module.h:155-165`
**Function:** `waitMove2Goal()`

```cpp
void waitMove2Goal(byte InputID, s16 goalPosition, s16 offSet){
  while(servoFeedback[InputID - 11].pos < goalPosition - offSet || 
        servoFeedback[InputID - 11].pos > goalPosition + offSet){
    if (!servoFeedback[InputID - 11].status) {
      servoTorqueCtrl(254, 0);
      break;
    }
    getFeedback(InputID, true);
    delay(10);   // ← 10ms blocking per poll
  }
}
```

If a servo is mechanically blocked (jammed, overloaded, or physically prevented from reaching the goal), this loop **runs forever**. There is no maximum time limit. The robot hangs completely.

This function is called 4 times during `RoArmM2_moveInit()`:
- `waitMove2Goal(SHOULDER_DRIVING_SERVO_ID, ...)` `RoArm-M2_module.h:231`
- `waitMove2Goal(ELBOW_SERVO_ID, ...)` `RoArm-M2_module.h:248`
- And twice in `setNewAxisX()` `RoArm-M2_module.h:414-426`

Also called indirectly during boot via `RoArmM2_moveInit()` in `setup()`.

### Fix direction

Add a timeout: `unsigned long deadline = millis() + 5000; while(...) { if(millis() > deadline) break; ... }`

---

## Bug #5 🔴 CRITICAL — `RoArmM2_delayMillis()` Blocks Everything

**File:** `RoArm-M2_module.h:916-918`

```cpp
void RoArmM2_delayMillis(int inputTime) {
  delay(inputTime);
}
```

Command `{"T":111,"cmd":3000}` will block all processing for 3 seconds. This command is deliberately stored in mission files and called during `missionPlay()`. Combined with mission replay, the system can block for arbitrarily long periods. `ugv_advance.h:267-272`

---

## Bug #6 🟠 HIGH — `missionPlay()` Is a Blocking Infinite Loop

**File:** `ugv_advance.h:253-274`
**Function:** `missionPlay()`

```cpp
void missionPlay(String inputName, int repeatTimes) {
    ...
    while (1) {
        ...
        for (int i = 1; i<=_LineNum; i++) {
            if (serialMissionAbort()) {
                return;
            }
            moveToStep(inputName, i);  // ← also calls delay() internally
        }
    }
}
```

When `repeatTimes = -1`, this is a **permanent infinite loop**. The only way to escape is to send bytes over USB serial (checked by `serialMissionAbort()`). No HTTP request, no ESP-NOW command, no web button can break out of it. The robot is completely unresponsive while a mission plays.

### Also: the abort check only looks at Serial

`serialMissionAbort()` checks `Serial.available()` — but commands can also arrive from HTTP or ESP-NOW. Those are ignored.

---

## Bug #7 🟠 HIGH — `CMD_DELAY_MILLIS` (`T:111`) Uses Wrong Field Name in `insertDelayCmd`

**File:** `ugv_advance.h:156-163`
**Function:** `insertDelayCmd()`

```cpp
void insertDelayCmd(String inputName, int inputStepNum, int delayTime) {
    jsonInfoSend.clear();
    jsonInfoSend["T"] = 111;
    jsonInfoSend["cmd"] = delayTime;
    ...
    insertLine(inputName + ".mission", inputStepNum + 1, contentBuffer);
}
```

The command signature in `json_cmd.h:244` documents `{"T":111,"cmd":3000}` which matches. However, the function parameter is named `delayTime` but it is passed in from the caller via `jsonCmdReceive["spd"]` in `uart_ctrl.h:331`:

```cpp
case CMD_INSERT_DELAY:
    insertDelayCmd(
        jsonCmdReceive["name"],
        jsonCmdReceive["stepNum"],
        jsonCmdReceive["spd"]   // ← "spd" used instead of "delay"
    );break;
```

The JSON command documentation shows `{"T":227,"stepNum":3,"delay":3000}` (`json_cmd.h:389`) but the handler reads `"spd"` instead of `"delay"`. **This command is broken** — the delay value is always 0 or garbage.

---

## Bug #8 🟠 HIGH — `ang2deg()` Name Is Inverted (Degrees → Radians, Not Degrees)

**File:** `RoArm-M2_module.h:35-37`

```cpp
double ang2deg(double inputAng) {
  return (inputAng / 180) * M_PI;
}
```

The function is named `ang2deg` but it converts **degrees to radians** (multiplying by π/180). The correct conversion for "angle to degrees" would be the opposite. This is a naming bug that exists throughout the angle-control functions:

```cpp
BASE_JOINT_RAD = ang2deg(inputAng);   // RoArm-M2_module.h:981
```

While this works correctly (the math is right), the misleading name could cause maintenance bugs if someone "fixes" it in the future.

---

## Bug #9 🟠 HIGH — `shoulderJointCtrlRad()` Returns Without a Value on Two Paths

**File:** `RoArm-M2_module.h:287-303`
**Function:** `RoArmM2_shoulderJointCtrlRad()`

```cpp
int RoArmM2_shoulderJointCtrlRad(byte returnType, ...) {
  ...
  if(returnType == 1){
    st.WritePosEx(...);  // no return value
  }
  else if(returnType == SHOULDER_DRIVING_SERVO_ID){  // == 12
    return goalPos[1];
  }
  else if(returnType == SHOULDER_DRIVEN_SERVO_ID){   // == 13
    return goalPos[2];
  }
  // ← NO RETURN for returnType == 0 or returnType == 1
}
```

When `returnType == 0` or `returnType == 1`, the function falls off the end without a `return` statement. In C++, this produces **undefined behavior** — the return value is whatever garbage is in the register. Any caller using the return value of this function with `returnType=0` or `returnType=1` gets random data.

---

## Bug #10 🟠 HIGH — `maxNumInArray()` Returns No Value on Some Paths

**File:** `RoArm-M2_module.h:784-806`
**Function:** `maxNumInArray()`

```cpp
double maxNumInArray(){
  if (EEMode == 0) {
    ...
    return maxVal;
  } else if (EEMode == 1) {
    ...
    return maxVal;
  }
  // ← NO RETURN if EEMode is any other value
}
```

If `EEMode` is anything other than 0 or 1 (which can happen via `CMD_EOAT_TYPE` with `mode=2` or higher), this function returns garbage. `deltaSteps` in `RoArmM2_movePosGoalfromLast()` becomes unpredictable → loop may run for 0 iterations or billions.

---

## Bug #11 🟠 HIGH — `createWifiConfigFileByStatus()` Condition Logic Is Always True

**File:** `wifi_ctrl.h:129`

```cpp
bool createWifiConfigFileByStatus() {
    if (WIFI_MODE_ON_BOOT != 0 || WIFI_MODE_ON_BOOT != -1){
```

The condition `(x != 0 || x != -1)` is **always true** for any integer `x`, because no number can simultaneously equal both 0 and -1. The intended logic was probably `(x != 0 && x != -1)`. The current code saves the WiFi config file even when WiFi mode is "OFF" (mode 0), defeating the guard.

---

## Bug #12 🟠 HIGH — ESP-NOW Receive Callback Runs in Interrupt Context

**File:** `esp_now_ctrl.h:118`

```cpp
void OnDataRecv(const unsigned char* mac, const unsigned char* incomingData, int len) {
  ...
  memcpy(&espNowMegsRecv, incomingData, sizeof(espNowMegsRecv));
  ...
  // cmd == 1: calls jsonCmdReceiveHandler() directly
  jsonCmdReceiveHandler();  // ← RUNS INSIDE ISR
  ...
}
```

ESP-NOW receive callbacks run in a WiFi interrupt service routine (ISR) context. **Calling complex functions like `jsonCmdReceiveHandler()`, `Serial.println()`, `serializeJson()`, and `st.WritePosEx()` from an ISR is unsafe** on ESP32. It can cause:
- Stack overflow (ISR has limited stack)
- Watchdog timer resets
- Data corruption of shared globals
- Crashes due to non-reentrant library functions (ArduinoJson, Serial)

The `cmd == 1` path calls `jsonCmdReceiveHandler()` directly inside the ISR. The `cmd == 2` path correctly sets `runNewJsonCmd = true` and defers to the main loop — this is the safe pattern. The `cmd == 0` and `cmd == 1` paths are not safe.

---

## Bug #13 🟠 HIGH — `OnDataSent` Callback Calls `Serial.println()` From ISR

**File:** `esp_now_ctrl.h:91-107`

```cpp
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  ...
  serializeJson(jsonInfoHttp, getInfoJsonString);
  Serial.println(getInfoJsonString);  // ← UART from ISR context
}
```

`Serial.println()` and `serializeJson()` are not ISR-safe. Called from the ESP-NOW send callback (which also runs in interrupt context), this can corrupt the UART buffer and cause random crashes.

---

## Bug #14 🟡 MEDIUM — `StaticJsonDocument<256>` Is Too Small for Some Commands

**File:** `General_Driver.ino:2-4`

```cpp
StaticJsonDocument<256> jsonCmdReceive;
StaticJsonDocument<256> jsonInfoSend;
StaticJsonDocument<512> jsonInfoHttp;
```

The `jsonCmdReceive` buffer is 256 bytes. Some commands like `CMD_ESP_NOW_SINGLE` (`T:306`) include a `"megs"` string field that could be up to 210 characters plus field names plus other fields. The JSON could easily exceed 256 bytes. When `ArduinoJson` runs out of space, it silently truncates the document — **fields disappear without any error**.

---

## Bug #15 🟡 MEDIUM — `getWheelSpeed()` Function Is Defined But Never Called

**File:** `movtion_module.h:153-168`

```cpp
void getWheelSpeed() {
  unsigned long currentTime = micros();
  ...
  lastTime = currentTime;  // ← shares lastTime with nothing
}
```

There is a `getWheelSpeed()` function that reads **both** encoders at once, sharing a single `lastTime` variable. However, the main loop only calls `getLeftSpeed()` and `getRightSpeed()` separately, each with their own timestamp variables (`lastLeftSpdTime`, `lastRightSpdTime`). The `getWheelSpeed()` function is dead code and `lastTime` is written by it but read by nothing else.

---

## Bug #16 🟡 MEDIUM — `imuCalibration()`, `getIMUOffset()`, `setIMUOffset()` Are Empty

**File:** `IMU_ctrl.h:39-76`

```cpp
void imuCalibration() {
    // EMPTY — no implementation
}

void getIMUOffset() {
    // EMPTY — no implementation
}

void setIMUOffset(int16_t inputX, int16_t inputY, int16_t inputZ) {
    // EMPTY — no implementation
}
```

Three commands (`T:127`, `T:128`, `T:129`) are documented, registered in the switch handler, and do nothing. Sending `{"T":127}` silently succeeds with no calibration occurring. The user receives no error or warning.

---

## Bug #17 🟡 MEDIUM — `gimbalUserCtrl()` Contains Blocking `delay()` Calls

**File:** `gimbal_module.h:144-198`

```cpp
void gimbalUserCtrl(int inputX, int inputY, int inputSpd) {
  ...
  servoTorqueCtrl(GIMBAL_PAN_ID, 0);
  delay(5);       // ← blocking
  servoTorqueCtrl(GIMBAL_PAN_ID, 1);
  getGimbalFeedback();
  ...
  servoTorqueCtrl(GIMBAL_TILT_ID, 0);
  delay(5);       // ← blocking
  servoTorqueCtrl(GIMBAL_TILT_ID, 1);
}
```

These delays block the main loop every time a gimbal hold/stop is requested.

---

## Bug #18 🟡 MEDIUM — `gimbalCtrlStop()` Briefly Disables and Re-enables Torque

**File:** `gimbal_module.h:71-77`

```cpp
void gimbalCtrlStop() {
  st.EnableTorque(GIMBAL_PAN_ID, 0);
  st.EnableTorque(GIMBAL_TILT_ID, 0);
  delay(SERVO_STOP_DELAY);   // SERVO_STOP_DELAY = 3ms
  st.EnableTorque(GIMBAL_PAN_ID, 1);
  st.EnableTorque(GIMBAL_TILT_ID, 1);
}
```

"Stop" disables torque for 3 ms then re-enables it. This means the gimbal **is not stopped** — it re-engages after 3 ms and will drift to whatever position it was going to. A proper stop should hold the current position, not toggle torque.

---

## Bug #19 🟡 MEDIUM — `getGimbalFeedback()` Writes to Wrong Array on Failure

**File:** `gimbal_module.h:89-99`

```cpp
void getGimbalFeedback() {
  if(st.FeedBack(GIMBAL_PAN_ID)!=-1) {
    gimbalFeedback[0].status = true;
    ...
  } else{
    servoFeedback[0].status = false;   // ← WRONG: writes to servoFeedback, not gimbalFeedback
    ...
  }
```

On failure, the code writes to `servoFeedback[0]` (the arm servo array) instead of `gimbalFeedback[0]`. This corrupts the status of arm servo #1 (BASE_SERVO_ID) when the gimbal PAN servo fails. Same bug on line ~111 for the TILT servo writing to `servoFeedback[1]`.

---

## Bug #20 🟡 MEDIUM — `setGoalSpeed()` Has a Buffer/Setpoint Mismatch

**File:** `movtion_module.h:308-316`

```cpp
if (setpointA != setpointA_buffer) {
  pidA.Setpoint(setpointA);
  setpointA_buffer = inputLeft;   // ← stores inputLeft, not setpointA
}
```

`setpointA = inputLeft * spd_rate_A` but `setpointA_buffer = inputLeft` (without the rate). So if the rate changes between calls, `setpointA != setpointA_buffer` will be true forever and the PID setpoint updates every call regardless. The rate-adjusted comparison check is broken.

---

## Bug #21 🟡 MEDIUM — `RoArmM2_resetPID()` Writes to EPROM Without Unlock

**File:** `RoArm-M2_module.h:948-953`

```cpp
void RoArmM2_resetPID() {
  RoArmM2_setJointPID(BASE_JOINT, 16, 0);
  ...
}

void RoArmM2_setJointPID(byte jointInput, float inputP, float inputI) {
  case BASE_JOINT:
    st.writeByte(BASE_SERVO_ID, ST_PID_P_ADDR, inputP);  // ← no unLockEprom first
    st.writeByte(BASE_SERVO_ID, ST_PID_I_ADDR, inputI);  // ← no unLockEprom first
```

All other EPROM writes in the codebase (torque ctrl, ID change, PID via `setServosPID`) call `st.unLockEprom()` before writing and `st.LockEprom()` after. `RoArmM2_setJointPID()` does **not** unlock the EPROM first. The writes may silently fail.

---

## Bug #22 🟡 MEDIUM — `macStringToByteArray()` Does Not Validate MAC Format

**File:** `esp_now_ctrl.h:110-115`

```cpp
void macStringToByteArray(const String& macString, uint8_t* byteArray) {
  for (int i = 0; i < 6; i++) {
    byteArray[i] = strtol(macString.substring(i * 3, i * 3 + 2).c_str(), NULL, 16);
  }
}
```

This function only checks that `inputMac.length() == 17` (in the callers). However:
- It does not check that separators are `:` (a string like `"GG:HH:II:JJ:KK:LL"` of length 17 would silently produce zeroes)
- `strtol` errors are silently ignored
- The resulting invalid MAC could be sent as an ESP-NOW target, causing hardware-level failures

---

## Bug #23 🟡 MEDIUM — `configEEmodeType()` Self-Assigns EoAT Variables

**File:** `ugv_advance.h:297-298`

```cpp
EoAT_A = EoAT_A;   // ← assigns variable to itself, does nothing
EoAT_B = EoAT_B;
```

These lines have no effect. Likely meant to reset `EoAT_A` and `EoAT_B` to 0.

---

## Bug #24 🟢 LOW — `insertDelayCmd()` Uses Wrong Parameter Name

**File:** `ugv_advance.h:156`

The function signature is `insertDelayCmd(String inputName, int inputStepNum, int delayTime)` but the caller passes `jsonCmdReceive["spd"]` (`uart_ctrl.h:331`). The field mismatch means sending `{"T":227,"stepNum":3,"delay":3000}` will result in `delayTime=0`.

---

## Bug #25 🟢 LOW — `nvs_flash.h` Included Twice

**File:** `General_Driver.ino:9` and `General_Driver.ino:15`

```cpp
#include <nvs_flash.h>   // line 9
...
#include <nvs_flash.h>   // line 15 — duplicate
```

Not harmful due to include guards, but messy.

---

## Bug #26 🟢 LOW — `RoArmM2_singleJointAngleCtrl()` Prints Debug to Serial Unconditionally

**File:** `RoArm-M2_module.h:971-973`

```cpp
void RoArmM2_singleJointAngleCtrl(...){
  Serial.println("---");
  Serial.print(jointInput);Serial.print("\t");Serial.print(inputAng); ...
  Serial.print(inputSpd); ...
```

These `Serial.println()` calls are not guarded by `InfoPrint`. Every call to this function (from command `T:121`) always prints debug data, even when debug output is disabled with `{"T":605,"cmd":0}`.

---

## Bug #27 🟢 LOW — `currentTimeMillis` Used But Never Defined

**File:** `movtion_module.h:394`

```cpp
void heartBeatCtrl() {
  if (currentTimeMillis - lastCmdRecvTime > HEART_BEAT_DELAY) {
```

`currentTimeMillis` is used here but there is no declaration or assignment of it anywhere in the visible codebase. This should be `millis()`. If this compiles it is because there is an implicit declaration somewhere, but it will hold an undefined value — meaning the heartbeat watchdog either never fires or always fires immediately.

---

## Summary Table

| # | Severity | Location | Issue |
|---|----------|----------|-------|
| 1 | 🔴 CRITICAL | `RoArm-M2_module.h:823` | Blocking motion loop (1-2s freeze) |
| 2 | 🔴 CRITICAL | `wifi_ctrl.h:207` | Blocking WiFi connect (up to 15s) |
| 3 | 🔴 CRITICAL | `General_Driver.ino:103` | `while(!Serial)` hangs forever without USB |
| 4 | 🔴 CRITICAL | `RoArm-M2_module.h:155` | `waitMove2Goal()` infinite loop if servo jammed |
| 5 | 🔴 CRITICAL | `RoArm-M2_module.h:916` | `RoArmM2_delayMillis()` blocks all processing |
| 6 | 🟠 HIGH | `ugv_advance.h:253` | `missionPlay()` infinite blocking loop |
| 7 | 🟠 HIGH | `uart_ctrl.h:331` | `CMD_INSERT_DELAY` reads wrong JSON field |
| 8 | 🟠 HIGH | `RoArm-M2_module.h:35` | `ang2deg()` misleading name (converts deg→rad) |
| 9 | 🟠 HIGH | `RoArm-M2_module.h:287` | `shoulderJointCtrlRad()` missing return values |
| 10 | 🟠 HIGH | `RoArm-M2_module.h:784` | `maxNumInArray()` missing return on some paths |
| 11 | 🟠 HIGH | `wifi_ctrl.h:129` | WiFi config condition always true (logic error) |
| 12 | 🟠 HIGH | `esp_now_ctrl.h:118` | `jsonCmdReceiveHandler()` called from ISR |
| 13 | 🟠 HIGH | `esp_now_ctrl.h:91` | `Serial.println()` called from ISR |
| 14 | 🟡 MEDIUM | `General_Driver.ino:2` | JSON buffer too small for some commands |
| 15 | 🟡 MEDIUM | `movtion_module.h:153` | Dead function `getWheelSpeed()` |
| 16 | 🟡 MEDIUM | `IMU_ctrl.h:39` | IMU calibration/offset functions empty |
| 17 | 🟡 MEDIUM | `gimbal_module.h:183` | `delay()` in gimbal user control |
| 18 | 🟡 MEDIUM | `gimbal_module.h:71` | `gimbalCtrlStop()` doesn't actually stop |
| 19 | 🟡 MEDIUM | `gimbal_module.h:89` | Gimbal failure writes to wrong array |
| 20 | 🟡 MEDIUM | `movtion_module.h:308` | PID buffer comparison uses wrong variable |
| 21 | 🟡 MEDIUM | `RoArm-M2_module.h:922` | `setJointPID()` writes EPROM without unlock |
| 22 | 🟡 MEDIUM | `esp_now_ctrl.h:110` | MAC parser doesn't validate format |
| 23 | 🟡 MEDIUM | `ugv_advance.h:297` | Self-assignment `EoAT_A = EoAT_A` (no-op) |
| 24 | 🟢 LOW | `ugv_advance.h:156` | `insertDelayCmd` wrong field name |
| 25 | 🟢 LOW | `General_Driver.ino:15` | `nvs_flash.h` included twice |
| 26 | 🟢 LOW | `RoArm-M2_module.h:971` | Unconditional debug Serial output |
| 27 | 🟢 LOW | `movtion_module.h:394` | `currentTimeMillis` undefined variable |

---

*End of Report 2.*
