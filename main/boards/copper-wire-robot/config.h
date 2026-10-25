#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Copper Wire Robot (robot day dong) - based on bread-compact-wifi
// Core: ESP32-S3 N16R8 (16MB Flash / 8MB PSRAM)
// Audio simplex I2S: INMP441 mic + MAX98357 speaker amp
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX
// Mic INMP441: WS=4, SC/SCK=5, SD=6 (SD cua mic -> DIN cua ESP32)
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
// Mach am thanh MAX98357: LRC=7, BCLK=15, DIN=16 (DIN cua amp <- DOUT cua ESP32)
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_16
#else
// Duplex khong dung cho board nay (mic va amp dung chan rieng, simplex o tren)
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_7
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_16
#endif

#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_47
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39

// --- Hai man hinh OLED dung CHUNG 1 bus I2C: SDA=GPIO41, SCL=GPIO42 ---
// Ca hai module deu noi song song vao dung 2 chan nay (GND/VCC/SCL/SDA).
// Dia chi thuc te theo phan cung cua Nam (2026-10-25):
// man LON o 0x3D, man NHO o 0x3C - da khac nhau san nen khong can
// doi dien tro/jumper chon dia chi nua.
// Man lon 1.54" 128x64 SSD1309/CH1116 (tuong thich SSD1306): hien bieu cam
#define DISPLAY_SDA_PIN GPIO_NUM_41
#define DISPLAY_SCL_PIN GPIO_NUM_42
#define DISPLAY_LARGE_I2C_ADDR 0x3D
#define DISPLAY_LARGE_WIDTH    128
#define DISPLAY_LARGE_HEIGHT   64
// Man nho 0.91" 128x32 SSD1306: hien text (trang thai + chat), dia chi 0x3C
#define DISPLAY_SMALL_I2C_ADDR 0x3C
#define DISPLAY_SMALL_WIDTH    128
#define DISPLAY_SMALL_HEIGHT   32
// Tuong thich nguoc cho code cu (man chinh = man lon)
#define DISPLAY_WIDTH   DISPLAY_LARGE_WIDTH
#define DISPLAY_HEIGHT  DISPLAY_LARGE_HEIGHT
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true
#define DISPLAY_SMALL_MIRROR_X true
#define DISPLAY_SMALL_MIRROR_Y true

// --- Copper robot additions ---
// Motor driver DRV8833 (board trong anh: IN1 IN2 VCC GND IN3 SLEEP IN4,
// EN/EEP, OUT1-OUT4, jumper J1) -> 2x N20 gear motors, differential drive
// IN1=12, IN2=13 (motor trai, OUT1/OUT2), IN3=14, IN4=21 (motor phai, OUT3/OUT4)
// Tren board nay chan SLEEP/EN da duoc noi san qua jumper J1 (EEP) nen
// khong can GPIO - trong code de MOTOR_SLEEP_GPIO = NC. Neu ban thao J1
// thi phai noi SLEEP len VCC/3V3 thu cong, neu khong motor se khong chay.
#define MOTOR_LEFT_IN1  GPIO_NUM_12
#define MOTOR_LEFT_IN2  GPIO_NUM_13
#define MOTOR_RIGHT_IN1 GPIO_NUM_14
#define MOTOR_RIGHT_IN2 GPIO_NUM_21
#define MOTOR_SLEEP_GPIO GPIO_NUM_NC

// Optional HC-SR04 distance sensor (ECHO must go through 1k/2k divider to 3.3V)
// If you use VL53L0X instead, wire it to the display I2C bus (41/42) and ignore these.
#define DIST_TRIG_GPIO GPIO_NUM_17
#define DIST_ECHO_GPIO GPIO_NUM_18

#endif // _BOARD_CONFIG_H_
