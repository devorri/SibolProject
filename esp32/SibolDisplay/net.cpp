#include "net.h"
#include "state.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiManager.h>        // by tzapu
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>        // v7
#include <JPEGDEC.h>            // by Larry Bank
#include <vector>
#include "mbedtls/base64.h"
#include "esp_heap_caps.h"

static QueueHandle_t s_netQueue = nullptr;
static QueueHandle_t s_aiQueue = nullptr;
static WiFiManager wm;
static WiFiClientSecure s_supa;          // kept open (keep-alive) for Supabase

// Latest camera JPEG (shared with the AI task for "check my plant")
#define JPG_CAP (200 * 1024)
static uint8_t* s_jpg = nullptr;         // net task download buffer
static size_t s_jpgLen = 0;
static uint8_t* s_jpgShared = nullptr;   // copy for the AI task (guarded by stateLock)
static size_t s_jpgSharedLen = 0;

bool netSend(const NetCmd& cmd) {
  QueueHandle_t q = (cmd.type == CMD_PUMP || cmd.type == CMD_RESET_WIFI) ? s_netQueue : s_aiQueue;
  return q && xQueueSend(q, &cmd, 0) == pdTRUE;
}

// =====================================================
// Helpers
// =====================================================
static void setError(const char* msg) {
  stateLock();
  strlcpy(g_state.lastError, msg, sizeof(g_state.lastError));
  stateUnlock();
  Serial.printf("[net] %s\n", msg);
}

static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

// "2026-11-27T03:12:45.123+00:00" -> UTC epoch
static time_t parseIso(const char* s) {
  if (!s) return 0;
  int Y, M, D, h, m;
  float sec;
  if (sscanf(s, "%d-%d-%dT%d:%d:%f", &Y, &M, &D, &h, &m, &sec) < 6) return 0;
  int64_t t = daysFromCivil(Y, M, D) * 86400 + h * 3600 + m * 60 + (int)sec;
  const char* tz = strchr(s + 19, '+');
  int sign = 1;
  if (!tz) { tz = strchr(s + 19, '-'); sign = -1; }
  if (tz) {
    int oh = 0, om = 0;
    sscanf(tz + 1, "%d:%d", &oh, &om);
    t -= sign * (oh * 3600 + om * 60);
  }
  return (time_t)t;
}

// "Thu, 27 Nov 2026 03:12:45 GMT" -> UTC epoch
static time_t parseHttpDate(const String& s) {
  static const char* MON = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {0};
  int d, y, h, m, sec;
  if (sscanf(s.c_str(), "%*3s, %d %3s %d %d:%d:%d", &d, mon, &y, &h, &m, &sec) != 6) return 0;
  const char* p = strstr(MON, mon);
  if (!p) return 0;
  int month = (p - MON) / 3 + 1;
  return (time_t)(daysFromCivil(y, month, d) * 86400 + h * 3600 + m * 60 + sec);
}

static String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 16);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) break;
        out += c;
    }
  }
  return out;
}

// Collects an HTTP body (chunked or not) into a fixed buffer.
class BufferStream : public Stream {
 public:
  BufferStream(uint8_t* b, size_t c) : buf(b), cap(c) {}
  size_t write(uint8_t c) override {
    if (len < cap) { buf[len++] = c; return 1; }
    overflow = true;
    return 0;
  }
  size_t write(const uint8_t* d, size_t n) override {
    size_t k = (n <= cap - len) ? n : cap - len;
    memcpy(buf + len, d, k);
    len += k;
    if (k < n) overflow = true;
    return k;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  uint8_t* buf;
  size_t cap;
  size_t len = 0;
  bool overflow = false;
};

// =====================================================
// Supabase REST
// =====================================================
static void supaHeaders(HTTPClient& http) {
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " SUPABASE_KEY);
}

static int supaReq(const char* method, const String& path, const String& body, String* resp = nullptr) {
  HTTPClient http;
  http.setReuse(true);
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  if (!http.begin(s_supa, String(SUPABASE_URL) + path)) return -1;
  supaHeaders(http);
  if (body.length()) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "return=minimal");
  }
  int code = body.length() ? http.sendRequest(method, body) : http.sendRequest(method);
  String r;
  // 204 has no body: never call getString() on it (it would wait for the socket to close)
  if (code > 0 && code != 204 && http.getSize() != 0) r = http.getString();
  http.end();
  if (code <= 0) s_supa.stop();
  if (code <= 0 || code >= 400) {
    char msg[96];
    snprintf(msg, sizeof(msg), "Supabase %s -> %d", method, code);
    setError(msg);
    if (code >= 400) Serial.println(r);
  }
  if (resp) *resp = r;
  return code;
}

