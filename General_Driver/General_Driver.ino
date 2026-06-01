#include <ArduinoJson.h>
StaticJsonDocument<256> jsonCmdReceive;
StaticJsonDocument<256> jsonInfoSend;
StaticJsonDocument<512> jsonInfoHttp;

#include <SCServo.h>
#include <nvs_flash.h>
#include <esp_system.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_SSD1306.h>
#include <INA219_WE.h>
#include <math.h>

// functions for battery info.
#include "battery_ctrl.h"

// functions for oled.
#include "oled_ctrl.h"

// config for ugv.
#include "ugv_config.h"

// functions for the leds.
#include "ugv_led_ctrl.h"

// functions for gimbal ctrl (also defines SMS_STS st, ServoFeedback, servoTorqueCtrl, gimbalServoInit).
#include "gimbal_module.h"

// define json cmd.
#include "json_cmd.h"

// functions for IMU ctrl.
#include "IMU_ctrl.h"

// functions for editing files in flash.
#include "files_ctrl.h"

// advance functions.
#include "ugv_advance.h"

// functions for wifi ctrl.
#include "wifi_ctrl.h"

// functions for uart json ctrl.
#include "uart_ctrl.h"

// functions for http & web server.
#include "http_server.h"


void moduleType_Gimbal() {
  getGimbalFeedback();
  gimbalSteady(steadyGoalY);
}


void setup() {
  Serial.begin(115200);
  Wire.begin(S_SDA, S_SCL);
  // Note: removed while(!Serial){} — that blocked forever on battery power,
  // preventing Serial1 (GPIO 18/19) from ever being initialised.

  ina219_init();
  inaDataUpdate();

  mainType = 1;
  moduleType = 2;

  init_oled();
  screenLine_0 = "Gimbal Controller";
  screenLine_1 = "version: gimbal-1.0";
  screenLine_2 = "starting...";
  screenLine_3 = "";
  oled_update();

  delay(500);

  imu_init();

  led_pin_init();

  screenLine_2 = screenLine_3;
  screenLine_3 = "Initialize LittleFS";
  oled_update();
  if(InfoPrint == 1){Serial.println("Initialize LittleFS.");}
  initFS();

  screenLine_2 = screenLine_3;
  screenLine_3 = "ServoCtrl init...";
  oled_update();
  if(InfoPrint == 1){Serial.println("Gimbal servo init (Serial1 GPIO18/19).");}
  gimbalServoInit();

  screenLine_2 = screenLine_3;
  screenLine_3 = "WiFi init";
  oled_update();
  if(InfoPrint == 1){Serial.println("WiFi init.");}
  initWifi();

  screenLine_2 = screenLine_3;
  screenLine_3 = "HTTP server init";
  oled_update();
  if(InfoPrint == 1){Serial.println("HTTP server init.");}
  initHttpWebServer();

  screenLine_3 = "Gimbal started";
  oled_update();
  if(InfoPrint == 1){Serial.println("Gimbal started.");}

  getThisDevMacAddress();
  updateOledWifiInfo();

  led_pwm_ctrl(0, 0);
}


void loop() {
  serialCtrl();
  server.handleClient();

  if (moduleType == 2) {
    moduleType_Gimbal();
  }

  oledInfoUpdate();

  updateIMUData();

  if (baseFeedbackFlow) {
    baseInfoFeedback();
  }

  size_t freeHeap = esp_get_free_heap_size();
}
