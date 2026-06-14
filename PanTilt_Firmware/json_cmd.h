// ─────────────────────────────────────────────────────────────────────────────
// json_cmd.h — command T-code defines (gimbal-only firmware)
// Stripped: removed arm, motor, encoder, ESP-NOW, mission defines.
// ─────────────────────────────────────────────────────────────────────────────

// ── Feedback T-codes ─────────────────────────────────────────────────────────
#define FEEDBACK_BASE_INFO  1001
#define FEEDBACK_IMU_DATA   1002
#define CMD_BUS_SERVO_ERROR 1005

// ── OLED ─────────────────────────────────────────────────────────────────────
// {"T":3,"lineNum":0,"Text":"hello"}
#define CMD_OLED_CTRL    3
// {"T":-3}
#define CMD_OLED_DEFAULT -3

// ── Module type ───────────────────────────────────────────────────────────────
// {"T":4,"cmd":2}
#define CMD_MODULE_TYPE  4

// ── IMU ───────────────────────────────────────────────────────────────────────
// {"T":126}
#define CMD_GET_IMU_DATA    126
// {"T":127}
#define CMD_CALI_IMU_STEP   127
// {"T":128}
#define CMD_GET_IMU_OFFSET  128
// {"T":129,"x":-12,"y":0,"z":0}
#define CMD_SET_IMU_OFFSET  129

// ── Feedback stream ───────────────────────────────────────────────────────────
// {"T":130}
#define CMD_BASE_FEEDBACK         130
// {"T":131,"cmd":1}
#define CMD_BASE_FEEDBACK_FLOW    131
// {"T":142,"cmd":50}
#define CMD_FEEDBACK_FLOW_INTERVAL 142
// {"T":143,"cmd":0}
#define CMD_UART_ECHO_MODE        143

// ── LED ───────────────────────────────────────────────────────────────────────
// {"T":132,"IO4":255,"IO5":0}
#define CMD_LED_CTRL  132

// ── Gimbal control ───────────────────────────────────────────────────────────
// {"T":133,"X":45,"Y":20,"SPD":300,"ACC":0}
#define CMD_GIMBAL_CTRL_SIMPLE  133
// {"T":134,"X":45,"Y":20,"SX":300,"SY":300}
#define CMD_GIMBAL_CTRL_MOVE    134
// {"T":135}
#define CMD_GIMBAL_CTRL_STOP    135
// {"T":136,"cmd":3000}
#define CMD_HEART_BEAT_SET      136
// {"T":137,"s":1,"y":0}
#define CMD_GIMBAL_STEADY       137
// {"T":141,"X":1,"Y":0,"SPD":300}
#define CMD_GIMBAL_USER_CTRL    141

// ── Servo torque ─────────────────────────────────────────────────────────────
// {"T":210,"cmd":0}  off    {"T":210,"cmd":1}  on
#define CMD_TORQUE_CTRL  210

// ── File operations ──────────────────────────────────────────────────────────
// {"T":200}
#define CMD_SCAN_FILES   200
// {"T":201,"name":"f.txt","content":"x"}
#define CMD_CREATE_FILE  201
// {"T":202,"name":"f.txt"}
#define CMD_READ_FILE    202
// {"T":203,"name":"f.txt"}
#define CMD_DELETE_FILE  203
// {"T":204,"name":"f.txt","content":"x"}
#define CMD_APPEND_LINE  204
// {"T":205,"name":"f.txt","lineNum":3,"content":"x"}
#define CMD_INSERT_LINE  205
// {"T":206,"name":"f.txt","lineNum":3,"content":"x"}
#define CMD_REPLACE_LINE 206
// {"T":207,"name":"f.txt","lineNum":3}
#define CMD_READ_LINE    207
// {"T":208,"name":"f.txt","lineNum":3}
#define CMD_DELETE_LINE  208

// ── Servo settings ────────────────────────────────────────────────────────────
// {"T":501,"raw":2,"new":3}
#define CMD_SET_SERVO_ID   501
// {"T":502,"id":2}
#define CMD_SET_MIDDLE     502
// {"T":503,"id":2,"p":32}
#define CMD_SET_SERVO_PID  503

// ── System ───────────────────────────────────────────────────────────────────
// {"T":600}
#define CMD_REBOOT           600
// {"T":601}
#define CMD_FREE_FLASH_SPACE 601
// {"T":604}
#define CMD_NVS_CLEAR        604
// {"T":605,"cmd":1}
#define CMD_INFO_PRINT       605
