#include "ui.h"
#include "board.h"
#include "config.h"
#include "game.h"
#include "state.h"
#include <Arduino.h>
#include <climits>

// =====================================================
// Screens and widgets
// =====================================================
static lv_obj_t* scrHome;
static lv_obj_t* scrDash;
static lv_obj_t* scrChat;
static lv_obj_t* scrGame;

// home
static lv_obj_t* hClock;
static lv_obj_t* hDate;
static lv_obj_t* hStatus;
static lv_obj_t* hSoil;
static lv_obj_t* hTemp;
static lv_obj_t* hRisk;
static lv_obj_t* hPortal;
static lv_obj_t* hSettings;      // overlay
static lv_obj_t* hSettingsInfo;

// dashboard
static lv_obj_t* dTabs;
static lv_obj_t* dStatus;
static lv_obj_t* dCamImg;
static lv_obj_t* dCamBadge;
static lv_obj_t* dCamEmpty;
static lv_obj_t* dSoilVal;
static lv_obj_t* dSoilSub;
static lv_obj_t* dAirVal;
static lv_obj_t* dAirSub;
static lv_obj_t* dRiskVal;
static lv_obj_t* dRiskSub;
static lv_obj_t* dPumpState;
static lv_obj_t* dPumpSub;
static lv_obj_t* dPumpBtns[3];
static lv_obj_t* dChart;
static lv_chart_series_t* dSeries;
static lv_obj_t* dChartNote;
static lv_obj_t* dFc[7];
static lv_obj_t* dTips;
static lv_obj_t* dTipsNote;
static lv_obj_t* dTipsBtn;
static lv_image_dsc_t camDsc[2];
static bool camHasFrame = false;
static uint32_t tipsSeqSeen = 0;
static bool tipsRequested = false;

// chat
static lv_obj_t* cList;
static lv_obj_t* cInput;
static lv_obj_t* cKeyboard;
static lv_obj_t* cChips;
static lv_obj_t* cPending = nullptr;   // "thinking..." bubble
static uint32_t chatSeqSeen = 0;

// =====================================================
// Helpers
// =====================================================
lv_color_t levelColor(const char* level) {
  if (!level) return lv_color_hex(C_MUTED);
  if (!strcmp(level, "High")) return lv_color_hex(C_RED);
  if (!strcmp(level, "Moderate")) return lv_color_hex(C_YELLOW);
  if (!strcmp(level, "Low")) return lv_color_hex(C_GOOD);
  return lv_color_hex(C_MUTED);
}

static lv_obj_t* makeScreen() {
  lv_obj_t* s = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(s, lv_color_hex(C_BG), 0);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

static lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  return l;
}

static lv_obj_t* card(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_set_pos(c, x, y);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_bg_color(c, lv_color_hex(C_WHITE), 0);
  lv_obj_set_style_border_color(c, lv_color_hex(0xC9D6C5), 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_radius(c, 12, 0);
  lv_obj_set_style_pad_all(c, 10, 0);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}

lv_obj_t* uiButton(lv_obj_t* parent, const char* text, uint32_t color, lv_event_cb_t cb, void* user) {
  lv_obj_t* b = lv_button_create(parent);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
  lv_obj_set_style_radius(b, 10, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

static void onBackHome(lv_event_t* e) { (void)e; uiGoHome(); }

lv_obj_t* uiTopBar(lv_obj_t* parent, const char* title, lv_obj_t** rightSlot) {
  lv_obj_t* bar = lv_obj_create(parent);
  lv_obj_set_size(bar, LCD_W, 50);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(C_DARK), 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 0, 0);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* back = uiButton(bar, LV_SYMBOL_LEFT "  Home", C_GREEN, onBackHome, nullptr);
  lv_obj_set_size(back, 120, 38);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, 6, 0);

  lv_obj_t* t = label(bar, title, &lv_font_montserrat_20, C_WHITE);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, 0);

  if (rightSlot) {
    *rightSlot = label(bar, "", &lv_font_montserrat_14, 0xCFE3C8);
    lv_obj_align(*rightSlot, LV_ALIGN_RIGHT_MID, -10, 0);
  }
  return bar;
}

static String fmtAgo(time_t ts) {
  time_t n = nowUtc();
  if (!ts || n < 1700000000) return "-";
  long s = (long)(n - ts);
  if (s < 0) s = 0;
  if (s < 60) return String(s) + "s ago";
  if (s < 3600) return String(s / 60) + " min ago";
  if (s < 86400) return String(s / 3600) + " h ago";
  return String(s / 86400) + " d ago";
}

