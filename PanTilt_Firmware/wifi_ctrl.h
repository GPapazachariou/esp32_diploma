// ─────────────────────────────────────────────────────────────────────────────
// wifi_ctrl.h  — WiFi management (AP / STA / AP+STA)
//
// Changes from General_Driver:
//   Bug Fix 8:  createWifiConfigFileByStatus()  ||  → &&
//   Bug Fix 9:  byte WIFI_CURRENT_MODE = -1      → int (unsigned byte wraps to 255)
//   Added:      thisDevMac[], macToString(), getThisDevMacAddress()
//               (moved from deleted esp_now_ctrl.h; needed by wifiStatusFeedback)
//   Changed:    wifiStatusFeedback() uses jsPrint() so RPi also gets the response
// ─────────────────────────────────────────────────────────────────────────────

// Forward declaration: jsPrint() is defined in uart_ctrl.h (included after this file).
void jsPrint(const String& msg);

// ── WiFi credentials / boot mode ──────────────────────────────────────────────
byte WIFI_MODE_ON_BOOT = 1;
const char* sta_ssid     = "none";
const char* sta_password = "none";
const char* ap_ssid      = "GimbalCtrl";
const char* ap_password  = "12345678";

bool defaultModeToAPSTA = true;

File wifiConfigYaml;

unsigned long connectionStartTime;
unsigned long connectionTimeout = 15000;
int  WIFI_CURRENT_MODE = -1;    // Bug Fix 9: was byte (unsigned), -1 → 255
IPAddress localIP;
DynamicJsonDocument wifiDoc(256);
bool wifiConfigFound = false;

// ── MAC address utilities ─────────────────────────────────────────────────────
// Moved here from deleted esp_now_ctrl.h
uint8_t thisDevMac[6];

void macToString(uint8_t* mac, String& outStr) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  outStr = String(buf);
}

void getThisDevMacAddress() {
  WiFi.macAddress(thisDevMac);
  macToString(thisDevMac, thisMacStr);
}

// ── OLED update ───────────────────────────────────────────────────────────────
void updateOledWifiInfo() {
  switch (WIFI_CURRENT_MODE) {
  case 0:
    screenLine_0 = "AP: OFF";
    screenLine_1 = "ST: OFF";
    break;
  case 1:
    screenLine_0 = String("AP:") + ap_ssid;
    screenLine_1 = String("PW:") + ap_password;
    break;
  case 2:
    screenLine_0 = "AP: OFF";
    screenLine_1 = String("ST:") + localIP.toString();
    break;
  case 3:
    screenLine_0 = String("AP:") + ap_ssid;
    screenLine_1 = String("ST:") + localIP.toString();
    break;
  default:
    screenLine_0 = "WiFi: initialising";
    screenLine_1 = "";
    break;
  }
  oled_update();
}


bool loadWifiConfig() {
  wifiConfigYaml = LittleFS.open("/wifiConfig.json", "r");
  if (wifiConfigYaml) {
    if (InfoPrint == 1) { Serial.println("/wifiConfig.json load succeed."); }
    DeserializationError err = deserializeJson(wifiDoc, wifiConfigYaml);
    if (err != DeserializationError::Ok) {
      if (InfoPrint == 1) { Serial.println("wifiConfig.json parse failed."); }
      wifiConfigYaml.close();
      wifiConfigFound = false;
      return false;
    }

    WIFI_MODE_ON_BOOT = wifiDoc["wifi_mode_on_boot"] | 1;  // default to AP if missing
    sta_ssid          = wifiDoc["sta_ssid"]     | "";
    sta_password      = wifiDoc["sta_password"] | "";
    ap_ssid           = wifiDoc["ap_ssid"]      | "GimbalCtrl";
    ap_password       = wifiDoc["ap_password"]  | "12345678";

    if (InfoPrint == 1) {
      String s; serializeJson(wifiDoc, s); Serial.println(s);
    }
    wifiConfigYaml.close();
    wifiConfigFound = true;
    return true;
  } else {
    if (InfoPrint == 1) { Serial.println("wifiConfig.json not found."); }
    wifiConfigFound = false;
    return false;
  }
}


IPAddress getIPAddress(byte inputMode) {
  localIP = WiFi.localIP();
  if (InfoPrint == 1) {
    Serial.print("IP: ");
    Serial.println(localIP.toString());
  }
  jsonInfoHttp.clear();
  jsonInfoHttp["ip"] = localIP.toString();
  return localIP;
}


