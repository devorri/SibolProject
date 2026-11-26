#pragma once
// =====================================================
// Sibol Display — settings (edit these)
// =====================================================

// ---------- Supabase (same project as the web + ESP32-CAM) ----------
#define SUPABASE_URL  "https://aempovkxyhykafeugfbk.supabase.co"
#define SUPABASE_KEY  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImFlbXBvdmt4eWh5a2FmZXVnZmJrIiwicm9sZSI6ImFub24iLCJpYXQiOjE3OTUwOTQxOTIsImV4cCI6MjExMDY3MDE5Mn0.xPyIdg-i5B7VxqTQHSqQ2kaiFNB64pwS6XaZbcg5GBQ"
#define DEVICE_ID     "esp32"                     // must match the ESP32-CAM sketch
#define FRAME_URL     SUPABASE_URL "/storage/v1/object/public/files/cam/" DEVICE_ID "/latest.jpg"

// ---------- Gemini ----------
#define GEMINI_KEY    "AQ.Ab8RN6Jr7EiaWxT3hLuOXY2UEtBa4FsKZZitN7tDO9Yaus_REw"
// Tried in order; the first one that answers is remembered. (3.5-flash was overloaded in testing.)
static const char* const GEMINI_MODELS[] = { "gemini-3.1-flash-lite", "gemini-3.5-flash", "gemini-3-flash-preview" };

// ---------- WiFi setup hotspot (first boot / after "Reset WiFi") ----------
#define AP_NAME       "SIBOL-Display"
#define AP_PASS       "12345678"

// ---------- Field (same as the website Settings) ----------
#define LOCATION_NAME "San Miguel, Taguig City"
#define FIELD_LAT     14.5266
#define FIELD_LNG     121.0645
#define CROP_NAME     "Rice (Palay)"
#define PLANTING_DATE "2026-08-30"      // YYYY-MM-DD
#define OPTIMAL_MIN   40                // soil moisture %
#define OPTIMAL_MAX   60

// ---------- Timing ----------
#define POLL_READING_MS   5000
#define POLL_CONTROLS_MS  2000
#define POLL_TREND_MS     60000
#define POLL_FORECAST_MS  (30UL * 60 * 1000)
#define FRAME_GAP_MS      600           // min gap between camera frames
#define STREAM_PING_MS    10000         // keep the ESP32-CAM uploading while the dashboard is open
#define ESP32_OFFLINE_S   45            // no heartbeat this long = ESP32-CAM offline