static void pollReading() {
  String r;
  if (supaReq("GET", "/rest/v1/sensor_readings?select=created_at,soil_moisture,temperature,humidity"
                     "&device_id=eq." DEVICE_ID "&order=created_at.desc&limit=1", "", &r) != 200) return;
  JsonDocument doc;
  if (deserializeJson(doc, r)) return;
  JsonObject row = doc[0];
  if (row.isNull()) return;
  stateLock();
  g_state.hasReading = true;
  g_state.soilValid = !row["soil_moisture"].isNull();
  g_state.tempValid = !row["temperature"].isNull();
  g_state.humValid = !row["humidity"].isNull();
  g_state.soil = row["soil_moisture"] | 0.0f;
  g_state.temp = row["temperature"] | 0.0f;
  g_state.hum = row["humidity"] | 0.0f;
  g_state.readingTs = parseIso(row["created_at"] | "");
  stateUnlock();
}

static void pollControls() {
  String r;
  if (supaReq("GET", "/rest/v1/device_controls?select=pump_mode,pump_command,pump_state,last_seen"
                     "&device_id=eq." DEVICE_ID, "", &r) != 200) return;
  JsonDocument doc;
  if (deserializeJson(doc, r)) return;
  JsonObject row = doc[0];
  if (row.isNull()) return;
  stateLock();
  g_state.hasControls = true;
  strlcpy(g_state.pumpMode, row["pump_mode"] | "auto", sizeof(g_state.pumpMode));
  g_state.pumpCmd = row["pump_command"] | false;
  g_state.pumpState = row["pump_state"] | false;
  g_state.lastSeen = parseIso(row["last_seen"] | "");
  stateUnlock();
}

static void pollTrend() {
  String r;
  if (supaReq("GET", "/rest/v1/sensor_daily?select=date,soil_moisture&order=date.desc&limit=8", "", &r) != 200) return;
  JsonDocument doc;
  if (deserializeJson(doc, r)) return;
  JsonArray arr = doc.as<JsonArray>();
  float vals[TREND_MAX];
  int n = 0;
  for (int i = (int)arr.size() - 1; i >= 0 && n < TREND_MAX; i--) {
    if (arr[i]["soil_moisture"].isNull()) continue;
    vals[n++] = arr[i]["soil_moisture"].as<float>();
  }
  stateLock();
  g_state.trendCount = n;
  memcpy(g_state.trend, vals, n * sizeof(float));
  stateUnlock();
}

static void sendPump(const char* mode, bool command) {
  String body = String("{\"pump_mode\":\"") + mode + "\",\"pump_command\":" + (command ? "true" : "false") + "}";
  supaReq("PATCH", "/rest/v1/device_controls?device_id=eq." DEVICE_ID, body);
  pollControls();
}

static void streamPing() {
  time_t until = nowUtc() + 30;
  if (until < 1700000000) return;   // clock not synced yet
  struct tm t;
  gmtime_r(&until, &t);
  char iso[32];
  strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &t);
  supaReq("PATCH", "/rest/v1/device_controls?device_id=eq." DEVICE_ID, String("{\"stream_until\":\"") + iso + "\"}");
}

