#ifndef CONFIG_H
#define CONFIG_H

// -------- OLED Config --------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C

// -------- Pin Assignments --------
#define PIN_RELAY      27
#define PIN_YELLOW     25
#define PIN_BLUE       26
#define PIN_EBS        4

#define PIN_ASMS_ACTIVE      5
#define PIN_BRAKES_ENGAGED  12
#define PIN_THROTTLE_ENGAGED 13
#define PIN_ESTOP_ENGAGED   14
#define PIN_KILL_SWITCH     15
#define PIN_EBS_ENGAGED     18
#define PIN_ASB_OK          19
#define PIN_TS_ACTIVE       23

// -------- Serial & Timeout --------
#define UART_BAUDRATE 115200
#define JETSON_TIMEOUT 2000  // in ms

#endif
