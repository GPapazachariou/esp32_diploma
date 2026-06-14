// ─── Raspberry Pi GPIO UART ───────────────────────────────────────────────────
// GPIO 16 was BENCB (encoder), GPIO 17 was AIN2 (motor) — both freed.
#define RPi_RX_PIN 16
#define RPi_TX_PIN 17

// ─── Servo bus (Serial1) ──────────────────────────────────────────────────────
#define S_RXD 18
#define S_TXD 19

// ─── Debug / print level ──────────────────────────────────────────────────────
// 0: no output   1: debug output   2: flow feedback
byte InfoPrint = 1;

// ─── Device type ─────────────────────────────────────────────────────────────
byte mainType   = 1;   // WAVE ROVER board (hard-coded)
byte moduleType = 2;   // pan-tilt gimbal (hard-coded)

// ─── Gimbal servo IDs ─────────────────────────────────────────────────────────
#define GIMBAL_PAN_ID  2
#define GIMBAL_TILT_ID 1

// ─── Gimbal state ─────────────────────────────────────────────────────────────
bool steadyMode    = false;  // true = stabilisation active
bool baseFeedbackFlow = 0;   // true = continuous JSON feedback stream

// ─── I2C bus (OLED, IMU, battery monitor) ────────────────────────────────────
#define S_SCL 33
#define S_SDA 32

// ─── LED output pins ──────────────────────────────────────────────────────────
#define IO4_PIN 4
#define IO5_PIN 5
int IO4_CH = 7;
int IO5_CH = 8;
const uint16_t FREQ = 200;
const uint16_t ANALOG_WRITE_BITS = 8;
const uint16_t MAX_PWM = (1 << ANALOG_WRITE_BITS) - 1;
const uint16_t MIN_PWM = MAX_PWM / 4;

// ─── Bus servo PID register addresses ────────────────────────────────────────
#define ST_PID_P_ADDR 21
#define ST_PID_D_ADDR 22
#define ST_PID_I_ADDR 23
#define ST_PID_DEFAULT_P 32
#define ST_TORQUE_MAX 1000
#define ST_TORQUE_MIN  50

// ─── Servo stop behaviour ─────────────────────────────────────────────────────
#define SERVO_STOP_DELAY 3   // ms (unused after gimbalCtrlStop fix, kept for reference)

// ─── Heartbeat watchdog ───────────────────────────────────────────────────────
// If no command is received for HEART_BEAT_DELAY ms, hold current position.
int           HEART_BEAT_DELAY   = 3000;
unsigned long lastCmdRecvTime    = 0;
bool          heartbeatStopFlag  = false;

// ─── Serial / UART settings ───────────────────────────────────────────────────
int  feedbackFlowExtraDelay = 100;
bool uartCmdEcho = true;

// ─── WiFi / MAC ───────────────────────────────────────────────────────────────
String thisMacStr;

// ─── Web / HTTP feedback buffer ───────────────────────────────────────────────
String jsonFeedbackWeb = "";

// ─── IMU data ─────────────────────────────────────────────────────────────────
double icm_pitch = 0, icm_roll = 0, icm_yaw = 0, icm_temp = 0;
unsigned long last_imu_update = 0;
double qw = 1, qx = 0, qy = 0, qz = 0;
double ax = 0, ay = 0, az = 0;
double mx = 0, my = 0, mz = 0;
double gx = 0, gy = 0, gz = 0;

// ─── Timing helper (used by ugv_advance.h) ───────────────────────────────────
unsigned long prev_time = 0;
