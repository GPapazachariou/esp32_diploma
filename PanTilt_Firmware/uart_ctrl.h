// ─────────────────────────────────────────────────────────────────────────────
// uart_ctrl.h  — JSON command parser and dispatcher
//
// Changes from General_Driver:
//   + jsPrint()     — broadcasts responses to both Serial (USB) and Serial2 (RPi)
//   + serial2Ctrl() — reads JSON commands from Raspberry Pi on GPIO 16/17
//   + heartBeatCtrl() — fixed (undefined currentTimeMillis → millis())
//   - Removed: arm, motor, encoder, ESP-NOW, mission commands
//   - Removed: all WiFi / web-server commands (RPi-UART-only control)
// ─────────────────────────────────────────────────────────────────────────────

// Broadcast a JSON response to both USB serial and Raspberry Pi GPIO UART.
void jsPrint(const String& msg) {
  Serial.println(msg);
  Serial2.println(msg);
}


void jsonCmdReceiveHandler() {
  int cmdType = jsonCmdReceive["T"].as<int>();
  switch (cmdType) {

  // ── OLED ──────────────────────────────────────────────────────────────────
  case CMD_OLED_CTRL:
    oledCtrl(jsonCmdReceive["lineNum"], jsonCmdReceive["Text"]);
    break;
  case CMD_OLED_DEFAULT:
    setOledDefault();
    break;

  // ── Module type ───────────────────────────────────────────────────────────
  case CMD_MODULE_TYPE:
    changeModuleType(jsonCmdReceive["cmd"]);
    break;

  // ── IMU ───────────────────────────────────────────────────────────────────
  case CMD_GET_IMU_DATA:
    getIMUData();
    break;
  case CMD_CALI_IMU_STEP:
    imuCalibration();
    break;
  case CMD_GET_IMU_OFFSET:
    getIMUOffset();
    break;
  case CMD_SET_IMU_OFFSET:
    setIMUOffset(jsonCmdReceive["x"], jsonCmdReceive["y"], jsonCmdReceive["z"]);
    break;

  // ── Feedback ──────────────────────────────────────────────────────────────
  case CMD_BASE_FEEDBACK:
    baseInfoFeedback();
    break;
  case CMD_BASE_FEEDBACK_FLOW:
    setBaseInfoFeedbackMode(jsonCmdReceive["cmd"]);
    break;
  case CMD_FEEDBACK_FLOW_INTERVAL:
    setFeedbackFlowInterval(jsonCmdReceive["cmd"]);
    break;
  case CMD_UART_ECHO_MODE:
    setCmdEcho(jsonCmdReceive["cmd"]);
    break;

  // ── LED ───────────────────────────────────────────────────────────────────
  case CMD_LED_CTRL:
    led_pwm_ctrl(jsonCmdReceive["IO4"], jsonCmdReceive["IO5"]);
    break;

  // ── Gimbal ────────────────────────────────────────────────────────────────
  case CMD_GIMBAL_CTRL_SIMPLE:
    heartbeatStopFlag = false;
    lastCmdRecvTime = millis();
    gimbalCtrlSimple(
      jsonCmdReceive["X"],
      jsonCmdReceive["Y"],
      jsonCmdReceive["SPD"],
      jsonCmdReceive["ACC"]);
    break;

  case CMD_GIMBAL_CTRL_MOVE:
    heartbeatStopFlag = false;
    lastCmdRecvTime = millis();
    gimbalCtrlMove(
      jsonCmdReceive["X"],
      jsonCmdReceive["Y"],
      jsonCmdReceive["SX"],
      jsonCmdReceive["SY"]);
    break;

  case CMD_GIMBAL_CTRL_STOP:
    gimbalCtrlStop();
    break;

  case CMD_HEART_BEAT_SET:
    changeHeartBeatDelay(jsonCmdReceive["cmd"]);
    break;

  case CMD_GIMBAL_STEADY:
    heartbeatStopFlag = false;
    lastCmdRecvTime = millis();
    gimbalSteadySet(jsonCmdReceive["s"], jsonCmdReceive["y"]);
    break;

  case CMD_GIMBAL_USER_CTRL:
    heartbeatStopFlag = false;
    lastCmdRecvTime = millis();
    gimbalUserCtrl(
      jsonCmdReceive["X"],
      jsonCmdReceive["Y"],
      jsonCmdReceive["SPD"]);
    break;

  // ── Servo torque ──────────────────────────────────────────────────────────
  case CMD_TORQUE_CTRL:
    servoTorqueCtrl(254, jsonCmdReceive["cmd"]);
    break;

  // ── File operations ───────────────────────────────────────────────────────
  case CMD_SCAN_FILES:
    scanFlashContents();
    break;
  case CMD_CREATE_FILE:
    createFile(jsonCmdReceive["name"], jsonCmdReceive["content"]);
    break;
  case CMD_READ_FILE:
    readFile(jsonCmdReceive["name"]);
    break;
  case CMD_DELETE_FILE:
    deleteFile(jsonCmdReceive["name"]);
    break;
  case CMD_APPEND_LINE:
    appendLine(jsonCmdReceive["name"], jsonCmdReceive["content"]);
    break;
  case CMD_INSERT_LINE:
    insertLine(jsonCmdReceive["name"], jsonCmdReceive["lineNum"], jsonCmdReceive["content"]);
    break;
  case CMD_REPLACE_LINE:
    replaceLine(jsonCmdReceive["name"], jsonCmdReceive["lineNum"], jsonCmdReceive["content"]);
    break;
  case CMD_READ_LINE:
    readSingleLine(jsonCmdReceive["name"], jsonCmdReceive["lineNum"]);
    break;
  case CMD_DELETE_LINE:
    deleteSingleLine(jsonCmdReceive["name"], jsonCmdReceive["lineNum"]);
    break;

  // ── Servo settings ────────────────────────────────────────────────────────
  case CMD_SET_SERVO_ID:
    changeID(jsonCmdReceive["raw"], jsonCmdReceive["new"]);
    break;
  case CMD_SET_MIDDLE:
    setMiddlePos(jsonCmdReceive["id"]);
    break;
  case CMD_SET_SERVO_PID:
    setServosPID(jsonCmdReceive["id"], jsonCmdReceive["p"]);
    break;

  // ── System ────────────────────────────────────────────────────────────────
  case CMD_REBOOT:
    esp_restart();
    break;
  case CMD_FREE_FLASH_SPACE:
    freeFlashSpace();
    break;
  case CMD_NVS_CLEAR:
    nvs_flash_erase();
    delay(500);
    nvs_flash_init();
    break;
  case CMD_INFO_PRINT:
    configInfoPrint(jsonCmdReceive["cmd"]);
    break;
  }
}


