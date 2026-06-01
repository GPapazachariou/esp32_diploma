// Servo control object (moved here from RoArm-M2_module.h).
SMS_STS st;

struct ServoFeedback {
  bool status;
  int pos;
  int speed;
  int load;
  float voltage;
  float current;
  float temper;
  byte mode;
};

void servoTorqueCtrl(byte servoID, u8 enableCMD) {
  st.EnableTorque(servoID, enableCMD);
}

void gimbalServoInit() {
  Serial1.begin(1000000, SERIAL_8N1, S_RXD, S_TXD);
  st.pSerial = &Serial1;
}

u8 gimbalID[2] = {GIMBAL_PAN_ID, GIMBAL_TILT_ID};

s16 gimbalPos[2];
u16 gimbalSpd[2];
u8  gimbalAcc[2];

ServoFeedback gimbalFeedback[2];
// [0] PAN
// [1] TILT

float steadyGoalY = 0;

float constrainFloat(float value, float min, float max) {
  if (value < min) {
    return min;
  } else if (value > max) {
    return max;
  }
  return value;
}

float mapFloat(float value, float fromLow, float fromHigh, float toLow, float toHigh) {
  return (value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
}

void gimbalCtrlSimple(float Xinput, float Yinput, float spdInput, float accInput) {
  Xinput = constrainFloat(Xinput, -180, 180);
  Yinput = constrainFloat(Yinput, -30, 90);

  // Fix: use signed range so negative pan angles map correctly.
  gimbalPos[0] = 2047 + (int)round(map(Xinput, -180, 180, -2047, 2047));
  gimbalPos[1] = 2047 - (int)round(map(Yinput, -30, 90, -341, 1024));

  gimbalSpd[0] = (int)round(map(spdInput, 0, 360, 0, 4095));
  gimbalSpd[1] = (int)round(map(spdInput, 0, 360, 0, 4095));

  gimbalAcc[0] = (int)round(map(accInput, 0, 360, 0, 4095));
  gimbalAcc[1] = (int)round(map(accInput, 0, 360, 0, 4095));

  st.SyncWritePosEx(gimbalID, 2, gimbalPos, gimbalSpd, gimbalAcc);
}

void gimbalCtrlMove(float Xinput, float Yinput, float spdInputX, float spdInputY) {
  Xinput = constrainFloat(Xinput, -180, 180);
  Yinput = constrainFloat(Yinput, -30, 90);

  spdInputX = constrain(spdInputX, 1, 2500);
  spdInputY = constrain(spdInputY, 1, 2500);

  // Fix: use signed range so negative pan angles map correctly.
  gimbalPos[0] = 2047 + (int)round(map(Xinput, -180, 180, -2047, 2047));
  gimbalPos[1] = 2047 - (int)round(map(Yinput, -30, 90, -341, 1024));

  gimbalSpd[0] = spdInputX;
  gimbalSpd[1] = spdInputY;

  gimbalAcc[0] = 0;
  gimbalAcc[1] = 0;

  st.SyncWritePosEx(gimbalID, 2, gimbalPos, gimbalSpd, gimbalAcc);
}


float panAngleCompute(int inputPos) {
  return mapFloat((inputPos - 2047), -2047, 2047, -180, 180);
}

float tiltAngleCompute(int inputPos) {
  return mapFloat((2047 - inputPos), -1024, 341, -90, 30);
}

// Fix: hold current position instead of torque-toggling (servo drifts after torque re-enable).
void gimbalCtrlStop() {
  getGimbalFeedback();
  gimbalPos[0] = gimbalFeedback[0].pos;
  gimbalPos[1] = gimbalFeedback[1].pos;
  gimbalSpd[0] = 0;
  gimbalSpd[1] = 0;
  gimbalAcc[0] = 0;
  gimbalAcc[1] = 0;
  st.SyncWritePosEx(gimbalID, 2, gimbalPos, gimbalSpd, gimbalAcc);
}

void getGimbalFeedback() {
  if(st.FeedBack(GIMBAL_PAN_ID)!=-1) {
    gimbalFeedback[0].status = true;
    gimbalFeedback[0].pos = st.ReadPos(-1);
    gimbalFeedback[0].speed = st.ReadSpeed(-1);
    gimbalFeedback[0].load = st.ReadLoad(-1);
    gimbalFeedback[0].voltage = st.ReadVoltage(-1);
    gimbalFeedback[0].current = st.ReadCurrent(-1);
    gimbalFeedback[0].temper = st.ReadTemper(-1);
    gimbalFeedback[0].mode = st.ReadMode(GIMBAL_PAN_ID);
  } else {
    // Fix: was writing to servoFeedback[] (arm array) instead of gimbalFeedback[].
    gimbalFeedback[0].status = false;
    if(InfoPrint == 1){
      jsonInfoHttp.clear();
      jsonInfoHttp["T"] = 1005;
      jsonInfoHttp["id"] = GIMBAL_PAN_ID;
      jsonInfoHttp["status"] = 0;
      String getInfoJsonString;
      serializeJson(jsonInfoHttp, getInfoJsonString);
      Serial.println(getInfoJsonString);
    }
  }

  if(st.FeedBack(GIMBAL_TILT_ID)!=-1) {
    gimbalFeedback[1].status = true;
    gimbalFeedback[1].pos = st.ReadPos(-1);
    gimbalFeedback[1].speed = st.ReadSpeed(-1);
    gimbalFeedback[1].load = st.ReadLoad(-1);
    gimbalFeedback[1].voltage = st.ReadVoltage(-1);
    gimbalFeedback[1].current = st.ReadCurrent(-1);
    gimbalFeedback[1].temper = st.ReadTemper(-1);
    gimbalFeedback[1].mode = st.ReadMode(GIMBAL_TILT_ID);
  } else {
    // Fix: was writing to servoFeedback[] (arm array) instead of gimbalFeedback[].
    gimbalFeedback[1].status = false;
    if(InfoPrint == 1){
      jsonInfoHttp.clear();
      jsonInfoHttp["T"] = 1005;
      jsonInfoHttp["id"] = GIMBAL_TILT_ID;
      jsonInfoHttp["status"] = 0;
      String getInfoJsonString;
      serializeJson(jsonInfoHttp, getInfoJsonString);
      Serial.println(getInfoJsonString);
    }
  }
}

void gimbalSteadySet(bool inputCmd, float inputY) {
  steadyMode = inputCmd;
  if (inputY < -45) {
    inputY = -45;
  } else if (inputY > 90) {
    inputY = 90;
  }
  steadyGoalY = inputY;
}


void gimbalSteady(float inputBiasY) {
  if (!steadyMode) {
    return;
  }
  gimbalCtrlSimple(0, inputBiasY - icm_pitch, 0, 0);
}


void gimbalUserCtrl(int inputX, int inputY, int inputSpd) {
  static float goalX = 0;
  static float goalY = 0;

  if(inputX == -1 && inputY == 1){
    goalX = -180;
    goalY = 90;
  }
  else if(inputX == 0 && inputY == 1){
    goalY = 90;
  }
  else if(inputX == 1 && inputY == 1){
    goalX = 180;
    goalY = 90;
  }
  else if(inputX == -1 && inputY == 0){
    goalX = -180;
  }
  else if(inputX == 1 && inputY == 0){
    goalX = 180;
  }
  else if(inputX == -1 && inputY == -1){
    goalX = -180;
    goalY = -45;
  }
  else if(inputX == 0 && inputY == -1){
    goalY = -45;
  }
  else if(inputX == 1 && inputY == -1){
    goalX = 180;
    goalY = -45;
  }

  if(inputX == 2 && inputY == 2){
    gimbalCtrlSimple(0, 0, 0, 10);
  }
  else{
    gimbalCtrlSimple(goalX, goalY, inputSpd, 0);
    if(inputX == 0){
      // Fix: removed torque-toggle + delay(5) pattern (blocking + causes drift).
      getGimbalFeedback();
      goalX = panAngleCompute(gimbalFeedback[0].pos);
    }
    if(inputY == 0){
      // Fix: removed torque-toggle + delay(5) pattern (blocking + causes drift).
      getGimbalFeedback();
      goalY = tiltAngleCompute(gimbalFeedback[1].pos);
    }
  }

}


// Change a servo's ID (works with any ID including broadcast 254).
void changeID(byte oldID, byte newID) {
  st.unLockEprom(oldID);
  st.writeByte(oldID, SMS_STS_ID, newID);
  st.LockEprom(newID);
  if(InfoPrint == 1) {
    Serial.print("changeID: ");Serial.print(oldID);
    Serial.print(" -> ");Serial.println(newID);
  }
}

// Set the current position as the middle (zero) position of the servo.
void setMiddlePos(byte inputID) {
  st.CalibrationOfs(inputID);
}

// Write the P gain into the servo's EEPROM register.
bool setServosPID(byte inputID, byte inputP) {
  st.unLockEprom(inputID);
  st.writeByte(inputID, ST_PID_P_ADDR, inputP);
  st.LockEprom(inputID);
  return true;
}
