// 2: flow feedback.
// 1: [default]print debug info in serial.
// 0: don't print debug info in serial.
byte InfoPrint = 1;

// 1: WAVE ROVER
// 2: UGV02(UGV)
// 3: UGV01(UGV)
byte mainType = 1;

// 0: [Base default] without gimbal.
// 2: [Gimbal default] Gimbal mounted on the UGV.
byte moduleType = 2;

// false: gimbal steady mode off.
//  true: gimbal steady mode on.
bool steadyMode = false;

// 0: turn off base info feedback flow.
// 1: turn on base info feedback flow.
bool baseFeedbackFlow = 0;

String thisMacStr;
String jsonFeedbackWeb = "";

// the uart used to control servos.
// GPIO 18 - S_RXD, GPIO 19 - S_TXD, as default.
#define S_RXD 18
#define S_TXD 19

// --- --- --- i2c Settings --- --- ---
#define S_SCL   33
#define S_SDA   32

// --- --- --- Bus Servo Settings --- --- ---
#define ST_PID_P_ADDR 21
#define ST_PID_D_ADDR 22
#define ST_PID_I_ADDR 23

#define ST_PID_ROARM_P   16
#define ST_PID_DEFAULT_P 32

#define GIMBAL_PAN_ID  2
#define GIMBAL_TILT_ID 1

#define SERVO_STOP_DELAY 3

int HEART_BEAT_DELAY = 3000;
unsigned long lastCmdRecvTime = millis();

// --- --- --- LED / IO --- --- ---
#define IO4_PIN 4
#define IO5_PIN 5

int IO4_CH = 7;
int IO5_CH = 8;

const uint16_t FREQ = 200;
const uint16_t ANALOG_WRITE_BITS = 8;
const uint16_t MAX_PWM = pow(2, ANALOG_WRITE_BITS)-1;
const uint16_t MIN_PWM = MAX_PWM/4;

// --- --- --- misc --- --- ---
int feedbackFlowExtraDelay = 0;
bool uartCmdEcho = 1;

unsigned long prev_time = 0;

// --- --- --- ugv imu --- --- ---
double icm_pitch, icm_roll, icm_yaw, icm_temp;
unsigned long last_imu_update = 0;

double qw, qx, qy, qz;
double ax, ay, az;
double mx, my, mz;
double gx, gy, gz;
