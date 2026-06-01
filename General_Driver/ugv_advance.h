// place holder.
void jsonCmdReceiveHandler();

// set the InfoPrint.
void configInfoPrint(byte inputCmd) {
	switch (inputCmd) {
	case 0: InfoPrint = 0; break;
	case 1: InfoPrint = 1; break;
	case 2: InfoPrint = 2; break;
	}
}

// set the baseInfoFeedback.
void setBaseInfoFeedbackMode(bool inputCmd) {
	baseFeedbackFlow = (inputCmd == 1) ? 1 : 0;
}

// baseInfoFeedback: outputs IMU + battery + gimbal angles over serial.
void baseInfoFeedback() {
	static unsigned long last_feedback_time;
	if (millis() - last_feedback_time < (unsigned long)feedbackFlowExtraDelay) {
		return;
	}
	last_feedback_time = millis();

	jsonInfoHttp.clear();
	jsonInfoHttp["T"] = FEEDBACK_BASE_INFO;
	jsonInfoHttp["r"] = icm_roll;
	jsonInfoHttp["p"] = icm_pitch;
	jsonInfoHttp["y"] = icm_yaw;
	jsonInfoHttp["temp"] = temp;
	jsonInfoHttp["v"] = loadVoltage_V;
	jsonInfoHttp["pan"]  = panAngleCompute(gimbalFeedback[0].pos);
	jsonInfoHttp["tilt"] = tiltAngleCompute(gimbalFeedback[1].pos);

	String getInfoJsonString;
	serializeJson(jsonInfoHttp, getInfoJsonString);
	Serial.println(getInfoJsonString);
}

// change module type.
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
