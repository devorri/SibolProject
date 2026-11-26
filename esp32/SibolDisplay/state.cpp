#include "state.h"
#include "config.h"
#include <climits>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "esp_heap_caps.h"

SibolState g_state;
static SemaphoreHandle_t s_mutex = nullptr;

uint16_t* g_frameBuf[2] = { nullptr, nullptr };
volatile int g_frameFront = 0;
volatile int g_frameReady = -1;
volatile int g_frameW = 0, g_frameH = 0;
volatile bool g_wantCamera = false;

void stateInit() {
  s_mutex = xSemaphoreCreateMutex();
  size_t bytes = FRAME_MAX_W * FRAME_MAX_H * sizeof(uint16_t);
  g_frameBuf[0] = (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  g_frameBuf[1] = (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!g_frameBuf[0] || !g_frameBuf[1]) Serial.println("[state] frame buffer alloc FAILED");
}

void stateLock()   { if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY); }
void stateUnlock() { if (s_mutex) xSemaphoreGive(s_mutex); }

SibolState stateCopy() {
  stateLock();
  SibolState c = g_state;
  stateUnlock();
  return c;
}

time_t nowUtc() { return time(nullptr); }

bool esp32Online(const SibolState& s) {
  if (!s.hasControls || s.lastSeen == 0) return false;
  time_t n = nowUtc();
  if (n < 1700000000) return false;   // clock not synced yet
  return (n - s.lastSeen) < ESP32_OFFLINE_S;
}

// ---------- Crop model ----------
struct Stage { const char* name; const char* phase; int from; int to; const char* sens; };
static const Stage STAGES[] = {
  { "Seedling",      "Vegetative Stage",   0,   20,     "Moderate" },
  { "Tillering",     "Vegetative Stage",   21,  45,     "Low" },
  { "Flowering",     "Reproductive Stage", 46,  85,     "High" },
  { "Grain Filling", "Ripening Stage",     86,  105,    "Moderate" },
  { "Maturity",      "Harvest Ready",      106, 100000, "Low" },
};

static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

CropModel computeCrop(const SibolState& s) {
  CropModel m;
  // days after planting (Manila date)
  int py = 0, pm = 0, pd = 0;
  sscanf(PLANTING_DATE, "%d-%d-%d", &py, &pm, &pd);
  time_t n = nowUtc() + 8 * 3600;
  int64_t today = n > 1700000000 ? (int64_t)(n / 86400) : daysFromCivil(py, pm, pd);
  m.days = (int)(today - daysFromCivil(py, pm, pd));

  m.stageIndex = -1;
  m.stageName = m.days < 0 ? "Not yet planted" : "-";
  m.phase = "";
  m.sensitivity = "Moderate";
  for (int i = 0; i < 5 && m.days >= 0; i++) {
    if (m.days >= STAGES[i].from && m.days <= STAGES[i].to) {
      m.stageIndex = i;
      m.stageName = STAGES[i].name;
      m.phase = STAGES[i].phase;
      m.sensitivity = STAGES[i].sens;
      break;
    }
  }

  // forecast heat: hottest of the next 3 days
  m.heat = INT_MIN;
  m.rainSoon = false;
  for (int i = 0; i < s.fcCount && i < 3; i++) {
    if (s.fc[i].tMax > m.heat) m.heat = s.fc[i].tMax;
    if (s.fc[i].rainProb >= 60) m.rainSoon = true;
  }
  m.heatLevel = m.heat == INT_MIN ? "-" : m.heat >= 35 ? "High" : m.heat >= 32 ? "Moderate" : "Low";

  // moisture label
  bool hasM = s.hasReading && s.soilValid;
  float mo = s.soil;
  if (!hasM) m.moistureLabel = "No data";
  else if (mo < OPTIMAL_MIN * 0.6f) m.moistureLabel = "Low";
  else if (mo < OPTIMAL_MIN) m.moistureLabel = "Slightly Low";
  else if (mo > OPTIMAL_MAX + 20) m.moistureLabel = "Saturated";
  else m.moistureLabel = "Optimal";

  // risk
  int mp = 0;
  if (hasM) {
    if (mo < OPTIMAL_MIN * 0.6f) mp = 3;
    else if (mo < OPTIMAL_MIN) mp = 2;
    else if (mo < OPTIMAL_MIN + 5) mp = 1;
  }
  int hp = 0;
  if (m.heat != INT_MIN) {
    if (m.heat >= 35) hp = 3;
    else if (m.heat >= 32) hp = 2;
    else if (m.heat >= 30) hp = 1;
  }
  float mult = strcmp(m.sensitivity, "High") == 0 ? 1.5f : strcmp(m.sensitivity, "Moderate") == 0 ? 1.0f : 0.7f;
  float raw = (mp + hp) * mult;
  m.riskScore = raw / 9.0f > 1 ? 1 : raw / 9.0f;
  m.riskLevel = raw >= 5 ? "High" : raw >= 2.5f ? "Moderate" : "Low";
  return m;
}

// ---------- Text cleanup for LVGL fonts ----------
String asciiOnly(const String& in) {
  String out;
  out.reserve(in.length());
  const char* p = in.c_str();
  while (*p) {
    uint8_t c = (uint8_t)*p;
    if (c < 0x80) {
      if (c == '*' || c == '#' || c == '`') { p++; continue; }   // strip markdown
      out += (char)c;
      p++;
      continue;
    }
    // decode UTF-8 code point
    uint32_t cp = 0;
    int extra = 0;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    p++;
    for (int i = 0; i < extra && *p; i++, p++) cp = (cp << 6) | ((uint8_t)*p & 0x3F);

    switch (cp) {
      case 0x2018: case 0x2019: out += '\''; break;
      case 0x201C: case 0x201D: out += '"'; break;
      case 0x2013: case 0x2014: out += '-'; break;
      case 0x2026: out += "..."; break;
      case 0x2022: case 0x00B7: out += '-'; break;
      case 0x00B0: out += "\xC2\xB0"; break;           // degree sign is in the font
      case 0x00A0: out += ' '; break;
      case 0x00E9: case 0x00E8: out += 'e'; break;
      case 0x00F1: out += 'n'; break;
      case 0x00D1: out += 'N'; break;
      default: break;                                   // emoji etc. dropped
    }
  }
  return out;
}