static const char* weatherText(int code) {
  if (code == 0) return "Clear";
  if (code <= 2) return "Partly cloudy";
  if (code == 3) return "Cloudy";
  if (code == 45 || code == 48) return "Fog";
  if (code >= 51 && code <= 57) return "Drizzle";
  if (code >= 61 && code <= 67) return "Rain";
  if (code >= 80 && code <= 82) return "Showers";
  if (code >= 95) return "Storm";
  return "Mixed";
}

static void goTo(lv_obj_t* scr) {
  g_wantCamera = false;
  lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, 150, 0, false);
}

// =====================================================
// HOME
// =====================================================
static void openDash(lv_event_t* e) {
  (void)e;
  goTo(scrDash);
  g_wantCamera = lv_tabview_get_tab_active(dTabs) == 0;
}
static void openChat(lv_event_t* e) { (void)e; goTo(scrChat); }
static void openGame(lv_event_t* e) { (void)e; goTo(scrGame); gameResume(); }

static void toggleSettings(lv_event_t* e) {
  (void)e;
  if (lv_obj_has_flag(hSettings, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(hSettings, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(hSettings, LV_OBJ_FLAG_HIDDEN);
}

static void resetWifi(lv_event_t* e) {
  (void)e;
  NetCmd c = {};
  c.type = CMD_RESET_WIFI;
  netSend(c);
  lv_label_set_text(hSettingsInfo, "Erasing WiFi and restarting...");
}

static lv_obj_t* bigButton(lv_obj_t* parent, int x, const char* icon, const char* title, const char* sub,
                           uint32_t color, lv_event_cb_t cb) {
  lv_obj_t* b = lv_button_create(parent);
  lv_obj_set_pos(b, x, 230);
  lv_obj_set_size(b, 236, 200);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
  lv_obj_set_style_radius(b, 18, 0);
  lv_obj_set_style_shadow_width(b, 12, 0);
  lv_obj_set_style_shadow_opa(b, LV_OPA_20, 0);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* i = label(b, icon, &lv_font_montserrat_40, C_WHITE);
  lv_obj_align(i, LV_ALIGN_TOP_MID, 0, 22);
  lv_obj_t* t = label(b, title, &lv_font_montserrat_28, C_WHITE);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 88);
  lv_obj_t* s = label(b, sub, &lv_font_montserrat_14, 0xE6F0E2);
  lv_obj_set_width(s, 210);
  lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(s, LV_LABEL_LONG_WRAP);
  lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 134);
  return b;
}

static void buildHome() {
  scrHome = makeScreen();

  lv_obj_t* title = label(scrHome, "SIBOL", &lv_font_montserrat_40, C_GREEN);
  lv_obj_set_pos(title, 24, 18);
  lv_obj_t* slogan = label(scrHome, "See the signs. Save the harvest.", &lv_font_montserrat_16, C_MUTED);
  lv_obj_set_pos(slogan, 26, 66);

  hClock = label(scrHome, "--:--", &lv_font_montserrat_40, C_DARK);
  lv_obj_align(hClock, LV_ALIGN_TOP_RIGHT, -70, 14);
  hDate = label(scrHome, "", &lv_font_montserrat_14, C_MUTED);
  lv_obj_align(hDate, LV_ALIGN_TOP_RIGHT, -70, 64);

  lv_obj_t* gear = uiButton(scrHome, LV_SYMBOL_SETTINGS, C_GREEN, toggleSettings, nullptr);
  lv_obj_set_size(gear, 46, 46);
  lv_obj_align(gear, LV_ALIGN_TOP_RIGHT, -12, 18);

  // status strip
  lv_obj_t* strip = card(scrHome, 24, 98, 752, 110);
  hStatus = label(strip, "Connecting...", &lv_font_montserrat_14, C_MUTED);
  lv_obj_set_pos(hStatus, 0, 0);
  hSoil = label(strip, "Soil --%", &lv_font_montserrat_28, C_BLUE);
  lv_obj_set_pos(hSoil, 0, 30);
  hTemp = label(strip, "Air --" "\xC2\xB0" "C", &lv_font_montserrat_28, C_ORANGE);
  lv_obj_set_pos(hTemp, 250, 30);
  hRisk = label(strip, "Risk --", &lv_font_montserrat_28, C_MUTED);
  lv_obj_set_pos(hRisk, 500, 30);
  hPortal = label(strip, "", &lv_font_montserrat_14, C_RED);
  lv_obj_set_pos(hPortal, 0, 68);

  bigButton(scrHome, 24, LV_SYMBOL_IMAGE, "Dashboard", "Camera, sensors, water pump, forecast", C_GREEN, openDash);
  bigButton(scrHome, 282, LV_SYMBOL_KEYBOARD, "Sibol Chat", "Ask the AI about your crop or anything", C_BLUE, openChat);
  bigButton(scrHome, 540, LV_SYMBOL_PLAY, "Palay Defense", "Protect the rice field from pests!", C_ORANGE, openGame);

  // settings overlay
  hSettings = card(scrHome, 200, 80, 400, 300);
  lv_obj_set_style_shadow_width(hSettings, 30, 0);
  lv_obj_set_style_shadow_opa(hSettings, LV_OPA_40, 0);
  lv_obj_t* st = label(hSettings, "Settings", &lv_font_montserrat_20, C_DARK);
  lv_obj_set_pos(st, 0, 0);
  hSettingsInfo = label(hSettings, "", &lv_font_montserrat_14, C_TEXT);
  lv_obj_set_width(hSettingsInfo, 370);
  lv_label_set_long_mode(hSettingsInfo, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(hSettingsInfo, 0, 34);
  lv_obj_t* rb = uiButton(hSettings, LV_SYMBOL_WIFI "  Reset WiFi", C_RED, resetWifi, nullptr);
  lv_obj_set_size(rb, 170, 44);
  lv_obj_align(rb, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t* cb = uiButton(hSettings, "Close", C_GREEN, toggleSettings, nullptr);
  lv_obj_set_size(cb, 120, 44);
  lv_obj_align(cb, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_flag(hSettings, LV_OBJ_FLAG_HIDDEN);
}

static void refreshHome(const SibolState& s, const CropModel& m) {
  time_t n = nowUtc();
  if (n > 1700000000) {
    time_t local = n;
    struct tm t;
    localtime_r(&local, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%I:%M %p", &t);
    lv_label_set_text(hClock, buf[0] == '0' ? buf + 1 : buf);
    strftime(buf, sizeof(buf), "%a, %b %d %Y", &t);
    lv_label_set_text(hDate, buf);
  }

  String st = String(s.wifi ? LV_SYMBOL_WIFI " Online" : LV_SYMBOL_WIFI " Offline") + "   |   ESP32-CAM " +
              (esp32Online(s) ? "online" : "offline") + "   |   " LOCATION_NAME;
  lv_label_set_text(hStatus, st.c_str());

  if (s.hasReading && s.soilValid) lv_label_set_text_fmt(hSoil, "Soil %d%%", (int)lroundf(s.soil));
  if (s.tempValid) lv_label_set_text_fmt(hTemp, "Air %.1f" "\xC2\xB0" "C", s.temp);
  lv_label_set_text_fmt(hRisk, "Risk %s", m.riskLevel);
  lv_obj_set_style_text_color(hRisk, levelColor(m.riskLevel), 0);

  if (s.portal && !s.wifi) {
    lv_label_set_text(hPortal, "WiFi setup: connect your phone to \"" AP_NAME "\" (pass " AP_PASS "), then open 192.168.4.1");
  } else {
    lv_label_set_text(hPortal, s.lastError[0] && !s.wifi ? s.lastError : "");
  }

}

// =====================================================
// DASHBOARD
// =====================================================
static void onTabChanged(lv_event_t* e) {
  (void)e;
  uint32_t tab = lv_tabview_get_tab_active(dTabs);
  g_wantCamera = (tab == 0) && lv_screen_active() == scrDash;
  if (tab == 2 && !tipsRequested) {
    tipsRequested = true;
    NetCmd c = {};
    c.type = CMD_TIPS;
    if (netSend(c)) {
      stateLock(); g_state.tipsBusy = true; stateUnlock();
    }
  }
}

static void onPump(lv_event_t* e) {
  int which = (int)(intptr_t)lv_event_get_user_data(e);   // 0 auto, 1 on, 2 off
  NetCmd c = {};
  c.type = CMD_PUMP;
  strlcpy(c.mode, which == 0 ? "auto" : "manual", sizeof(c.mode));
  c.command = (which == 1);
  netSend(c);
  // optimistic highlight
  stateLock();
  strlcpy(g_state.pumpMode, c.mode, sizeof(g_state.pumpMode));
  g_state.pumpCmd = c.command;
  stateUnlock();
}

static void onTipsRefresh(lv_event_t* e) {
  (void)e;
  NetCmd c = {};
  c.type = CMD_TIPS;
  if (netSend(c)) {
    stateLock(); g_state.tipsBusy = true; stateUnlock();
  }
}

static lv_obj_t* statCard(lv_obj_t* parent, int y, int h, const char* title, uint32_t color, lv_obj_t** val, lv_obj_t** sub) {
  lv_obj_t* c = card(parent, 496, y, 296, h);
  lv_obj_t* t = label(c, title, &lv_font_montserrat_14, C_MUTED);
  lv_obj_set_pos(t, 0, 0);
  *val = label(c, "--", &lv_font_montserrat_28, color);
  lv_obj_set_pos(*val, 0, 18);
  *sub = label(c, "", &lv_font_montserrat_14, C_MUTED);
  lv_obj_set_width(*sub, 270);
  lv_label_set_long_mode(*sub, LV_LABEL_LONG_DOT);
  lv_obj_align(*sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  return c;
}

static void buildDash() {
  scrDash = makeScreen();
  uiTopBar(scrDash, "Dashboard", &dStatus);

  dTabs = lv_tabview_create(scrDash);
  lv_obj_set_pos(dTabs, 0, 50);
  lv_obj_set_size(dTabs, LCD_W, LCD_H - 50);
  lv_tabview_set_tab_bar_size(dTabs, 40);
  lv_obj_add_event_cb(dTabs, onTabChanged, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_t* tLive = lv_tabview_add_tab(dTabs, "Live");
  lv_obj_t* tTrend = lv_tabview_add_tab(dTabs, "Trend & Forecast");
  lv_obj_t* tAi = lv_tabview_add_tab(dTabs, "AI Tips");
  lv_obj_t* tabs[3] = { tLive, tTrend, tAi };
  for (lv_obj_t* t : tabs) {
    lv_obj_set_style_pad_all(t, 0, 0);
    lv_obj_set_style_bg_color(t, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  }
  // swiping between tabs fights with the pump buttons; tap the tab bar instead
  lv_obj_remove_flag(lv_tabview_get_content(dTabs), LV_OBJ_FLAG_SCROLLABLE);

  // ---------- Live: camera ----------
  lv_obj_t* cam = lv_obj_create(tLive);
  lv_obj_set_pos(cam, 8, 6);
  lv_obj_set_size(cam, 480, 360);
  lv_obj_set_style_bg_color(cam, lv_color_hex(0x1A1F1A), 0);
  lv_obj_set_style_radius(cam, 10, 0);
  lv_obj_set_style_border_width(cam, 0, 0);
  lv_obj_set_style_pad_all(cam, 0, 0);
  lv_obj_set_style_clip_corner(cam, true, 0);
  lv_obj_remove_flag(cam, LV_OBJ_FLAG_SCROLLABLE);

  dCamImg = lv_image_create(cam);
  lv_obj_set_size(dCamImg, 480, 360);
  lv_obj_center(dCamImg);
  lv_image_set_inner_align(dCamImg, LV_IMAGE_ALIGN_CONTAIN);
  lv_obj_add_flag(dCamImg, LV_OBJ_FLAG_HIDDEN);

  dCamEmpty = label(cam, LV_SYMBOL_IMAGE "\nWaiting for the ESP32-CAM...", &lv_font_montserrat_16, 0xCFD8CF);
  lv_obj_set_style_text_align(dCamEmpty, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(dCamEmpty);

  dCamBadge = label(cam, "Connecting...", &lv_font_montserrat_14, C_WHITE);
  lv_obj_set_style_bg_color(dCamBadge, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(dCamBadge, LV_OPA_60, 0);
  lv_obj_set_style_pad_hor(dCamBadge, 10, 0);
  lv_obj_set_style_pad_ver(dCamBadge, 4, 0);
  lv_obj_set_style_radius(dCamBadge, 12, 0);
  lv_obj_align(dCamBadge, LV_ALIGN_TOP_LEFT, 8, 8);

  // ---------- Live: right column ----------
  statCard(tLive, 6, 82, "Soil Moisture (sensor)", C_BLUE, &dSoilVal, &dSoilSub);
  statCard(tLive, 94, 82, "Air (DHT11 sensor, live)", C_ORANGE, &dAirVal, &dAirSub);
  statCard(tLive, 182, 82, "Crop Risk", C_MUTED, &dRiskVal, &dRiskSub);

  lv_obj_t* pc = card(tLive, 496, 270, 296, 96);
  dPumpState = label(pc, "Water Pump: --", &lv_font_montserrat_16, C_TEXT);
  lv_obj_set_pos(dPumpState, 0, 0);
  dPumpSub = label(pc, "", &lv_font_montserrat_14, C_MUTED);
  lv_obj_set_width(dPumpSub, 274);
  lv_label_set_long_mode(dPumpSub, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(dPumpSub, 0, 22);
  const char* names[3] = { "Auto", "ON", "OFF" };
  for (int i = 0; i < 3; i++) {
    dPumpBtns[i] = uiButton(pc, names[i], C_MUTED, onPump, (void*)(intptr_t)i);
    lv_obj_set_size(dPumpBtns[i], 86, 36);
    lv_obj_set_pos(dPumpBtns[i], i * 92, 44);
  }

  // ---------- Trend & Forecast ----------
  lv_obj_t* chartCard = card(tTrend, 8, 6, 784, 186);
  lv_obj_t* ct = label(chartCard, "Soil moisture - daily average (last 8 days)", &lv_font_montserrat_14, C_MUTED);
  lv_obj_set_pos(ct, 0, 0);
  dChart = lv_chart_create(chartCard);
  lv_obj_set_pos(dChart, 30, 22);
  lv_obj_set_size(dChart, 730, 140);
  lv_chart_set_type(dChart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(dChart, 8);
  lv_chart_set_range(dChart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_div_line_count(dChart, 5, 8);
  dSeries = lv_chart_add_series(dChart, lv_color_hex(0x3B5FA8), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(dChart, dSeries, LV_CHART_POINT_NONE);
  for (int v = 0; v <= 100; v += 25) {
    lv_obj_t* yl = label(chartCard, String(v).c_str(), &lv_font_montserrat_14, C_MUTED);
    lv_obj_set_pos(yl, 0, 22 + 140 - (v * 140 / 100) - 8);
  }
  dChartNote = label(chartCard, "", &lv_font_montserrat_14, C_MUTED);
  lv_obj_align(dChartNote, LV_ALIGN_TOP_RIGHT, 0, 0);

  for (int i = 0; i < 7; i++) {
    lv_obj_t* f = card(tTrend, 8 + i * 112, 200, 104, 166);
    dFc[i] = label(f, "--", &lv_font_montserrat_14, C_TEXT);
    lv_obj_set_width(dFc[i], 84);
    lv_obj_set_style_text_align(dFc[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(dFc[i], LV_LABEL_LONG_WRAP);
    lv_obj_align(dFc[i], LV_ALIGN_TOP_MID, 0, 0);
  }

  // ---------- AI Tips ----------
  lv_obj_t* tc = card(tAi, 8, 6, 784, 360);
  lv_obj_add_flag(tc, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* tt = label(tc, "Recommended Actions (Gemini AI)", &lv_font_montserrat_20, C_DARK);
  lv_obj_set_pos(tt, 0, 0);
  dTipsBtn = uiButton(tc, LV_SYMBOL_REFRESH "  Refresh", C_GREEN, onTipsRefresh, nullptr);
  lv_obj_set_size(dTipsBtn, 130, 38);
  lv_obj_align(dTipsBtn, LV_ALIGN_TOP_RIGHT, 0, -4);
  dTips = label(tc, "Open this tab to get AI advice for your field.", &lv_font_montserrat_16, C_TEXT);
  lv_obj_set_width(dTips, 750);
  lv_label_set_long_mode(dTips, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(dTips, 0, 44);
  dTipsNote = label(tc, "", &lv_font_montserrat_14, C_MUTED);
  lv_obj_align(dTipsNote, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

static void swapCameraFrame() {
  int r = g_frameReady;
  if (r < 0) return;
  lv_image_dsc_t* d = &camDsc[r];
  memset(d, 0, sizeof(*d));
  d->header.magic = LV_IMAGE_HEADER_MAGIC;
  d->header.cf = LV_COLOR_FORMAT_RGB565;
  d->header.w = g_frameW;
  d->header.h = g_frameH;
  d->header.stride = g_frameW * 2;
  d->data_size = g_frameW * g_frameH * 2;
  d->data = (const uint8_t*)g_frameBuf[r];
  lv_image_set_src(dCamImg, d);
  g_frameFront = r;
  g_frameReady = -1;
  if (!camHasFrame) {
    camHasFrame = true;
    lv_obj_remove_flag(dCamImg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(dCamEmpty, LV_OBJ_FLAG_HIDDEN);
  }
}

static void refreshDash(const SibolState& s, const CropModel& m) {
  swapCameraFrame();

  lv_label_set_text_fmt(dStatus, "%s  |  ESP32 %s", s.wifi ? "WiFi OK" : "No WiFi", esp32Online(s) ? "online" : "offline");

  // camera badge
  time_t n = nowUtc();
  if (s.frameTs && n > 1700000000) {
    long age = (long)(n - s.frameTs);
    if (age < 12) lv_label_set_text(dCamBadge, LV_SYMBOL_EYE_OPEN "  LIVE");
    else lv_label_set_text(dCamBadge, ("Last frame " + fmtAgo(s.frameTs)).c_str());
  } else {
    lv_label_set_text(dCamBadge, s.wifi ? "Asking camera for frames..." : "No WiFi");
  }

  // soil
  if (s.hasReading && s.soilValid) {
    lv_label_set_text_fmt(dSoilVal, "%d%%", (int)lroundf(s.soil));
    lv_label_set_text_fmt(dSoilSub, "%s  (optimal %d-%d%%)", m.moistureLabel, OPTIMAL_MIN, OPTIMAL_MAX);
  } else {
    lv_label_set_text(dSoilVal, "--%");
    lv_label_set_text(dSoilSub, "Waiting for ESP32");
  }

  // air
  if (s.tempValid) lv_label_set_text_fmt(dAirVal, "%.1f" "\xC2\xB0" "C", s.temp);
  else lv_label_set_text(dAirVal, "--" "\xC2\xB0" "C");
  lv_label_set_text_fmt(dAirSub, "Humidity %s  |  %s", s.humValid ? (String((int)lroundf(s.hum)) + "%").c_str() : "--",
                        fmtAgo(s.readingTs).c_str());

  // risk
  lv_label_set_text(dRiskVal, m.riskLevel);
  lv_obj_set_style_text_color(dRiskVal, levelColor(m.riskLevel), 0);
  if (m.heat != INT_MIN)
    lv_label_set_text_fmt(dRiskSub, "Forecast heat %d" "\xC2\xB0" "C  |  %s", m.heat, m.stageName);
  else
    lv_label_set_text_fmt(dRiskSub, "Forecast loading  |  %s", m.stageName);

  // pump
  if (s.hasControls) {
    lv_label_set_text_fmt(dPumpState, "Water Pump: %s", s.pumpState ? "ON" : "OFF");
    lv_obj_set_style_text_color(dPumpState, lv_color_hex(s.pumpState ? C_BLUE : C_TEXT), 0);
    bool manual = !strcmp(s.pumpMode, "manual");
    const char* sub = !esp32Online(s) ? "ESP32 offline - applies when it reconnects"
                    : (manual && s.pumpCmd != s.pumpState) ? "Sending command..."
                    : manual ? "Manual control" : "Auto: waters when soil is dry";
    lv_label_set_text(dPumpSub, sub);
    uint32_t colors[3] = { C_MUTED, C_MUTED, C_MUTED };
    if (!manual) colors[0] = C_GREEN;
    else if (s.pumpCmd) colors[1] = C_BLUE;
    else colors[2] = 0x555555;
    for (int i = 0; i < 3; i++) lv_obj_set_style_bg_color(dPumpBtns[i], lv_color_hex(colors[i]), 0);
  }

  // trend
  for (int i = 0; i < 8; i++) {
    int idx = s.trendCount - 8 + i;
    lv_chart_set_value_by_id(dChart, dSeries, i, idx >= 0 ? (int32_t)lroundf(s.trend[idx]) : LV_CHART_POINT_NONE);
  }
  lv_chart_refresh(dChart);
  if (s.trendCount) lv_label_set_text_fmt(dChartNote, "Latest day: %d%%", (int)lroundf(s.trend[s.trendCount - 1]));
  else lv_label_set_text(dChartNote, "No history yet");

  // forecast
  for (int i = 0; i < 7; i++) {
    if (i >= s.fcCount) { lv_label_set_text(dFc[i], "--"); continue; }
    const ForecastDay& f = s.fc[i];
    int y, mo, d;
    sscanf(f.date, "%d-%d-%d", &y, &mo, &d);
    static const char* MON[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    char rain[16];
    if (f.rainProb >= 0) snprintf(rain, sizeof(rain), "Rain %d%%", f.rainProb);
    else strlcpy(rain, "", sizeof(rain));
    lv_label_set_text_fmt(dFc[i], "%s\n%s %d\n\n%d" "\xC2\xB0" "C\n%d" "\xC2\xB0" "C\n%s\n%s",
                          i == 0 ? "Today" : "", MON[(mo + 11) % 12], d, f.tMax, f.tMin, weatherText(f.code), rain);
    lv_obj_set_style_text_color(dFc[i], lv_color_hex(f.tMax >= 35 ? C_RED : C_TEXT), 0);
  }

  // AI tips
  if (s.tipsBusy) {
    lv_label_set_text(dTipsNote, "Gemini is analyzing your field...");
  } else if (s.tipsSeq != tipsSeqSeen) {
    tipsSeqSeen = s.tipsSeq;
    lv_label_set_text(dTips, s.tips.c_str());
    lv_label_set_text_fmt(dTipsNote, "Model: %s", s.aiModel.c_str());
  }
}

// =====================================================
// CHAT
// =====================================================
static lv_obj_t* addBubble(const char* text, bool user) {
  lv_obj_t* row = lv_obj_create(cList);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* b = lv_label_create(row);
  lv_label_set_text(b, text);
  lv_label_set_long_mode(b, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(b, &lv_font_montserrat_16, 0);
  lv_obj_set_style_max_width(b, 560, 0);
  lv_obj_set_width(b, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(b, 14, 0);
  lv_obj_set_style_pad_ver(b, 10, 0);
  lv_obj_set_style_radius(b, 14, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(user ? C_GREEN : C_WHITE), 0);
  lv_obj_set_style_text_color(b, lv_color_hex(user ? C_WHITE : C_TEXT), 0);
  if (!user) {
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(0xC9D6C5), 0);
  }
  lv_obj_align(b, user ? LV_ALIGN_TOP_RIGHT : LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_update_layout(cList);
  lv_obj_scroll_to_view(row, LV_ANIM_ON);
  return b;
}

static void sendChat(const char* text, bool withPhoto) {
  if (!text || !text[0]) return;
  if (cPending) return;   // wait for the current answer
  addBubble(text, true);
  cPending = addBubble(withPhoto ? "Looking at the camera photo..." : "Thinking...", false);
  lv_obj_set_style_text_color(cPending, lv_color_hex(C_MUTED), 0);

  NetCmd c = {};
  c.type = withPhoto ? CMD_CHAT_PHOTO : CMD_CHAT;
  strlcpy(c.text, text, sizeof(c.text));
  stateLock();
  g_state.chatBusy = true;
  stateUnlock();
  if (!netSend(c)) {
    lv_label_set_text(cPending, "Busy - please wait a moment and try again.");
    cPending = nullptr;
  }
}

static void hideKeyboard() {
  lv_obj_add_flag(cKeyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(cChips, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_state(cInput, LV_STATE_FOCUSED);
}

static void onSend(lv_event_t* e) {
  (void)e;
  String t = lv_textarea_get_text(cInput);
  t.trim();
  if (!t.length()) return;
  sendChat(t.c_str(), false);
  lv_textarea_set_text(cInput, "");
  hideKeyboard();
}

static void onInputEvent(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
    lv_obj_remove_flag(cKeyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(cChips, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(cList);
    lv_obj_scroll_to_y(cList, LV_COORD_MAX, LV_ANIM_OFF);
  }
}

static void onKeyboardEvent(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) onSend(e);
  else if (code == LV_EVENT_CANCEL) hideKeyboard();
}

static void onChip(lv_event_t* e) {
  const char* text = (const char*)lv_event_get_user_data(e);
  bool photo = strstr(text, "photo") != nullptr;
  sendChat(text, photo);
}

static void onClearChat(lv_event_t* e) {
  (void)e;
  if (cPending) return;
  lv_obj_clean(cList);
  NetCmd c = {};
  c.type = CMD_CHAT_CLEAR;
  netSend(c);
  addBubble("Hi! I'm Sibol AI. Ask me about your crop, the sensors, the water pump - or anything else.", false);
}

static const char* CHIPS[] = {
  "How is my crop today?",
  "Should I water now?",
  "Check my plant photo",
  "Ano ang forecast bukas?",
};

static void buildChat() {
  scrChat = makeScreen();
  lv_obj_t* right;
  lv_obj_t* bar = uiTopBar(scrChat, "Sibol AI Chat", &right);
  lv_obj_t* clr = uiButton(bar, LV_SYMBOL_TRASH "  New chat", C_GREEN, onClearChat, nullptr);
  lv_obj_set_size(clr, 140, 38);
  lv_obj_align(clr, LV_ALIGN_RIGHT_MID, -6, 0);

  // body: flex column so the keyboard pushes the messages up
  lv_obj_t* body = lv_obj_create(scrChat);
  lv_obj_set_pos(body, 0, 50);
  lv_obj_set_size(body, LCD_W, LCD_H - 50);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(body, 0, 0);
  lv_obj_set_style_radius(body, 0, 0);
  lv_obj_set_style_pad_all(body, 6, 0);
  lv_obj_set_style_pad_row(body, 6, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);

  cList = lv_obj_create(body);
  lv_obj_set_width(cList, lv_pct(100));
  lv_obj_set_flex_grow(cList, 1);
  lv_obj_set_style_bg_color(cList, lv_color_hex(C_MINT), 0);
  lv_obj_set_style_border_width(cList, 0, 0);
  lv_obj_set_style_radius(cList, 12, 0);
  lv_obj_set_style_pad_all(cList, 10, 0);
  lv_obj_set_style_pad_row(cList, 8, 0);
  lv_obj_set_flex_flow(cList, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(cList, LV_DIR_VER);

  cChips = lv_obj_create(body);
  lv_obj_set_size(cChips, lv_pct(100), 44);
  lv_obj_set_style_bg_opa(cChips, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cChips, 0, 0);
  lv_obj_set_style_pad_all(cChips, 0, 0);
  lv_obj_set_style_pad_column(cChips, 6, 0);
  lv_obj_set_flex_flow(cChips, LV_FLEX_FLOW_ROW);
  lv_obj_set_scroll_dir(cChips, LV_DIR_HOR);
  for (const char* chip : CHIPS) {
    lv_obj_t* b = uiButton(cChips, chip, C_GREEN_LT, onChip, (void*)chip);
    lv_obj_set_height(b, 40);
    lv_obj_set_width(b, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(b, 14, 0);
  }

  lv_obj_t* row = lv_obj_create(body);
  lv_obj_set_size(row, lv_pct(100), 50);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  cInput = lv_textarea_create(row);
  lv_textarea_set_one_line(cInput, true);
  lv_textarea_set_placeholder_text(cInput, "Type a message... (English or Tagalog)");
  lv_textarea_set_max_length(cInput, 380);
  lv_obj_set_flex_grow(cInput, 1);
  lv_obj_set_height(cInput, 50);
  lv_obj_set_style_text_font(cInput, &lv_font_montserrat_16, 0);
  lv_obj_add_event_cb(cInput, onInputEvent, LV_EVENT_ALL, nullptr);

  lv_obj_t* send = uiButton(row, LV_SYMBOL_RIGHT "  Send", C_BLUE, onSend, nullptr);
  lv_obj_set_size(send, 110, 50);

  cKeyboard = lv_keyboard_create(body);
  lv_obj_set_size(cKeyboard, lv_pct(100), 210);
  lv_keyboard_set_textarea(cKeyboard, cInput);
  lv_obj_add_event_cb(cKeyboard, onKeyboardEvent, LV_EVENT_ALL, nullptr);
  lv_obj_add_flag(cKeyboard, LV_OBJ_FLAG_HIDDEN);

  addBubble("Hi! I'm Sibol AI. Ask me about your crop, the sensors, the water pump - or anything else.", false);
}

static void refreshChat(const SibolState& s) {
  if (s.chatSeq != chatSeqSeen) {
    chatSeqSeen = s.chatSeq;
    if (cPending) {
      lv_label_set_text(cPending, s.chatReply.c_str());
      lv_obj_set_style_text_color(cPending, lv_color_hex(C_TEXT), 0);
      lv_obj_update_layout(cList);
      lv_obj_scroll_to_view(lv_obj_get_parent(cPending), LV_ANIM_ON);
      cPending = nullptr;
    }
  }
}

// =====================================================
// Refresh timer (UI thread)
// =====================================================
static void uiTimer(lv_timer_t* t) {
  (void)t;
  SibolState s = stateCopy();
  CropModel m = computeCrop(s);
  lv_obj_t* active = lv_screen_active();

  if (active == scrHome) {
    refreshHome(s, m);
    if (!lv_obj_has_flag(hSettings, LV_OBJ_FLAG_HIDDEN)) {
      char info[300];
      snprintf(info, sizeof(info),
               "WiFi: %s\nIP: %s\nBoard expander: %s\nESP32-CAM: %s\nGemini model: %s\n\n"
               "Field settings are in config.h.",
               s.wifi ? "connected" : (s.portal ? "setup hotspot " AP_NAME : "connecting..."),
               s.ip[0] ? s.ip : "-", boardExpanderName(), esp32Online(s) ? "online" : "offline",
               s.aiModel.length() ? s.aiModel.c_str() : "(not used yet)");
      lv_label_set_text(hSettingsInfo, info);
    }
  } else if (active == scrDash) {
    refreshDash(s, m);
  } else if (active == scrChat) {
    refreshChat(s);
  }
  if (active != scrChat) refreshChat(s);   // keep replies even if the user left
}

void uiGoHome() {
  gamePause();
  goTo(scrHome);
}

void uiInit() {
  lv_obj_set_style_text_font(lv_screen_active(), &lv_font_montserrat_16, 0);
  buildHome();
  buildDash();
  buildChat();
  scrGame = makeScreen();
  gameBuild(scrGame);
  lv_screen_load(scrHome);
  lv_timer_create(uiTimer, 300, nullptr);
}
