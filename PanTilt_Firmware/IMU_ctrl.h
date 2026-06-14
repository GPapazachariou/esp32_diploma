#include "IMU.h"

// Forward declaration: jsPrint() is defined in uart_ctrl.h (included after this file).
void jsPrint(const String& msg);

EulerAngles stAngles;
IMU_ST_SENSOR_DATA_FLOAT stGyroRawData;
IMU_ST_SENSOR_DATA_FLOAT stAccelRawData;
IMU_ST_SENSOR_DATA stMagnRawData;
float temp;


void imu_init() {
  imuInit();
}


void updateIMUData() {
  imuDataGet(&stAngles, &stGyroRawData, &stAccelRawData, &stMagnRawData);

  ax = stAccelRawData.X;
  ay = stAccelRawData.Y;
  az = stAccelRawData.Z;

  mx = stMagnRawData.s16X;
  my = stMagnRawData.s16Y;
  mz = stMagnRawData.s16Z;

  gx = stGyroRawData.X;
  gy = stGyroRawData.Y;
  gz = stGyroRawData.Z;

  icm_roll  = stAngles.roll;
  icm_pitch = stAngles.pitch;
  icm_yaw   = stAngles.yaw;

  temp = temperatureRead();
}


// Bug Fix 11: these three functions were empty — callers had no way to know
// whether the command was received or silently dropped.
void imuCalibration() {
  jsPrint("{\"T\":1002,\"info\":\"IMU calibration not implemented\"}");
}

void getIMUOffset() {
  jsPrint("{\"T\":1002,\"info\":\"IMU offset get not implemented\"}");
}

void setIMUOffset(int16_t inputX, int16_t inputY, int16_t inputZ) {
  jsPrint("{\"T\":1002,\"info\":\"IMU offset set not implemented\"}");
}


void getIMUData() {
  jsonInfoHttp.clear();
  jsonInfoHttp["T"] = FEEDBACK_IMU_DATA;

  jsonInfoHttp["r"]  = icm_roll;
  jsonInfoHttp["p"]  = icm_pitch;
  jsonInfoHttp["y"]  = icm_yaw;

  jsonInfoHttp["ax"] = ax;
  jsonInfoHttp["ay"] = ay;
  jsonInfoHttp["az"] = az;

  jsonInfoHttp["gx"] = gx;
  jsonInfoHttp["gy"] = gy;
  jsonInfoHttp["gz"] = gz;

  jsonInfoHttp["mx"] = mx;
  jsonInfoHttp["my"] = my;
  jsonInfoHttp["mz"] = mz;

  jsonInfoHttp["temp"] = temp;

  String getInfoJsonString;
  serializeJson(jsonInfoHttp, getInfoJsonString);
  jsPrint(getInfoJsonString);  // → both Serial and Serial2
}
