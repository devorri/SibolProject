#include "board.h"
#include <Arduino.h>
#include <Wire.h>
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_idf_version.h"

// ---------- Pins (same on 4.3 / 4.3B / 4.3C) ----------
#define PIN_I2C_SDA   8
#define PIN_I2C_SCL   9
#define PIN_TP_INT    4

#define PIN_HSYNC     46
#define PIN_VSYNC     3
#define PIN_DE        5
#define PIN_PCLK      7
// DATA0..15 = B3..B7, G2..G7, R3..R7
static const int RGB_DATA_PINS[16] = { 14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40 };

// IO expander outputs: EXIO1 = touch reset, EXIO2 = backlight, EXIO3 = LCD reset, EXIO5 = USB select
#define EXIO_BOOT     0x2C   // backlight + LCD reset released, touch held in reset
#define EXIO_RUN      0x2E   // everything released

#define LVGL_BUF_LINES 60

static esp_lcd_panel_handle_t s_panel = nullptr;
static ExpanderType s_exp = EXP_NONE;
static uint8_t s_gtAddr = 0x5D;
static bool s_touchOk = false;

// =====================================================
// I2C helpers
// =====================================================
static bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static void expanderWrite(uint8_t bits) {
  if (s_exp == EXP_CH422G) {
    Wire.beginTransmission(0x24);   // system register: enable push-pull outputs
    Wire.write(0x01);
    Wire.endTransmission();
    Wire.beginTransmission(0x38);   // output register
    Wire.write(bits);
    Wire.endTransmission();
  } else if (s_exp == EXP_CH32V003) {
    Wire.beginTransmission(0x24);   // direction register: all outputs
    Wire.write(0x02);
    Wire.write(0xFF);
    Wire.endTransmission();
    Wire.beginTransmission(0x24);   // output register
    Wire.write(0x03);
    Wire.write(bits);
    Wire.endTransmission();
  }
}

// =====================================================
// GT911 touch
// =====================================================
static bool gtRead(uint16_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(s_gtAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(s_gtAddr, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

static void gtWrite(uint16_t reg, uint8_t val) {
  Wire.beginTransmission(s_gtAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(val);
  Wire.endTransmission();
}

static void touchRead(lv_indev_t* indev, lv_indev_data_t* data) {
  (void)indev;
  static bool pressed = false;
  static int16_t lastX = 0, lastY = 0;

  if (s_touchOk) {
    uint8_t status = 0;
    if (gtRead(0x814E, &status, 1) && (status & 0x80)) {
      uint8_t points = status & 0x0F;
      if (points > 0) {
        uint8_t p[4];
        if (gtRead(0x8150, p, 4)) {
          lastX = (int16_t)(p[0] | (p[1] << 8));
          lastY = (int16_t)(p[2] | (p[3] << 8));
          if (lastX >= LCD_W) lastX = LCD_W - 1;
          if (lastY >= LCD_H) lastY = LCD_H - 1;
          pressed = true;
        }
      } else {
        pressed = false;
      }
      gtWrite(0x814E, 0);   // clear "buffer ready"
    }
  }

  data->point.x = lastX;
  data->point.y = lastY;
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void touchInit() {
  expanderWrite(EXIO_BOOT);
  delay(100);
  pinMode(PIN_TP_INT, OUTPUT);       // INT low during reset -> GT911 address 0x5D
  digitalWrite(PIN_TP_INT, LOW);
  delay(100);
  expanderWrite(EXIO_RUN);
  delay(200);
  pinMode(PIN_TP_INT, INPUT);

  if (i2cPresent(0x5D))      { s_gtAddr = 0x5D; s_touchOk = true; }
  else if (i2cPresent(0x14)) { s_gtAddr = 0x14; s_touchOk = true; }

  if (s_touchOk) {
    uint8_t id[4] = {0};
    gtRead(0x8140, id, 4);
    Serial.printf("[board] GT911 touch at 0x%02X (id %c%c%c%c)\n", s_gtAddr, id[0], id[1], id[2], id[3]);
  } else {
    Serial.println("[board] Touch controller NOT found");
  }
}

// =====================================================
// RGB panel
// =====================================================
static void panelInit() {
  esp_lcd_rgb_panel_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = 16 * 1000 * 1000;
  cfg.timings.h_res = LCD_W;
  cfg.timings.v_res = LCD_H;
  cfg.timings.hsync_pulse_width = 4;
  cfg.timings.hsync_back_porch = 8;
  cfg.timings.hsync_front_porch = 8;
  cfg.timings.vsync_pulse_width = 4;
  cfg.timings.vsync_back_porch = 8;
  cfg.timings.vsync_front_porch = 8;
  cfg.timings.flags.pclk_active_neg = 1;
  cfg.data_width = 16;
  cfg.num_fbs = 1;
  cfg.bounce_buffer_size_px = LCD_W * 10;   // bounce buffers stop the screen drifting while WiFi is busy
  cfg.hsync_gpio_num = PIN_HSYNC;
  cfg.vsync_gpio_num = PIN_VSYNC;
  cfg.de_gpio_num = PIN_DE;
  cfg.pclk_gpio_num = PIN_PCLK;
  cfg.disp_gpio_num = -1;
  for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = RGB_DATA_PINS[i];
  cfg.flags.fb_in_psram = 1;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  cfg.dma_burst_size = 64;
#else
  cfg.psram_trans_align = 64;
#endif

  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &s_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
}

// =====================================================
// LVGL glue
// =====================================================
static uint32_t lvTick() { return millis(); }

static void lvFlush(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
  esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px);
  lv_display_flush_ready(disp);
}

void boardInit() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

  if (i2cPresent(0x38))      s_exp = EXP_CH422G;     // 4.3 / 4.3B
  else if (i2cPresent(0x24)) s_exp = EXP_CH32V003;   // 4.3C
  Serial.printf("[board] IO expander: %s\n", boardExpanderName());

  touchInit();
  panelInit();

  lv_init();
  lv_tick_set_cb(lvTick);

  size_t bytes = LCD_W * LVGL_BUF_LINES * sizeof(uint16_t);
  void* buf1 = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  void* buf2 = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf1 || !buf2) Serial.println("[board] LVGL buffer alloc FAILED - is PSRAM set to OPI PSRAM?");

  lv_display_t* disp = lv_display_create(LCD_W, LCD_H);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, lvFlush);
  lv_display_set_buffers(disp, buf1, buf2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchRead);

  Serial.printf("[board] Display ready. Free heap %u, PSRAM %u\n", ESP.getFreeHeap(), ESP.getFreePsram());
}

ExpanderType boardExpander() { return s_exp; }

const char* boardExpanderName() {
  switch (s_exp) {
    case EXP_CH422G:   return "CH422G (4.3 / 4.3B)";
    case EXP_CH32V003: return "CH32V003 (4.3C)";
    default:           return "none found";
  }
}