// =====================================================
// Forecast (Open-Meteo, plain HTTP, no key)
// =====================================================
static void pollForecast() {
  WiFiClient plain;
  HTTPClient http;
  http.setTimeout(10000);
  char url[300];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
           "&timezone=Asia%%2FManila&forecast_days=7",
           FIELD_LAT, FIELD_LNG);
  if (!http.begin(plain, url)) return;
  int code = http.GET();
  String r = code == 200 ? http.getString() : "";
  http.end();
  if (code != 200) { setError("Forecast download failed"); return; }

  JsonDocument doc;
  if (deserializeJson(doc, r)) return;
  JsonObject d = doc["daily"];
  JsonArray time_ = d["time"];
  ForecastDay fc[7];
  int n = 0;
  for (int i = 0; i < (int)time_.size() && n < 7; i++, n++) {
    strlcpy(fc[n].date, time_[i] | "", sizeof(fc[n].date));
    fc[n].tMax = lroundf(d["temperature_2m_max"][i] | 0.0f);
    fc[n].tMin = lroundf(d["temperature_2m_min"][i] | 0.0f);
    fc[n].code = d["weather_code"][i] | 0;
    fc[n].rainProb = d["precipitation_probability_max"][i].isNull() ? -1 : (int)(d["precipitation_probability_max"][i] | 0);
  }
  stateLock();
  g_state.fcCount = n;
  memcpy(g_state.fc, fc, sizeof(fc));
  stateUnlock();
}

// =====================================================
// Camera frame: download latest.jpg, decode to RGB565
// =====================================================
static JPEGDEC s_jpeg;
static uint16_t* s_decodeTarget = nullptr;
static int s_decodeW = 0, s_decodeH = 0;
static String s_lastFrameLm;

static int jpegDraw(JPEGDRAW* p) {
  for (int y = 0; y < p->iHeight; y++) {
    int dy = p->y + y;
    if (dy >= s_decodeH) break;
    int w = p->iWidth;
    if (p->x + w > s_decodeW) w = s_decodeW - p->x;
    if (w <= 0) continue;
    memcpy(s_decodeTarget + dy * s_decodeW + p->x, p->pPixels + y * p->iWidth, w * sizeof(uint16_t));
  }
  return 1;
}

static void fetchFrame() {
  HTTPClient http;
  http.setReuse(true);
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  String url = String(FRAME_URL) + "?t=" + String(millis());
  if (!http.begin(s_supa, url)) return;
  const char* hdrs[] = { "Last-Modified" };
  http.collectHeaders(hdrs, 1);
  int code = http.GET();
  if (code != 200) {
    if (code > 0 && code != 204 && http.getSize() != 0) http.getString();
    http.end();
    if (code <= 0) s_supa.stop();
    return;
  }
  String lm = http.header("Last-Modified");
  BufferStream bs(s_jpg, JPG_CAP);
  int rc = http.writeToStream(&bs);
  http.end();
  if (rc < 0 || bs.overflow || bs.len == 0) return;
  s_jpgLen = bs.len;

  if (lm.length() && lm == s_lastFrameLm) return;   // same frame as before
  s_lastFrameLm = lm;

  // share the JPEG with the AI task
  stateLock();
  memcpy(s_jpgShared, s_jpg, s_jpgLen);
  s_jpgSharedLen = s_jpgLen;
  g_state.frameTs = parseHttpDate(lm);
  stateUnlock();

  if (!s_jpeg.openRAM(s_jpg, (int)s_jpgLen, jpegDraw)) return;
  int w = s_jpeg.getWidth(), h = s_jpeg.getHeight();
  int opts = 0;
  if (w > FRAME_MAX_W || h > FRAME_MAX_H) { opts = JPEG_SCALE_HALF; w /= 2; h /= 2; }
  if (w > FRAME_MAX_W || h > FRAME_MAX_H) { s_jpeg.close(); return; }

  int back = 1 - g_frameFront;
  s_decodeTarget = g_frameBuf[back];
  s_decodeW = w;
  s_decodeH = h;
  s_jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
  bool ok = s_jpeg.decode(0, 0, opts);
  s_jpeg.close();
  if (!ok) return;

  g_frameW = w;
  g_frameH = h;
  g_frameReady = back;   // UI picks it up and swaps
}