// Bug Fix 8: was (WIFI_MODE_ON_BOOT != 0 || WIFI_MODE_ON_BOOT != -1) — always true.
bool createWifiConfigFileByStatus() {
  if (WIFI_MODE_ON_BOOT != 0 && WIFI_MODE_ON_BOOT != -1) {
    wifiDoc.clear();
    wifiDoc["wifi_mode_on_boot"] = WIFI_MODE_ON_BOOT;
    wifiDoc["sta_ssid"]     = sta_ssid;
    wifiDoc["sta_password"] = sta_password;
    wifiDoc["ap_ssid"]      = ap_ssid;
    wifiDoc["ap_password"]  = ap_password;

    File configFile = LittleFS.open("/wifiConfig.json", "w");
    if (configFile) {
      serializeJson(wifiDoc, configFile);
      configFile.close();
      if (InfoPrint == 1) { Serial.println("/wifiConfig.json saved."); }
      return true;
    } else {
      if (InfoPrint == 1) { Serial.println("/wifiConfig.json open failed."); }
      return false;
    }
  } else {
    if (InfoPrint == 1) { Serial.println("Not saving: wifi_mode_on_boot is 0 or -1."); }
    return false;
  }
}


bool wifiModeAP(const char* input_ssid, const char* input_password) {
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(input_ssid, input_password);
  if (InfoPrint == 1) {
    Serial.println("AP mode starts...");
    Serial.print("SSID: ");     Serial.println(input_ssid);
    Serial.print("Password: "); Serial.println(input_password);
    Serial.println("AP Address: 192.168.4.1");
  }
  WIFI_CURRENT_MODE = 1;
  localIP = WiFi.localIP();
  ap_ssid     = input_ssid;
  ap_password = input_password;
  updateOledWifiInfo();

  jsonInfoHttp.clear();
  jsonInfoHttp["info"]       = "AP mode starts";
  jsonInfoHttp["ap_ssid"]    = ap_ssid;
  jsonInfoHttp["ap_password"] = ap_password;
  return true;
}


bool wifiModeSTA(const char* input_ssid, const char* input_password) {
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(input_ssid, input_password);
  connectionStartTime = millis();

  if (InfoPrint == 1) {
    Serial.print("STA mode: connecting to ");
    Serial.println(input_ssid);
  }

  while (WiFi.status() != WL_CONNECTED) {
    unsigned long currentTime = millis();
    if (InfoPrint == 1) { Serial.print("."); }
    delay(500);
    if (currentTime - connectionStartTime >= connectionTimeout) {
      WIFI_CURRENT_MODE = -1;
      if (InfoPrint == 1) { Serial.println("\nSTA connection timeout."); }
      wifiModeAP(ap_ssid, ap_password);
      updateOledWifiInfo();
      jsonInfoHttp.clear();
      jsonInfoHttp["info"] = "STA connection timeout.";
      return false;
    }
  }

  if (InfoPrint == 1) { Serial.println("\nSTA connection succeed."); }
  WIFI_CURRENT_MODE = 2;
  getIPAddress(WIFI_CURRENT_MODE);
  sta_ssid     = input_ssid;
  sta_password = input_password;

  jsonInfoHttp.clear();
  jsonInfoHttp["info"]          = "STA connection succeed.";
  jsonInfoHttp["wifi_mode_on_boot"] = WIFI_MODE_ON_BOOT;
  jsonInfoHttp["sta_ssid"]      = sta_ssid;
  jsonInfoHttp["ap_ssid"]       = ap_ssid;

  if (defaultModeToAPSTA && !wifiConfigFound) {
    WIFI_MODE_ON_BOOT = 3;
    createWifiConfigFileByStatus();
  }
  updateOledWifiInfo();
  return true;
}


