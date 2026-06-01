#define FEEDBACK_BASE_INFO  1001
#define FEEDBACK_IMU_DATA   1002
// bus servos error feedback
// {"T":1005,"id":1,"status":0}
#define CMD_BUS_SERVO_ERROR 1005


// OLED INFO SET
// {"T":3,"lineNum":0,"Text":"putYourTextHere"}
#define CMD_OLED_CTRL	3

// OLED DEFAULT
// {"T":-3}
#define CMD_OLED_DEFAULT	-3

// MODULE TYPE
// 0: nothing   2: Gimbal
// {"T":4,"cmd":2}
#define CMD_MODULE_TYPE	4


// {"T":126}
#define CMD_GET_IMU_DATA	126

// {"T":127}
#define CMD_CALI_IMU_STEP	127

// {"T":128}
#define CMD_GET_IMU_OFFSET	128

// {"T":129,"x":-12,"y":0,"z":0}
#define CMD_SET_IMU_OFFSET	129

// {"T":130}
#define CMD_BASE_FEEDBACK 	130

// off: {"T":131,"cmd":0} [default]
//  on: {"T":131,"cmd":1}
#define CMD_BASE_FEEDBACK_FLOW   131

// set the extra delay time(ms) for feedback info
// {"T":142,"cmd":0}
#define CMD_FEEDBACK_FLOW_INTERVAL	142

// set the echo mode of recving new cmd.
// 0: [default]off  1: on
// {"T":143,"cmd":0}
#define CMD_UART_ECHO_MODE	143


// LIGHT CTRL
// {"T":132,"IO4":255,"IO5":255}
#define CMD_LED_CTRL	132

// GIMBAL CTRL(SIMPLE)
// {"T":133,"X":45,"Y":45,"SPD":0,"ACC":0}
#define CMD_GIMBAL_CTRL_SIMPLE	133

// GIMBAL CTRL MOVE
// {"T":134,"X":45,"Y":45,"SX":300,"SY":300}
#define CMD_GIMBAL_CTRL_MOVE	134

// GIMBAL CTRL STOP
// {"T":135}
#define CMD_GIMBAL_CTRL_STOP	135

// CHANGE HEART BEAT DELAY
// {"T":136,"cmd":3000}
#define CMD_HEART_BEAT_SET	136

// GIMBAL STEADY
// off: {"T":137,"s":0,"y":0}
//  on: {"T":137,"s":1,"y":0}
#define CMD_GIMBAL_STEADY	137

// GIMBAL USER CTRL
// {"T":141,"X":0,"Y":0,"SPD":300}
// -1: decrease  1: increase  0: stop  2,2: middle
#define CMD_GIMBAL_USER_CTRL	141


// torque-lock ctrl (broadcast id=254).
// off: {"T":210,"cmd":0}
//  on: {"T":210,"cmd":1}
#define CMD_TORQUE_CTRL 210


// === === === FILE CTRL === === ===

// {"T":200}
#define CMD_SCAN_FILES 200

// {"T":201,"name":"file.txt","content":"inputContentHere."}
#define CMD_CREATE_FILE 201

// {"T":202,"name":"file.txt"}
#define CMD_READ_FILE 202

// {"T":203,"name":"file.txt"}
#define CMD_DELETE_FILE 203

// {"T":204,"name":"file.txt","content":"inputContentHere."}
#define CMD_APPEND_LINE 204

// {"T":205,"name":"file.txt","lineNum":3,"content":"content"}
#define CMD_INSERT_LINE 205

// {"T":206,"name":"file.txt","lineNum":3,"content":"Content"}
#define CMD_REPLACE_LINE 206

// {"T":207,"name":"file.txt","lineNum":3}
#define CMD_READ_LINE 207

// {"T":208,"name":"file.txt","lineNum":3}
#define CMD_DELETE_LINE 208


// === === === wifi settings. === === ===

// {"T":401,"cmd":3}
#define CMD_WIFI_ON_BOOT 401

// {"T":402,"ssid":"UGV","password":"12345678"}
#define CMD_SET_AP  402

// {"T":403,"ssid":"na","password":"ps"}
#define CMD_SET_STA 403

// {"T":404,"ap_ssid":"UGV","ap_password":"12345678","sta_ssid":"na","sta_password":"ps"}
#define CMD_WIFI_APSTA   404

// {"T":405}
#define CMD_WIFI_INFO    405

// {"T":406}
#define CMD_WIFI_CONFIG_CREATE_BY_STATUS 406

// {"T":407,"mode":3,"ap_ssid":"UGV","ap_password":"12345678","sta_ssid":"na","sta_password":"ps"}
#define CMD_WIFI_CONFIG_CREATE_BY_INPUT 407

// {"T":408}
#define CMD_WIFI_STOP 408


// === === === servo settings. === === ===

// {"T":501,"raw":1,"new":2}
#define CMD_SET_SERVO_ID 501

// {"T":502,"id":2}
#define CMD_SET_MIDDLE   502

// {"T":503,"id":2,"p":32}
#define CMD_SET_SERVO_PID   503


// === === === esp32 settings. === === ===

// {"T":600}
#define CMD_REBOOT 	600

// {"T":601}
#define CMD_FREE_FLASH_SPACE	601

// {"T":604}
#define CMD_NVS_CLEAR	604

// {"T":605,"cmd":1}
#define CMD_INFO_PRINT	605