// =====================================================
// Network task
// =====================================================
static void netTask(void*) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  wm.setConfigPortalBlocking(false);
  wm.setConnectTimeout(20);
  bool ok = wm.autoConnect(AP_NAME, AP_PASS);
  stateLock();
  g_state.portal = !ok;
  stateUnlock();

  configTzTime("PHT-8", "pool.ntp.org", "time.google.com");
  s_supa.setInsecure();
  s_supa.setHandshakeTimeout(10);

  uint32_t tReading = 0, tControls = 0, tTrend = 0, tForecast = 0, tFrame = 0, tPing = 0;
  bool firstForecast = true, wasConnected = false;

  for (;;) {
    wm.process();
    bool conn = WiFi.status() == WL_CONNECTED;
    if (conn != wasConnected) {
      stateLock();
      g_state.wifi = conn;
      if (conn) {
        g_state.portal = false;
        strlcpy(g_state.ip, WiFi.localIP().toString().c_str(), sizeof(g_state.ip));
      }
      stateUnlock();
      if (!conn) s_supa.stop();
      Serial.printf("[net] WiFi %s %s\n", conn ? "connected" : "lost", conn ? WiFi.localIP().toString().c_str() : "");
      wasConnected = conn;
    }
    if (!g_state.timeSynced && nowUtc() > 1700000000) {
      stateLock();
      g_state.timeSynced = true;
      stateUnlock();
    }

    NetCmd cmd;
    if (xQueueReceive(s_netQueue, &cmd, 0) == pdTRUE) {
      if (cmd.type == CMD_RESET_WIFI) {
        wm.resetSettings();
        delay(300);
        ESP.restart();
      } else if (cmd.type == CMD_PUMP && conn) {
        sendPump(cmd.mode, cmd.command);
        tControls = millis();
      }
    }

    if (conn) {
      uint32_t now = millis();
      if (now - tControls >= POLL_CONTROLS_MS) { tControls = now; pollControls(); }
      if (now - tReading >= POLL_READING_MS)   { tReading = now; pollReading(); }
      if (now - tTrend >= POLL_TREND_MS || tTrend == 0) { tTrend = now ? now : 1; pollTrend(); }
      if (firstForecast || now - tForecast >= POLL_FORECAST_MS) { firstForecast = false; tForecast = now; pollForecast(); }

      if (g_wantCamera) {
        if (now - tPing >= STREAM_PING_MS || tPing == 0) { tPing = now ? now : 1; streamPing(); }
        if (g_frameReady < 0 && now - tFrame >= FRAME_GAP_MS) { tFrame = now; fetchFrame(); }
      } else {
        tPing = 0;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// =====================================================
// Gemini (AI task)
// =====================================================
static int s_modelIdx = 0;
static const int MODEL_COUNT = sizeof(GEMINI_MODELS) / sizeof(GEMINI_MODELS[0]);

struct ChatMsg { String role; String text; };
static std::vector<ChatMsg> s_history;

static bool geminiCall(const String& systemText, const String& contentsJson, String& out, String& err) {
  String body;
  body.reserve(contentsJson.length() + systemText.length() + 256);
  body = "{\"systemInstruction\":{\"parts\":[{\"text\":\"";
  body += jsonEscape(systemText);
  body += "\"}]},\"contents\":";
  body += contentsJson;
  body += ",\"generationConfig\":{\"temperature\":0.6}}";

  for (int k = 0; k < MODEL_COUNT; k++) {
    int idx = (s_modelIdx + k) % MODEL_COUNT;
    const char* model = GEMINI_MODELS[idx];
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(15);
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(40000);
    String url = String("https://generativelanguage.googleapis.com/v1beta/models/") + model + ":generateContent";
    if (!http.begin(client, url)) continue;
    http.addHeader("x-goog-api-key", GEMINI_KEY);
    http.addHeader("Content-Type", "application/json");
    unsigned long t0 = millis();
    int code = http.POST(body);
    String resp = (code > 0) ? http.getString() : "";
    http.end();
    Serial.printf("[ai] %s -> %d (%lu ms)\n", model, code, millis() - t0);

    if (code == 200) {
      JsonDocument filter;
      filter["candidates"][0]["content"]["parts"][0]["text"] = true;
      JsonDocument doc;
      if (deserializeJson(doc, resp, DeserializationOption::Filter(filter))) continue;
      String text;
      for (JsonObject part : doc["candidates"][0]["content"]["parts"].as<JsonArray>()) text += part["text"] | "";
      text.trim();
      if (!text.length()) continue;
      s_modelIdx = idx;
      out = text;
      stateLock();
      g_state.aiModel = model;
      stateUnlock();
      return true;
    }

    JsonDocument edoc;
    deserializeJson(edoc, resp);
    err = String(edoc["error"]["message"] | "");
    bool retry = code <= 0 || code == 404 || code == 429 || code >= 500;
    if (!retry) return false;
  }
  if (!err.length()) err = "All Gemini models are busy right now. Try again in a minute.";
  return false;
}

static String liveSnapshot(const SibolState& s) {
  CropModel m = computeCrop(s);
  String t;
  t += "Location: " LOCATION_NAME ", Philippines. Crop: " CROP_NAME ", planted " PLANTING_DATE ".\n";
  t += "Growth stage: " + String(m.phase) + " (" + m.stageName + "), day " + m.days + " after planting, sensitivity " + m.sensitivity + ".\n";
  t += "Soil moisture: " + (s.hasReading && s.soilValid ? String(s.soil, 0) + "%" : String("no data")) +
       " (optimal " + OPTIMAL_MIN + "-" + OPTIMAL_MAX + "%, status " + m.moistureLabel + ").\n";
  t += "Air temperature (DHT11 sensor, live): " + (s.tempValid ? String(s.temp, 1) + " C" : String("no data")) +
       ", humidity " + (s.humValid ? String(s.hum, 0) + "%" : String("no data")) + ".\n";
  t += "Forecast:";
  for (int i = 0; i < s.fcCount && i < 3; i++) {
    t += " " + String(s.fc[i].date) + " " + s.fc[i].tMin + "-" + s.fc[i].tMax + " C rain " +
         (s.fc[i].rainProb >= 0 ? String(s.fc[i].rainProb) + "%" : String("?")) + ";";
  }
  t += "\nCrop risk level: " + String(m.riskLevel) + ".\n";
  t += "Water pump: " + String(s.hasControls ? (s.pumpState ? "ON" : "OFF") : "unknown") + ", mode " + s.pumpMode +
       ". ESP32-CAM field unit is " + (esp32Online(s) ? "online" : "offline") + ".\n";
  return t;
}

static String systemPrompt(const SibolState& s) {
  String p =
    "You are Sibol AI, the assistant inside SIBOL, a crop monitoring system for Filipino farmers "
    "(slogan: See the signs. Save the harvest.).\n"
    "How SIBOL works: an ESP32-CAM unit at the field reads soil moisture (capacitive/resistive probe through an "
    "ADS1115), air temperature and humidity (DHT11), takes photos of the crop, and controls a water pump relay. "
    "In AUTO mode the pump waters when soil is below 35% and stops above 50%; it never runs more than 2 minutes "
    "at a time, then rests 1 minute. Users can also force the pump ON or OFF from the website or this 4.3 inch "
    "touchscreen (Dashboard > Pump buttons). Data is stored in Supabase. The weather forecast comes from Open-Meteo. "
    "Crop risk combines soil moisture, the hottest forecast day and how sensitive the growth stage is.\n\n"
    "LIVE DATA RIGHT NOW:\n";
  p += liveSnapshot(s);
  p += "\nRULES: Answer in the same language the user writes in (English, Tagalog or Taglish). "
       "Keep answers short (under 120 words), practical, in plain text: no markdown, no asterisks, no emojis. "
       "You cannot press buttons or control the pump yourself; tell the user which button to press. "
       "For questions outside farming or SIBOL, answer normally like a helpful general assistant.";
  return p;
}

static String historyJson(int photoIndex, const String& photoB64) {
  String s = "[";
  for (size_t i = 0; i < s_history.size(); i++) {
    if (i) s += ",";
    s += "{\"role\":\"" + s_history[i].role + "\",\"parts\":[";
    if ((int)i == photoIndex && photoB64.length()) {
      s += "{\"inline_data\":{\"mime_type\":\"image/jpeg\",\"data\":\"";
      s += photoB64;
      s += "\"}},";
    }
    s += "{\"text\":\"" + jsonEscape(s_history[i].text) + "\"}]}";
  }
  s += "]";
  return s;
}

static String photoBase64() {
  stateLock();
  size_t len = s_jpgSharedLen;
  uint8_t* tmp = len ? (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM) : nullptr;
  if (tmp) memcpy(tmp, s_jpgShared, len);
  stateUnlock();
  if (!tmp) return "";
  size_t outCap = 4 * ((len + 2) / 3) + 1, outLen = 0;
  unsigned char* b64 = (unsigned char*)heap_caps_malloc(outCap, MALLOC_CAP_SPIRAM);
  String r;
  if (b64 && mbedtls_base64_encode(b64, outCap, &outLen, tmp, len) == 0) {
    b64[outLen] = 0;
    r = (const char*)b64;
  }
  free(tmp);
  if (b64) free(b64);
  return r;
}

static void finishChat(const String& reply) {
  stateLock();
  g_state.chatReply = asciiOnly(reply);
  g_state.chatBusy = false;
  g_state.chatSeq++;
  stateUnlock();
}

static void aiTask(void*) {
  for (;;) {
    NetCmd cmd;
    if (xQueueReceive(s_aiQueue, &cmd, portMAX_DELAY) != pdTRUE) continue;

    if (cmd.type == CMD_CHAT_CLEAR) {
      s_history.clear();
      continue;
    }

    SibolState snap = stateCopy();
    if (!snap.wifi) {
      if (cmd.type == CMD_TIPS) {
        stateLock(); g_state.tipsBusy = false; g_state.tips = "No WiFi connection."; g_state.tipsSeq++; stateUnlock();
      } else {
        finishChat("I can't reach the internet right now. Check the WiFi connection.");
      }
      continue;
    }

    if (cmd.type == CMD_TIPS) {
      String contents = "[{\"role\":\"user\",\"parts\":[{\"text\":\"" + jsonEscape(
        "Give exactly 4 short, practical recommended actions for this field for the next 1-3 days, "
        "based on the live data. Number them 1. to 4., one per line, max 22 words each, plain text.") + "\"}]}]";
      String out, err;
      bool ok = geminiCall(systemPrompt(snap), contents, out, err);
      stateLock();
      g_state.tips = asciiOnly(ok ? out : ("AI unavailable: " + err));
      g_state.tipsBusy = false;
      g_state.tipsSeq++;
      stateUnlock();
      continue;
    }

    // chat / chat with photo
    String userText = String(cmd.text);
    String photo;
    if (cmd.type == CMD_CHAT_PHOTO) {
      photo = photoBase64();
      if (!photo.length()) {
        finishChat("I don't have a camera photo yet. Open the Dashboard first so the ESP32-CAM sends a picture, then ask again.");
        continue;
      }
    }

    s_history.push_back({ "user", userText });
    while (s_history.size() > 12) s_history.erase(s_history.begin());
    int photoIndex = photo.length() ? (int)s_history.size() - 1 : -1;

    String out, err;
    bool ok = geminiCall(systemPrompt(snap), historyJson(photoIndex, photo), out, err);
    if (ok) {
      s_history.push_back({ "model", out });
      finishChat(out);
    } else {
      s_history.pop_back();
      finishChat("Sorry, I couldn't answer: " + err);
    }
  }
}

// =====================================================
void netStart() {
  s_jpg = (uint8_t*)heap_caps_malloc(JPG_CAP, MALLOC_CAP_SPIRAM);
  s_jpgShared = (uint8_t*)heap_caps_malloc(JPG_CAP, MALLOC_CAP_SPIRAM);
  s_netQueue = xQueueCreate(6, sizeof(NetCmd));
  s_aiQueue = xQueueCreate(4, sizeof(NetCmd));
  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(aiTask, "ai", 16384, nullptr, 1, nullptr, 0);
}
