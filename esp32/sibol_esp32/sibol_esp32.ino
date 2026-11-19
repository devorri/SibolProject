/*
  Sibol — ESP32 field node (Supabase)
  Reads a capacitive soil moisture sensor (+ optional DHT22) and inserts a row
  into the Supabase table `sensor_readings` every SEND_INTERVAL_MS.

  Wiring
    Capacitive soil sensor v1.2/v2.0:  VCC -> 3V3, GND -> GND, AOUT -> GPIO34
    DHT22 (optional):                  VCC -> 3V3, GND -> GND, DATA -> GPIO4 (+10k pull-up to 3V3)

  Libraries (Arduino IDE > Library Manager)
    - ArduinoJson (Benoit Blanchon) v7
    - DHT sensor library (Adafruit) + Adafruit Unified Sensor   [only if USE_DHT = 1]

  Board: "ESP32 Dev Module"
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ======================= CONFIG — EDIT THESE =======================
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Same values as VITE_SUPABASE_URL / VITE_SUPABASE_ANON_KEY in the website's .env
const char* SUPABASE_URL = "https://your-project-ref.supabase.co";
const char* SUPABASE_KEY = "your-anon-or-publishable-key";
const char* DEVICE_ID    = "sibol-node-1";

const unsigned long SEND_INTERVAL_MS = 30000;  // 30 seconds

// Soil sensor calibration — read the raw value in Serial Monitor:
//   SOIL_DRY = sensor in open air, SOIL_WET = sensor dipped in water
#define SOIL_PIN 34
const int SOIL_DRY = 3200;
const int SOIL_WET = 1300;

#define USE_DHT 1        // set to 0 if you don't have a DHT22
#define DHT_PIN 4
#define DHT_TYPE DHT22
// ===================================================================

#if USE_DHT
#include <DHT.h>
DHT dht(DHT_PIN, DHT_TYPE);
#endif

unsigned long lastSend = 0;

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected. ESP32 IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWi-Fi failed, will retry.");
  }
}

int readSoilRaw() {
  long total = 0;
  for (int i = 0; i < 10; i++) {
    total += analogRead(SOIL_PIN);
    delay(10);
  }
  return total / 10;
}

float rawToPercent(int raw) {
  float pct = (float)(SOIL_DRY - raw) * 100.0 / (float)(SOIL_DRY - SOIL_WET);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

void sendReading() {
  int raw = readSoilRaw();
  float moisture = rawToPercent(raw);

  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  doc["soil_moisture"] = round(moisture * 10) / 10.0;
  doc["soil_raw"] = raw;

#if USE_DHT
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) doc["temperature"] = round(t * 10) / 10.0;
  if (!isnan(h)) doc["humidity"] = round(h * 10) / 10.0;
#endif

  String body;
  serializeJson(doc, body);
  Serial.printf("Soil raw=%d  ->  %.1f%%   payload: %s\n", raw, moisture, body.c_str());

  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();  // skips certificate check — fine for a prototype

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/sensor_readings";
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Prefer", "return=minimal");

  int code = http.POST(body);
  if (code == 201) {
    Serial.println("Saved to Supabase");
  } else if (code > 0) {
    Serial.printf("Supabase replied %d: %s\n", code, http.getString().c_str());
  } else {
    Serial.printf("POST failed: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);
#if USE_DHT
  dht.begin();
#endif
  connectWiFi();
  sendReading();
  lastSend = millis();
}

void loop() {
  connectWiFi();
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    sendReading();
  }
  delay(100);
}
