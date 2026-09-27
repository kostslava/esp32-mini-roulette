#pragma once

// ---- Wiring (ESP32-C3 SuperMini) ----
#define PIN_OLED_SDA 3
#define PIN_OLED_SCL 4
#define PIN_ENC_A 0
#define PIN_ENC_B 20
#define PIN_ENC_SW 1
#define PIN_BTN_OK 10
#define PIN_BTN_BACK 21

// ---- Display (128x64, I2C) ----
// 1 = 1.3" SH1106, 0 = 0.96" SSD1306.
#define OLED_SH1106 1
// Most modules are 0x3C; some are 0x3D (check the diag output).
#define OLED_I2C_ADDR 0x3C
// 400 kHz is safe everywhere; most modules also run fine at 800000 (smoother animations).
#define I2C_CLOCK_HZ 400000UL

// ---- Tunables ----
// Quadrature steps per click. Most encoder modules are 4; if one click moves two items, set 2.
#define ENC_STEPS_PER_DETENT 4
// Set to 1 if turning clockwise moves things the wrong way.
#define ENC_REVERSE 0

#define START_BALANCE 1000
#define AD_DURATION_MS 5000
