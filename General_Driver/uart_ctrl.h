void jsonCmdReceiveHandler(){
	int cmdType = jsonCmdReceive["T"].as<int>();
	switch(cmdType){

	// OLED
	case CMD_OLED_CTRL:
		oledCtrl(jsonCmdReceive["lineNum"], jsonCmdReceive["Text"]); break;
	case CMD_OLED_DEFAULT:
		setOledDefault(); break;

	// module type
	case CMD_MODULE_TYPE:
		changeModuleType(jsonCmdReceive["cmd"]); break;

	// IMU
	case CMD_GET_IMU_DATA:
		getIMUData(); break;
	case CMD_CALI_IMU_STEP:
		imuCalibration(); break;
	case CMD_GET_IMU_OFFSET:
		getIMUOffset(); break;
	case CMD_SET_IMU_OFFSET:
		setIMUOffset(jsonCmdReceive["x"], jsonCmdReceive["y"], jsonCmdReceive["z"]); break;

	// feedback
	case CMD_BASE_FEEDBACK:
		baseInfoFeedback(); break;
	case CMD_BASE_FEEDBACK_FLOW:
		setBaseInfoFeedbackMode(jsonCmdReceive["cmd"]); break;
	case CMD_FEEDBACK_FLOW_INTERVAL:
		setFeedbackFlowInterval(jsonCmdReceive["cmd"]); break;
	case CMD_UART_ECHO_MODE:
		setCmdEcho(jsonCmdReceive["cmd"]); break;

	// LED
	case CMD_LED_CTRL:
		led_pwm_ctrl(jsonCmdReceive["IO4"], jsonCmdReceive["IO5"]); break;

	// gimbal
	case CMD_GIMBAL_CTRL_SIMPLE:
		gimbalCtrlSimple(
			jsonCmdReceive["X"], jsonCmdReceive["Y"],
			jsonCmdReceive["SPD"], jsonCmdReceive["ACC"]); break;
	case CMD_GIMBAL_CTRL_MOVE:
		gimbalCtrlMove(
			jsonCmdReceive["X"], jsonCmdReceive["Y"],
			jsonCmdReceive["SX"], jsonCmdReceive["SY"]); break;
	case CMD_GIMBAL_CTRL_STOP:
		gimbalCtrlStop(); break;
	case CMD_HEART_BEAT_SET:
		changeHeartBeatDelay(jsonCmdReceive["cmd"]); break;
	case CMD_GIMBAL_STEADY:
		gimbalSteadySet(jsonCmdReceive["s"], jsonCmdReceive["y"]); break;
	case CMD_GIMBAL_USER_CTRL:
		gimbalUserCtrl(jsonCmdReceive["X"], jsonCmdReceive["Y"], jsonCmdReceive["SPD"]); break;

	// torque ctrl (broadcast ID 254)
	case CMD_TORQUE_CTRL:
		servoTorqueCtrl(254, jsonCmdReceive["cmd"]); break;

	// file ops
	case CMD_SCAN_FILES:
		scanFlashContents(); break;
	case CMD_CREATE_FILE:
		createFile(jsonCmdReceive["name"], jsonCmdReceive["content"]); break;
	case CMD_READ_FILE:
		readFile(jsonCmdReceive["name"]); break;
	case CMD_DELETE_FILE:
		deleteFile(jsonCmdReceive["name"]); break;
	case CMD_APPEND_LINE:
		appendLine(jsonCmdReceive["name"], jsonCmdReceive["content"]); break;
	case CMD_INSERT_LINE:
		insertLine(jsonCmdReceive["name"], jsonCmdReceive["lineNum"], jsonCmdReceive["content"]); break;
	case CMD_REPLACE_LINE:
		replaceLine(jsonCmdReceive["name"], jsonCmdReceive["lineNum"], jsonCmdReceive["content"]); break;
	case CMD_READ_LINE:
		readSingleLine(jsonCmdReceive["name"], jsonCmdReceive["lineNum"]); break;
	case CMD_DELETE_LINE:
		deleteSingleLine(jsonCmdReceive["name"], jsonCmdReceive["lineNum"]); break;

	// wifi
	case CMD_WIFI_ON_BOOT:
		configWifiModeOnBoot(jsonCmdReceive["cmd"]); break;
	case CMD_SET_AP:
		wifiModeAP(jsonCmdReceive["ssid"], jsonCmdReceive["password"]); break;
	case CMD_SET_STA:
		wifiModeSTA(jsonCmdReceive["ssid"], jsonCmdReceive["password"]); break;
	case CMD_WIFI_APSTA:
		wifiModeAPSTA(
			jsonCmdReceive["ap_ssid"], jsonCmdReceive["ap_password"],
			jsonCmdReceive["sta_ssid"], jsonCmdReceive["sta_password"]); break;
	case CMD_WIFI_INFO:
		wifiStatusFeedback(); break;
	case CMD_WIFI_CONFIG_CREATE_BY_STATUS:
		createWifiConfigFileByStatus(); break;
	case CMD_WIFI_CONFIG_CREATE_BY_INPUT:
		createWifiConfigFileByInput(
			jsonCmdReceive["mode"],
			jsonCmdReceive["ap_ssid"], jsonCmdReceive["ap_password"],
			jsonCmdReceive["sta_ssid"], jsonCmdReceive["sta_password"]); break;
	case CMD_WIFI_STOP:
		wifiStop(); break;

	// servo PID (bus servo register write)
	case CMD_SET_SERVO_ID:
		changeID(jsonCmdReceive["raw"], jsonCmdReceive["new"]); break;
	case CMD_SET_MIDDLE:
		setMiddlePos(jsonCmdReceive["id"]); break;
	case CMD_SET_SERVO_PID:
		setServosPID(jsonCmdReceive["id"], jsonCmdReceive["p"]); break;

	// esp32 ctrl
	case CMD_REBOOT:
		esp_restart(); break;
	case CMD_FREE_FLASH_SPACE:
		freeFlashSpace(); break;
	case CMD_NVS_CLEAR:
		nvs_flash_erase();
		delay(1000);
		nvs_flash_init(); break;
	case CMD_INFO_PRINT:
		configInfoPrint(jsonCmdReceive["cmd"]); break;
	}
}


void serialCtrl() {
  static String receivedData;

  while (Serial.available() > 0) {
    char receivedChar = Serial.read();
    receivedData += receivedChar;

    if (receivedChar == '\n') {
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
