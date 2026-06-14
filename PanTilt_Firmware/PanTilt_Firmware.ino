// ─────────────────────────────────────────────────────────────────────────────
// PanTilt_Firmware.ino
// Independent gimbal-only firmware for ESP32 pan-tilt module.
//
// Three UART channels:
//   Serial  (UART0, GPIO 0/1,   115200) — USB serial from PC / debug output
//   Serial1 (UART1, GPIO 18/19, 1Mbit) — Servo bus (SMS_STS, half-duplex)
//   Serial2 (UART2, GPIO 16/17, 115200) — Raspberry Pi GPIO UART
//
// Key fix vs original: no while(!Serial){} — device boots without USB cable.
// ─────────────────────────────────────────────────────────────────────────────

#include <ArduinoJson.h>
StaticJsonDocument<512> jsonCmdReceive;
StaticJsonDocument<512> jsonInfoSend;
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

// Include order matters: each file uses symbols from files above it.
#include "battery_ctrl.h"
#include "oled_ctrl.h"
#include "ugv_config.h"
#include "ugv_led_ctrl.h"
#include "gimbal_module.h"    // SMS_STS st, ServoFeedback, all gimbal functions
#include "json_cmd.h"
#include "IMU_ctrl.h"
#include "files_ctrl.h"
#include "ugv_advance.h"      // baseInfoFeedback, changeHeartBeatDelay, etc.
#include "wifi_ctrl.h"
#include "uart_ctrl.h"        // jsPrint, serialCtrl, serial2Ctrl, heartBeatCtrl
#include "http_server.h"      // WebServer server(80), initHttpWebServer


void setup() {
  // ── USB serial (PC / debug) ──────────────────────────────────────────────
  // Critical fix: NO while(!Serial){} here.
  // The original code had this guard which waited forever for a USB host,
  // preventing Serial1 (GPIO 18/19 servo bus) from ever initialising on
  // battery power.
  Serial.begin(115200);

  // ── Raspberry Pi GPIO UART ───────────────────────────────────────────────
  // RPi_RX_PIN=16 (was BENCB encoder), RPi_TX_PIN=17 (was AIN2 motor)
  // Wiring: RPi GPIO14(TX) → ESP32 GPIO16, RPi GPIO15(RX) → ESP32 GPIO17, GND→GND
  Serial2.begin(115200, SERIAL_8N1, RPi_RX_PIN, RPi_TX_PIN);

  // ── I2C (OLED, IMU, battery monitor on GPIO 32/33) ───────────────────────
  Wire.begin(S_SDA, S_SCL);

  // ── Battery monitor ───────────────────────────────────────────────────────
  ina219_init();
  inaDataUpdate();

  // ── OLED startup screen ───────────────────────────────────────────────────
  init_oled();
  screenLine_0 = "Gimbal Controller";
  screenLine_1 = "version 1.0";
  screenLine_2 = "starting...";
  screenLine_3 = "";
  oled_update();
  delay(1000);

  // ── IMU ───────────────────────────────────────────────────────────────────
  imu_init();

  // ── LED GPIO 4 / 5 ───────────────────────────────────────────────────────
  led_pin_init();

  // ── Flash filesystem (stores /wifiConfig.json) ───────────────────────────
  screenLine_3 = "Init LittleFS";
  oled_update();
  if (InfoPrint == 1) { Serial.println("Initialize LittleFS."); }
  initFS();

  // ── Servo bus on GPIO 18/19 (Serial1 at 1 Mbit/s) ───────────────────────
  screenLine_3 = "Init servo bus";
  oled_update();
  if (InfoPrint == 1) { Serial.println("ServoCtrl init UART1..."); }
  gimbalServoInit();
  // Inits Serial1, checks each servo, centres both axes to 0°.

  // ── WiFi ──────────────────────────────────────────────────────────────────
  screenLine_3 = "WiFi init";
  oled_update();
  if (InfoPrint == 1) { Serial.println("WiFi init."); }
  initWifi();

  // ── HTTP web server on port 80 ────────────────────────────────────────────
  initHttpWebServer();

  // ── Show AP SSID, password (or IP), voltage on OLED ──────────────────────
  updateOledWifiInfo();

  lastCmdRecvTime = millis();  // start heartbeat timer after setup

  if (InfoPrint == 1) { Serial.println("Gimbal firmware ready."); }
}


void loop() {
  // Read JSON commands from USB (PC / development machine)
  serialCtrl();

  // Read JSON commands from Raspberry Pi via GPIO 16/17
  serial2Ctrl();

  // Process any pending WiFi HTTP requests
  server.handleClient();

  // Query both servo positions (~1 ms, non-blocking half-duplex UART read)
  getGimbalFeedback();

  // If stabilisation mode is enabled, tilt camera to compensate body pitch
  gimbalSteady(steadyGoalY);

  // Update IMU angles (roll, pitch, yaw) — non-blocking sensor read
  updateIMUData();

  // Stream feedback JSON if enabled ({"T":131,"cmd":1})
  if (baseFeedbackFlow) {
    baseInfoFeedback();
  }

  // Refresh OLED every 10 s with battery voltage
  oledInfoUpdate();

  // Hold current position if no gimbal command received for HEART_BEAT_DELAY ms
  heartBeatCtrl();
}
