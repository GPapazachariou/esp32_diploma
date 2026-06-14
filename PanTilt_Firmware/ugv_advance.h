// ─────────────────────────────────────────────────────────────────────────────
// ugv_advance.h — configuration setters and feedback (gimbal-only)
//
// Stripped: removed all mission scripting, arm functions, motor speed rate,
//           and arm/motor fields from baseInfoFeedback().
// Changed:  baseInfoFeedback() outputs gimbal-only JSON.
//           Serial.println → jsPrint() so RPi also receives feedback.
// ─────────────────────────────────────────────────────────────────────────────

// Forward declaration needed because uart_ctrl.h defines jsPrint() and is
// included after this file.
void jsPrint(const String& msg);


void configInfoPrint(byte inputCmd) {
  switch (inputCmd) {
  case 0: InfoPrint = 0; break;
  case 1: InfoPrint = 1; break;
  case 2: InfoPrint = 2; break;
  }
}


void setBaseInfoFeedbackMode(bool inputCmd) {
  baseFeedbackFlow = (inputCmd == 1) ? 1 : 0;
}


// Streams a JSON feedback packet to both USB and RPi UART.
// Format: {"T":1001,"r":roll,"p":pitch,"y":yaw,"temp":T,"v":V,"pan":P,"tilt":T}
void baseInfoFeedback() {
  static unsigned long last_feedback_time = 0;
  if (millis() - last_feedback_time < (unsigned long)feedbackFlowExtraDelay) {
    return;
  }
  last_feedback_time = millis();

  jsonInfoHttp.clear();
  jsonInfoHttp["T"]    = FEEDBACK_BASE_INFO;
  jsonInfoHttp["r"]    = icm_roll;
  jsonInfoHttp["p"]    = icm_pitch;
  jsonInfoHttp["y"]    = icm_yaw;
  jsonInfoHttp["temp"] = temp;
  jsonInfoHttp["v"]    = loadVoltage_V;
  jsonInfoHttp["pan"]  = panAngleCompute(gimbalFeedback[0].pos);
  jsonInfoHttp["tilt"] = tiltAngleCompute(gimbalFeedback[1].pos);

  String getInfoJsonString;
  serializeJson(jsonInfoHttp, getInfoJsonString);
  jsPrint(getInfoJsonString);  // → both Serial and Serial2
}


void changeModuleType(byte inputCmd) {
  moduleType = inputCmd;
}


void setFeedbackFlowInterval(int inputCmd) {
  feedbackFlowExtraDelay = abs(inputCmd);
}


void setCmdEcho(bool inputCmd) {
  uartCmdEcho = inputCmd;
}


void changeHeartBeatDelay(int inputCmd) {
  HEART_BEAT_DELAY = inputCmd;
}