bool wifiModeAPSTA(const char* input_ap_ssid, const char* input_ap_password,
                   const char* input_sta_ssid, const char* input_sta_password) {
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(input_ap_ssid, input_ap_password);
  if (InfoPrint == 1) {
    Serial.print("AP+STA: AP="); Serial.println(input_ap_ssid);
  }
  ap_ssid     = input_ap_ssid;
  ap_password = input_ap_password;

  WiFi.begin(input_sta_ssid, input_sta_password);
  connectionStartTime = millis();
  if (InfoPrint == 1) {
    Serial.print("AP+STA: connecting STA to ");
    Serial.println(input_sta_ssid);
  }

  while (WiFi.status() != WL_CONNECTED) {
    unsigned long currentTime = millis();
    if (InfoPrint == 1) { Serial.print("."); }
    delay(500);
    if (currentTime - connectionStartTime >= connectionTimeout) {
      WIFI_CURRENT_MODE = -1;
      if (InfoPrint == 1) { Serial.println("\nSTA connection timeout."); }
      wifiModeAP(ap_ssid, ap_password);
      updateOledWifiInfo();
      jsonInfoHttp.clear();
      jsonInfoHttp["info"] = "STA connection timeout.";
      return false;
    }
  }

  if (InfoPrint == 1) { Serial.println("\nSTA connection succeed."); }
  WIFI_CURRENT_MODE = 3;
  getIPAddress(WIFI_CURRENT_MODE);
  sta_ssid     = input_sta_ssid;
  sta_password = input_sta_password;

  if (defaultModeToAPSTA && !wifiConfigFound) {
    WIFI_MODE_ON_BOOT = 3;
    createWifiConfigFileByStatus();
  }
  updateOledWifiInfo();

  jsonInfoHttp.clear();
  jsonInfoHttp["info"]          = "AP+STA connection succeed.";
  jsonInfoHttp["wifi_mode_on_boot"] = WIFI_MODE_ON_BOOT;
  jsonInfoHttp["sta_ssid"]      = sta_ssid;
  jsonInfoHttp["ap_ssid"]       = ap_ssid;
  return true;
}


void wifiStop() {
  WiFi.disconnect();
  WIFI_CURRENT_MODE = 0;
  WiFi.mode(WIFI_AP_STA);
  updateOledWifiInfo();
}


bool wifiModeOnBoot() {
  bool ok = false;
  switch (WIFI_MODE_ON_BOOT) {
  case 0:
    if (InfoPrint == 1) { Serial.println("wifi mode on boot: OFF"); }
    WIFI_CURRENT_MODE = 0;
    WiFi.mode(WIFI_AP_STA);
    ok = true;
    break;
  case 1:
    ok = wifiModeAP(ap_ssid, ap_password);
    break;
  case 2:
    ok = wifiModeSTA(sta_ssid, sta_password);
    break;
  case 3:
    ok = wifiModeAPSTA(ap_ssid, ap_password, sta_ssid, sta_password);
    break;
  }
  return ok;
}


void configWifiModeOnBoot(byte inputMode) {
  WIFI_MODE_ON_BOOT = inputMode;
  if (InfoPrint == 1) {
    Serial.print("wifi_mode_on_boot: ");
    Serial.println(WIFI_MODE_ON_BOOT);
  }
  createWifiConfigFileByStatus();
}


void createWifiConfigFileByInput(byte inputMode,
                                  const char* inputApSsid,
                                  const char* inputApPassword,
                                  const char* inputStaSsid,
                                  const char* inputStaPassword) {
  WIFI_MODE_ON_BOOT = inputMode;
  wifiModeAPSTA(inputApSsid, inputApPassword, inputStaSsid, inputStaPassword);
  if (InfoPrint == 1) {
    Serial.print("wifi_mode_on_boot: ");
    Serial.println(WIFI_MODE_ON_BOOT);
  }
  createWifiConfigFileByStatus();
}


// Sends WiFi status to both USB and RPi UART (jsPrint).
void wifiStatusFeedback() {
  getThisDevMacAddress();
  jsonInfoHttp.clear();
  jsonInfoHttp["T"]              = CMD_WIFI_INFO;
  jsonInfoHttp["ip"]             = localIP.toString();
  jsonInfoHttp["rssi"]           = WiFi.RSSI();
  jsonInfoHttp["wifi_mode_on_boot"] = WIFI_MODE_ON_BOOT;
  jsonInfoHttp["sta_ssid"]       = sta_ssid;
  jsonInfoHttp["ap_ssid"]        = ap_ssid;
  jsonInfoHttp["mac"]            = thisMacStr;

  String getInfoJsonString;
  serializeJson(jsonInfoHttp, getInfoJsonString);
  jsPrint(getInfoJsonString);
}


void initWifi() {
  loadWifiConfig();
  wifiModeOnBoot();
  getThisDevMacAddress();
}
