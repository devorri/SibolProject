#pragma once
// Shared data between the network task (core 0) and the UI (core 1).
#include <Arduino.h>
#include <time.h>

struct ForecastDay {
  char date[11];      // YYYY-MM-DD
  int tMax, tMin;
  int code;           // WMO weather code
  int rainProb;       // -1 = unknown
};

#define TREND_MAX 48

struct SibolState {
  // network
  bool wifi = false;
  bool portal = false;          // WiFi setup hotspot is running
  char ip[16] = "";
  bool timeSynced = false;

  // latest sensor reading (sensor_readings)
  bool hasReading = false;
  bool soilValid = false, tempValid = false, humValid = false;
  float soil = 0, temp = 0, hum = 0;
  time_t readingTs = 0;

  // pump / device (device_controls)
  bool hasControls = false;
  char pumpMode[8] = "auto";
  bool pumpCmd = false;
  bool pumpState = false;
  time_t lastSeen = 0;

  // forecast (Open-Meteo)
  int fcCount = 0;
  ForecastDay fc[7];

  // soil trend (oldest -> newest)
  int trendCount = 0;
  float trend[TREND_MAX];

  // camera
  time_t frameTs = 0;           // Last-Modified of the latest frame (UTC epoch)

  // AI
  bool chatBusy = false;
  uint32_t chatSeq = 0;         // bumps when a new reply is ready
  String chatReply;
  bool tipsBusy = false;
  uint32_t tipsSeq = 0;
  String tips;
  String aiModel;               // model that answered last

  char lastError[96] = "";
};

extern SibolState g_state;

void stateInit();
void stateLock();
void stateUnlock();
SibolState stateCopy();         // thread-safe snapshot

// ---------- Camera frame double buffer (RGB565) ----------
#define FRAME_MAX_W 640
#define FRAME_MAX_H 480
extern uint16_t* g_frameBuf[2];
extern volatile int g_frameFront;     // buffer the UI is showing
extern volatile int g_frameReady;     // -1 or index of a freshly decoded buffer
extern volatile int g_frameW, g_frameH;

// ---------- UI -> network requests ----------
enum NetCmdType : uint8_t { CMD_PUMP, CMD_CHAT, CMD_CHAT_PHOTO, CMD_CHAT_CLEAR, CMD_TIPS, CMD_RESET_WIFI };
struct NetCmd {
  NetCmdType type;
  char mode[8];
  bool command;
  char text[400];
};
bool netSend(const NetCmd& cmd);
extern volatile bool g_wantCamera;    // UI sets this while the camera view is visible

// ---------- Crop model (same rules as the website) ----------
struct CropModel {
  int days;                 // days after planting
  const char* stageName;    // e.g. "Grain Filling"
  const char* phase;        // e.g. "Ripening Stage"
  int stageIndex;           // 0..4, -1 if not planted
  const char* sensitivity;  // Low / Moderate / High
  int heat;                 // hottest forecast day in next 3 days, INT_MIN if none
  const char* heatLevel;    // Low / Moderate / High / "-"
  const char* moistureLabel;
  const char* riskLevel;    // Low / Moderate / High
  float riskScore;          // 0..1
  bool rainSoon;
};
CropModel computeCrop(const SibolState& s);
bool esp32Online(const SibolState& s);
time_t nowUtc();
String asciiOnly(const String& in);   // LVGL fonts are ASCII (+ degree sign)
