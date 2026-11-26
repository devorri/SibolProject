// =====================================================
// SIBOL Display — Waveshare ESP32-S3-Touch-LCD-4.3
//   Home: Dashboard | Sibol Chat (Gemini) | Palay Defense (game)
//   Data comes from the same Supabase project as the website + ESP32-CAM.
//
// Board settings (Arduino IDE, esp32 core 3.x):
//   Board: "ESP32S3 Dev Module"   PSRAM: "OPI PSRAM"   CPU: 240MHz
//   Flash Size: match YOUR board (check the boot log: 0x800000 = 8MB, 0x1000000 = 16MB)
//     8MB  -> Partition "8M with spiffs (3MB APP/1.5MB SPIFFS)"
//     16MB -> Partition "16M Flash (3MB APP/9.9MB FATFS)"
// Libraries: lvgl 9.x (+ lv_conf.h, see README), ArduinoJson 7, JPEGDEC, WiFiManager (tzapu)
// =====================================================
#include "board.h"
#include "state.h"
#include "net.h"
#include "ui.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== SIBOL Display ===");
  Serial.printf("PSRAM: %u bytes\n", ESP.getPsramSize());
  if (!ESP.getPsramSize()) Serial.println("!!! No PSRAM - set Tools > PSRAM > OPI PSRAM");

  stateInit();
  boardInit();
  uiInit();
  netStart();     // WiFi + Supabase + Gemini run on core 0
}

void loop() {
  lv_timer_handler();   // all screen drawing happens here (core 1)
  delay(5);
}