// ─── serialCtrl() — reads JSON from USB (PC / development machine) ────────────
void serialCtrl() {
  static String receivedData;
  while (Serial.available() > 0) {
    char c = Serial.read();
    receivedData += c;
    if (c == '\n') {
      DeserializationError err = deserializeJson(jsonCmdReceive, receivedData);
      if (err == DeserializationError::Ok) {
        if (InfoPrint == 1 && uartCmdEcho) {
          Serial.print(receivedData);
        }
        jsonCmdReceiveHandler();
      }
      receivedData = "";
    }
  }
}


// ─── serial2Ctrl() — reads JSON from Raspberry Pi on GPIO 16/17 ──────────────
void serial2Ctrl() {
  static String receivedData2;
  while (Serial2.available() > 0) {
    char c = Serial2.read();
    receivedData2 += c;
    if (c == '\n') {
      DeserializationError err = deserializeJson(jsonCmdReceive, receivedData2);
      if (err == DeserializationError::Ok) {
        if (InfoPrint == 1 && uartCmdEcho) {
          Serial2.print(receivedData2);  // echo back to RPi
        }
        jsonCmdReceiveHandler();
      }
      receivedData2 = "";
    }
  }
}


// ─── heartBeatCtrl() ─────────────────────────────────────────────────────────
// Bug Fix 7: original used undefined variable `currentTimeMillis`.
// If no gimbal command is received for HEART_BEAT_DELAY ms, hold current position.
void heartBeatCtrl() {
  if (millis() - lastCmdRecvTime > (unsigned long)HEART_BEAT_DELAY) {
    if (!heartbeatStopFlag) {
      gimbalCtrlStop();
      heartbeatStopFlag = true;
    }
  }
}
