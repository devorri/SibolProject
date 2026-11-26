/**
 * lv_conf.h for SIBOL Display (LVGL 9.x)
 * COPY THIS FILE TO:  Documents/Arduino/libraries/lv_conf.h
 * (next to the "lvgl" folder, NOT inside it)
 */
#if 1 /* enable content */

#ifndef LV_CONF_H
#define LV_CONF_H

/* Color */
#define LV_COLOR_DEPTH 16

/* Memory: use the normal heap (PSRAM-backed on ESP32-S3) */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

/* Refresh */
#define LV_DEF_REFR_PERIOD 20
#define LV_DPI_DEF 130

/* No OS integration: LVGL only runs in loop() */
#define LV_USE_OS LV_OS_NONE

/* Image cache for camera frames */
#define LV_CACHE_DEF_SIZE 0

/* Logging off (turn on for debugging) */
#define LV_USE_LOG 0

/* Fonts used by the UI */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Widgets used */
#define LV_USE_LABEL     1
#define LV_USE_BUTTON    1
#define LV_USE_IMAGE     1
#define LV_USE_CHART     1
#define LV_USE_TABVIEW   1
#define LV_USE_TEXTAREA  1
#define LV_USE_KEYBOARD  1
#define LV_USE_BUTTONMATRIX 1

/* Layouts */
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* Theme */
#define LV_USE_THEME_DEFAULT 1

/* Demos/examples off */
#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0

#endif /* LV_CONF_H */
#endif /* enable content */
