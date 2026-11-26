#pragma once
// Waveshare ESP32-S3-Touch-LCD-4.3 (also 4.3B / 4.3C): 800x480 RGB LCD + GT911 touch + LVGL 9 glue
#include <lvgl.h>

#define LCD_W 800
#define LCD_H 480

enum ExpanderType { EXP_NONE, EXP_CH422G, EXP_CH32V003 };

void boardInit();                 // I2C, IO expander, touch, RGB panel, LVGL display + touch input
ExpanderType boardExpander();
const char* boardExpanderName();
