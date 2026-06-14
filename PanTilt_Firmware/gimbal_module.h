// ─────────────────────────────────────────────────────────────────────────────
// gimbal_module.h
// Pan-tilt gimbal control — servo infrastructure + all gimbal functions.
//
// Servo infrastructure was originally in RoArm-M2_module.h; moved here since
// the arm module is not present in this firmware.
// ─────────────────────────────────────────────────────────────────────────────

// ─── Servo bus object ─────────────────────────────────────────────────────────
SMS_STS st;

// ─── Servo feedback struct ────────────────────────────────────────────────────
struct ServoFeedback {
  bool  status;
  s16   pos;
  s16   speed;
  s16   load;
  float voltage;
  float current;
  float temper;
  byte  mode;
};

// ─── Servo utilities ──────────────────────────────────────────────────────────

void servoTorqueCtrl(u8 id, u8 enable) {
  st.EnableTorque(id, enable);
}

void gimbalServoInit() {
  Serial1.begin(1000000, SERIAL_8N1, S_RXD, S_TXD);
  st.pSerial = &Serial1;
  delay(500);  // servo power-up delay (setup only, not called from loop)

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

  // Centre both axes at startup
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

// ─── Gimbal arrays ────────────────────────────────────────────────────────────
u8  gimbalID[2]  = {GIMBAL_PAN_ID, GIMBAL_TILT_ID};
s16 gimbalPos[2] = {2047, 2047};
u16 gimbalSpd[2] = {0, 0};
u8  gimbalAcc[2] = {0, 0};

ServoFeedback gimbalFeedback[2];
// [0] = PAN servo,  [1] = TILT servo

float steadyGoalY = 0;

// ─── Math helpers ─────────────────────────────────────────────────────────────

float constrainFloat(float value, float mn, float mx) {
  if (value < mn) return mn;
  if (value > mx) return mx;
  return value;
}

float mapFloat(float value, float fromLow, float fromHigh, float toLow, float toHigh) {
  return (value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
}

// ─── getGimbalFeedback() ── must be ABOVE gimbalCtrlStop() (forward-ref fix) ──
//
// Bug Fix 1 (compile): moved above gimbalCtrlStop which calls it.
// Bug Fix 2 (array):   failure paths write to gimbalFeedback[], not servoFeedback[].

void getGimbalFeedback() {
  if (st.FeedBack(GIMBAL_PAN_ID) != -1) {
    gimbalFeedback[0].status  = true;
    gimbalFeedback[0].pos     = st.ReadPos(-1);
    gimbalFeedback[0].speed   = st.ReadSpeed(-1);
    gimbalFeedback[0].load    = st.ReadLoad(-1);
    gimbalFeedback[0].voltage = st.ReadVoltage(-1);
    gimbalFeedback[0].current = st.ReadCurrent(-1);
    gimbalFeedback[0].temper  = st.ReadTemper(-1);
    gimbalFeedback[0].mode    = st.ReadMode(GIMBAL_PAN_ID);
  } else {
    gimbalFeedback[0].status = false;  // Bug Fix 2: was servoFeedback[0]
    if (InfoPrint == 1) {
      jsonInfoHttp.clear();
      jsonInfoHttp["T"] = 1005;
      jsonInfoHttp["id"] = GIMBAL_PAN_ID;
      jsonInfoHttp["status"] = 0;
      String s;
      serializeJson(jsonInfoHttp, s);
      Serial.println(s);
    }
  }

  if (st.FeedBack(GIMBAL_TILT_ID) != -1) {
    gimbalFeedback[1].status  = true;
    gimbalFeedback[1].pos     = st.ReadPos(-1);
    gimbalFeedback[1].speed   = st.ReadSpeed(-1);
    gimbalFeedback[1].load    = st.ReadLoad(-1);
    gimbalFeedback[1].voltage = st.ReadVoltage(-1);
    gimbalFeedback[1].current = st.ReadCurrent(-1);
    gimbalFeedback[1].temper  = st.ReadTemper(-1);
    gimbalFeedback[1].mode    = st.ReadMode(GIMBAL_TILT_ID);
  } else {
    gimbalFeedback[1].status = false;  // Bug Fix 2: was servoFeedback[1]
    if (InfoPrint == 1) {
      jsonInfoHttp.clear();
      jsonInfoHttp["T"] = 1005;
      jsonInfoHttp["id"] = GIMBAL_TILT_ID;
      jsonInfoHttp["status"] = 0;
      String s;
      serializeJson(jsonInfoHttp, s);
      Serial.println(s);
    }
  }
}

// ─── gimbalCtrlStop() ─────────────────────────────────────────────────────────
//
// Bug Fix 4: original released torque for 3ms causing drift then jerk.
// Fix: read current position, command it back at speed=0 → servo holds in place.

void gimbalCtrlStop() {
  gimbalPos[0] = gimbalFeedback[0].status ? gimbalFeedback[0].pos : 2047;
  gimbalPos[1] = gimbalFeedback[1].status ? gimbalFeedback[1].pos : 2047;
  gimbalSpd[0] = 0;
  gimbalSpd[1] = 0;
  gimbalAcc[0] = 0;
  gimbalAcc[1] = 0;
  st.SyncWritePosEx(gimbalID, 2, gimbalPos, gimbalSpd, gimbalAcc);
}

// ─── Position ↔ angle conversion ─────────────────────────────────────────────
//
// Bug Fix 6: original used wrong range (0-360 instead of signed ranges).
// These are now the exact inverse of gimbalCtrlSimple's mapFloat calls.

float panAngleCompute(int inputPos) {
  return mapFloat((float)(inputPos - 2047), -2047.0f, 2047.0f, -180.0f, 180.0f);
}

float tiltAngleCompute(int inputPos) {
  return mapFloat((float)(2047 - inputPos), -1024.0f, 341.0f, -90.0f, 30.0f);
}

// ─── gimbalCtrlSimple() ───────────────────────────────────────────────────────
//
// Bug Fix 3: original map() used range 0-360 causing negative angles to produce
// wrong servo positions. Also used map() on speed (it's not an angle).
// Fix: mapFloat with correct signed ranges; speed/acc clamped directly.

void gimbalCtrlSimple(float Xinput, float Yinput, float spdInput, float accInput) {
  Xinput = constrainFloat(Xinput, -180.0f, 180.0f);
  Yinput = constrainFloat(Yinput,  -30.0f,  90.0f);

  gimbalPos[0] = 2047 + (int)round(mapFloat(Xinput, -180.0f, 180.0f, -2047.0f, 2047.0f));
  gimbalPos[1] = 2047 - (int)round(mapFloat(Yinput,  -30.0f,  90.0f,  -341.0f, 1024.0f));
  gimbalSpd[0] = (u16)constrain((int)spdInput, 0, 4095);
  gimbalSpd[1] = (u16)constrain((int)spdInput, 0, 4095);
  gimbalAcc[0] = (u8)constrain((int)accInput,  0, 254);
  gimbalAcc[1] = (u8)constrain((int)accInput,  0, 254);

  st.SyncWritePosEx(gimbalID, 2, gimbalPos, gimbalSpd, gimbalAcc);
}

// ─── gimbalCtrlMove() ─────────────────────────────────────────────────────────
// Same map fix as gimbalCtrlSimple; per-axis speed is in raw servo steps.

void gimbalCtrlMove(float Xinput, float Yinput, float spdInputX, float spdInputY) {
  Xinput = constrainFloat(Xinput, -180.0f, 180.0f);
  Yinput = constrainFloat(Yinput,  -30.0f,  90.0f);
  spdInputX = constrain(spdInputX, 1.0f, 2500.0f);
  spdInputY = constrain(spdInputY, 1.0f, 2500.0f);

  gimbalPos[0] = 2047 + (int)round(mapFloat(Xinput, -180.0f, 180.0f, -2047.0f, 2047.0f));
  gimbalPos[1] = 2047 - (int)round(mapFloat(Yinput,  -30.0f,  90.0f,  -341.0f, 1024.0f));
  gimbalSpd[0] = (u16)spdInputX;
  gimbalSpd[1] = (u16)spdInputY;
  gimbalAcc[0] = 0;
  gimbalAcc[1] = 0;

  st.SyncWritePosEx(gimbalID, 2, gimbalPos, gimbalSpd, gimbalAcc);
}

// ─── gimbalSteadySet() ────────────────────────────────────────────────────────

void gimbalSteadySet(bool inputCmd, float inputY) {
  steadyMode = inputCmd;
  inputY = constrainFloat(inputY, -45.0f, 90.0f);
  steadyGoalY = inputY;
}

// ─── gimbalSteady() ───────────────────────────────────────────────────────────
// Called every loop. Tilts camera to compensate for body pitch (IMU).

void gimbalSteady(float inputBiasY) {
  if (!steadyMode) return;
  gimbalCtrlSimple(0.0f, inputBiasY - icm_pitch, 200.0f, 10.0f);
}

// ─── gimbalUserCtrl() ─────────────────────────────────────────────────────────
//
// Bug Fix 5: original had delay(5) + torque toggle for each axis (up to 10ms blocking).
// Fix: single getGimbalFeedback() call before both axis checks, no torque toggle.

void gimbalUserCtrl(int inputX, int inputY, int inputSpd) {
  static float goalX = 0;
  static float goalY = 0;

  if      (inputX == -1 && inputY ==  1) { goalX = -180; goalY =  90; }
  else if (inputX ==  0 && inputY ==  1) {               goalY =  90; }
  else if (inputX ==  1 && inputY ==  1) { goalX =  180; goalY =  90; }
  else if (inputX == -1 && inputY ==  0) { goalX = -180;               }
  else if (inputX ==  1 && inputY ==  0) { goalX =  180;               }
  else if (inputX == -1 && inputY == -1) { goalX = -180; goalY = -45; }
  else if (inputX ==  0 && inputY == -1) {               goalY = -45; }
  else if (inputX ==  1 && inputY == -1) { goalX =  180; goalY = -45; }

  if (inputX == 2 && inputY == 2) {
    gimbalCtrlSimple(0.0f, 0.0f, 0.0f, 10.0f);
  } else {
    if (inputX == 0 || inputY == 0) {
      getGimbalFeedback();  // single read, ~1ms — Bug Fix 5 (was two separate reads with delay)
    }
    gimbalCtrlSimple(goalX, goalY, (float)inputSpd, 0.0f);
    if (inputX == 0) { goalX = panAngleCompute(gimbalFeedback[0].pos); }
    if (inputY == 0) { goalY = tiltAngleCompute(gimbalFeedback[1].pos); }
  }
}
