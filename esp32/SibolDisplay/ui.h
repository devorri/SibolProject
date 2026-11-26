#pragma once
#include <lvgl.h>

// Colors shared by all screens
#define C_BG        0xF4F7F2
#define C_DARK      0x1F3D22
#define C_GREEN     0x4A7A3D
#define C_GREEN_LT  0x5B8A4C
#define C_MINT      0xE8F2EE
#define C_TEXT      0x1D1D1D
#define C_MUTED     0x6B6B6B
#define C_BLUE      0x2F8FD8
#define C_ORANGE    0xF0A020
#define C_RED       0xD93B2B
#define C_YELLOW    0xE8A317
#define C_GOOD      0x3AA335
#define C_WHITE     0xFFFFFF

void uiInit();
void uiGoHome();

// shared helpers (also used by the game)
lv_obj_t* uiTopBar(lv_obj_t* parent, const char* title, lv_obj_t** rightSlot);
lv_obj_t* uiButton(lv_obj_t* parent, const char* text, uint32_t color, lv_event_cb_t cb, void* user);
lv_color_t levelColor(const char* level);
