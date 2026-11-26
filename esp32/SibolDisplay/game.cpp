#include "game.h"
#include "ui.h"
#include "board.h"
#include <Arduino.h>
#include <Preferences.h>

// =====================================================
// Palay Defense
//   Plant SILI (shoots seeds), PALAY (earns seeds) and BAKOD (bamboo fence)
//   to stop KUHOL (golden apple snails) and DAGA (rats) reaching the rice field.
// =====================================================

#define ROWS 5
#define COLS 9
#define GX 40          // grid position on screen
#define GY 98
#define CW 84          // cell size
#define CH 76
#define GRID_W (COLS * CW)
#define GRID_H (ROWS * CH)
#define MAX_ENEMIES 40
#define MAX_SHOTS 60
#define TICK_MS 33

enum PlantType { P_NONE = 0, P_SILI, P_PALAY, P_BAKOD };
struct PlantInfo { const char* name; int cost; int hp; uint32_t color; };
static const PlantInfo PLANTS[] = {
  { "", 0, 0, 0 },
  { "Sili", 50, 6, 0xD93B2B },
  { "Palay", 25, 4, 0xE0B21E },
  { "Bakod", 50, 22, 0x6B8E23 },
};

struct Plant { PlantType type; int hp; float timer; lv_obj_t* obj; };
struct Enemy { bool alive; bool rat; float x; int row; int hp; int maxHp; float atk; lv_obj_t* obj; lv_obj_t* bar; };
struct Shot { bool alive; float x; int row; lv_obj_t* obj; };

static Plant plants[ROWS][COLS];
static Enemy enemies[MAX_ENEMIES];
static Shot shots[MAX_SHOTS];

static lv_obj_t* grid;
static lv_obj_t* lblSeeds;
static lv_obj_t* lblWave;
static lv_obj_t* lblScore;
static lv_obj_t* lblMsg;
static lv_obj_t* btnPlant[4];
static lv_obj_t* btnPause;
static lv_obj_t* overlay;
static lv_obj_t* overlayTitle;
static lv_obj_t* overlayText;
static lv_obj_t* overlayBtnLabel;
static lv_timer_t* timer;

static PlantType selected = P_SILI;
static int seeds, score, wave, best;
static float waveTime, spawnTimer, passiveTimer, msgTimer;
static bool running = false, started = false, over = false;
static uint32_t lastTick;

// =====================================================
// Helpers
// =====================================================
static lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color, int radius) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

static lv_obj_t* text(lv_obj_t* parent, const char* t, const lv_font_t* f, uint32_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, t);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  return l;
}

static void showMsg(const char* m) {
  lv_label_set_text(lblMsg, m);
  msgTimer = 1.8f;
}

static void updateHud() {
  lv_label_set_text_fmt(lblSeeds, LV_SYMBOL_CHARGE " %d seeds", seeds);
  lv_label_set_text_fmt(lblWave, "Wave %d", wave);
  lv_label_set_text_fmt(lblScore, "Score %d  |  Best %d", score, best);
  for (int i = 1; i <= 3; i++) {
    bool afford = seeds >= PLANTS[i].cost;
    lv_obj_set_style_bg_color(btnPlant[i], lv_color_hex(i == selected ? PLANTS[i].color : 0x8A9A86), 0);
    lv_obj_set_style_bg_opa(btnPlant[i], afford ? LV_OPA_COVER : LV_OPA_50, 0);
  }
}

// =====================================================
// Entities
// =====================================================
static void placePlant(int r, int c, PlantType t) {
  Plant& p = plants[r][c];
  p.type = t;
  p.hp = PLANTS[t].hp;
  p.timer = t == P_PALAY ? 6.0f : 0.5f;
  int w = t == P_BAKOD ? 30 : 58;
  int h = t == P_BAKOD ? 66 : 58;
  p.obj = box(grid, c * CW + (CW - w) / 2, r * CH + (CH - h) / 2, w, h, PLANTS[t].color, t == P_BAKOD ? 6 : 29);
  lv_obj_set_style_border_width(p.obj, 3, 0);
  lv_obj_set_style_border_color(p.obj, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_opa(p.obj, LV_OPA_60, 0);
  if (t != P_BAKOD) {
    lv_obj_t* l = text(p.obj, PLANTS[t].name, &lv_font_montserrat_14, 0xFFFFFF);
    lv_obj_center(l);
  }
}

static void removePlant(int r, int c) {
  Plant& p = plants[r][c];
  if (p.obj) lv_obj_delete(p.obj);
  p.obj = nullptr;
  p.type = P_NONE;
}

static void spawnEnemy() {
  for (Enemy& e : enemies) {
    if (e.alive) continue;
    e.alive = true;
    e.rat = random(100) < min(15 + wave * 8, 60);
    e.row = random(ROWS);
    e.x = GRID_W - 4;
    e.maxHp = (e.rat ? 3 : 6) + wave / 3;
    e.hp = e.maxHp;
    e.atk = 0;
    int w = e.rat ? 66 : 62, h = e.rat ? 38 : 46;
    e.obj = box(grid, (int)e.x, e.row * CH + (CH - h) / 2, w, h, e.rat ? 0x6E6E6E : 0x9B6A2F, e.rat ? 18 : 22);
    lv_obj_t* l = text(e.obj, e.rat ? "Daga" : "Kuhol", &lv_font_montserrat_14, 0xFFFFFF);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, -3);
    e.bar = box(e.obj, 8, h - 8, w - 16, 4, 0x7CFC00, 2);
    return;
  }
}

static void killEnemy(Enemy& e) {
  e.alive = false;
  if (e.obj) lv_obj_delete(e.obj);
  e.obj = nullptr;
}

static void shoot(int row, float x) {
  for (Shot& s : shots) {
    if (s.alive) continue;
    s.alive = true;
    s.row = row;
    s.x = x;
    s.obj = box(grid, (int)x, row * CH + CH / 2 - 7, 14, 14, 0x3AA335, 7);
    return;
  }
}

static void killShot(Shot& s) {
  s.alive = false;
  if (s.obj) lv_obj_delete(s.obj);
  s.obj = nullptr;
}

// =====================================================
// Game flow
// =====================================================
static void showOverlay(const char* title, const char* body, const char* btn) {
  lv_label_set_text(overlayTitle, title);
  lv_label_set_text(overlayText, body);
  lv_label_set_text(overlayBtnLabel, btn);
  lv_obj_remove_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(overlay);
}

static void resetGame() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) removePlant(r, c);
  for (Enemy& e : enemies) if (e.alive) killEnemy(e);
  for (Shot& s : shots) if (s.alive) killShot(s);
  seeds = 100;
  score = 0;
  wave = 1;
  waveTime = 0;
  spawnTimer = 6;
  passiveTimer = 0;
  over = false;
  selected = P_SILI;
  lv_label_set_text(lblMsg, "");
  updateHud();
}

static void gameOver() {
  over = true;
  running = false;
  if (score > best) {
    best = score;
    Preferences prefs;
    prefs.begin("palay", false);
    prefs.putInt("best", best);
    prefs.end();
  }
  char buf[160];
  snprintf(buf, sizeof(buf), "The pests reached the rice field!\n\nScore: %d   (Best: %d)\nYou reached wave %d.", score, best, wave);
  showOverlay("Game Over", buf, LV_SYMBOL_REFRESH "  Play again");
  updateHud();
}

static void tick(lv_timer_t* t) {
  (void)t;
  uint32_t now = millis();
  float dt = (now - lastTick) / 1000.0f;
  lastTick = now;
  if (!running) return;
  if (dt > 0.1f) dt = 0.1f;

  // economy + waves
  passiveTimer += dt;
  if (passiveTimer >= 10) { passiveTimer = 0; seeds += 25; }
  waveTime += dt;
  if (waveTime >= 25) { waveTime = 0; wave++; showMsg("A bigger wave is coming!"); }
  spawnTimer -= dt;
  if (spawnTimer <= 0) {
    spawnEnemy();
    if (wave >= 4 && random(100) < 30) spawnEnemy();
    float gap = 7.5f - wave * 0.6f;
    spawnTimer = gap < 2.2f ? 2.2f : gap;
  }
  if (msgTimer > 0) { msgTimer -= dt; if (msgTimer <= 0) lv_label_set_text(lblMsg, ""); }

  // plants
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      Plant& p = plants[r][c];
      if (p.type == P_SILI) {
        bool target = false;
        for (Enemy& e : enemies) if (e.alive && e.row == r && e.x > c * CW) { target = true; break; }
        p.timer -= dt;
        if (target && p.timer <= 0) { shoot(r, c * CW + CW - 20); p.timer = 1.4f; }
      } else if (p.type == P_PALAY) {
        p.timer -= dt;
        if (p.timer <= 0) { seeds += 25; p.timer = 9; showMsg("+25 seeds from Palay"); }
      }
    }
  }

  // enemies
  for (Enemy& e : enemies) {
    if (!e.alive) continue;
    int col = (int)((e.x + 8) / CW);
    Plant* blocker = (col >= 0 && col < COLS && plants[e.row][col].type != P_NONE) ? &plants[e.row][col] : nullptr;
    if (blocker) {
      e.atk -= dt;
      if (e.atk <= 0) {
        e.atk = e.rat ? 0.6f : 0.9f;
        blocker->hp--;
        if (blocker->hp <= 0) removePlant(e.row, col);
        else lv_obj_set_style_bg_opa(blocker->obj, (lv_opa_t)(120 + 135 * blocker->hp / PLANTS[blocker->type].hp), 0);
      }
    } else {
      float speed = (e.rat ? 30.0f : 13.0f) * (1 + wave * 0.04f);
      e.x -= speed * dt;
      lv_obj_set_x(e.obj, (int)e.x);
    }
    if (e.x < -30) { gameOver(); return; }
  }

  // shots
  for (Shot& s : shots) {
    if (!s.alive) continue;
    s.x += 320 * dt;
    if (s.x > GRID_W) { killShot(s); continue; }
    lv_obj_set_x(s.obj, (int)s.x);
    for (Enemy& e : enemies) {
      if (!e.alive || e.row != s.row) continue;
      if (s.x + 14 >= e.x && s.x <= e.x + 60) {
        killShot(s);
        e.hp--;
        if (e.hp <= 0) {
          score += e.rat ? 15 : 10;
          seeds += 10;
          killEnemy(e);
        } else {
          int w = e.rat ? 66 : 62;
          lv_obj_set_width(e.bar, (w - 16) * e.hp / e.maxHp);
        }
        break;
      }
    }
  }

  updateHud();
}

// =====================================================
// Input
// =====================================================
static void onGridClick(lv_event_t* e) {
  (void)e;
  if (!running) return;
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  int c = (p.x - GX) / CW;
  int r = (p.y - GY) / CH;
  if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return;
  if (plants[r][c].type != P_NONE) { showMsg("That spot is taken"); return; }
  int cost = PLANTS[selected].cost;
  if (seeds < cost) { showMsg("Not enough seeds"); return; }
  seeds -= cost;
  placePlant(r, c, selected);
  updateHud();
}

static void onSelect(lv_event_t* e) {
  selected = (PlantType)(intptr_t)lv_event_get_user_data(e);
  updateHud();
}

static void onPause(lv_event_t* e) {
  (void)e;
  if (!started || over) return;
  running = !running;
  lv_label_set_text(lv_obj_get_child(btnPause, 0), running ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  if (!running) showOverlay("Paused", "Take a break! Your field is waiting.", LV_SYMBOL_PLAY "  Resume");
}

static void onOverlayBtn(lv_event_t* e) {
  (void)e;
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  if (!started || over) {
    resetGame();
    started = true;
  }
  running = true;
  lastTick = millis();
  lv_label_set_text(lv_obj_get_child(btnPause, 0), LV_SYMBOL_PAUSE);
}

// =====================================================
// Build
// =====================================================
void gameBuild(lv_obj_t* scr) {
  Preferences prefs;
  prefs.begin("palay", true);
  best = prefs.getInt("best", 0);
  prefs.end();

  lv_obj_t* right;
  uiTopBar(scr, "Palay Defense", &right);
  lblScore = right;

  // toolbar
  lv_obj_t* bar = lv_obj_create(scr);
  lv_obj_set_pos(bar, 0, 50);
  lv_obj_set_size(bar, LCD_W, 46);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0xE8F2EE), 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 0, 0);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  lblSeeds = text(bar, "", &lv_font_montserrat_16, C_DARK);
  lv_obj_align(lblSeeds, LV_ALIGN_LEFT_MID, 10, 0);
  for (int i = 1; i <= 3; i++) {
    char t[24];
    snprintf(t, sizeof(t), "%s %d", PLANTS[i].name, PLANTS[i].cost);
    btnPlant[i] = uiButton(bar, t, PLANTS[i].color, onSelect, (void*)(intptr_t)i);
    lv_obj_set_size(btnPlant[i], 110, 38);
    lv_obj_align(btnPlant[i], LV_ALIGN_LEFT_MID, 150 + (i - 1) * 118, 0);
  }
  lblWave = text(bar, "", &lv_font_montserrat_16, C_DARK);
  lv_obj_align(lblWave, LV_ALIGN_LEFT_MID, 512, 0);
  lblMsg = text(bar, "", &lv_font_montserrat_14, C_RED);
  lv_obj_align(lblMsg, LV_ALIGN_LEFT_MID, 590, 0);
  btnPause = uiButton(bar, LV_SYMBOL_PAUSE, C_GREEN, onPause, nullptr);
  lv_obj_set_size(btnPause, 46, 38);
  lv_obj_align(btnPause, LV_ALIGN_RIGHT_MID, -6, 0);

  // rice field strip (what you protect)
  lv_obj_t* rice = box(scr, 0, GY, GX - 2, GRID_H, 0xE8C547, 0);
  lv_obj_t* rl = text(rice, "P\nA\nL\nA\nY", &lv_font_montserrat_16, 0x6B4E00);
  lv_obj_set_style_text_align(rl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(rl);

  // grid
  grid = lv_obj_create(scr);
  lv_obj_set_pos(grid, GX, GY);
  lv_obj_set_size(grid, GRID_W, GRID_H);
  lv_obj_set_style_bg_color(grid, lv_color_hex(0x7BB661), 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_radius(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(grid, onGridClick, LV_EVENT_CLICKED, nullptr);
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if ((r + c) % 2 == 0) box(grid, c * CW, r * CH, CW, CH, 0x6FA855, 0);

  // overlay (start / pause / game over)
  overlay = lv_obj_create(scr);
  lv_obj_set_size(overlay, 460, 270);
  lv_obj_center(overlay);
  lv_obj_set_style_radius(overlay, 16, 0);
  lv_obj_set_style_shadow_width(overlay, 30, 0);
  lv_obj_set_style_shadow_opa(overlay, LV_OPA_40, 0);
  lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  overlayTitle = text(overlay, "", &lv_font_montserrat_28, C_DARK);
  lv_obj_align(overlayTitle, LV_ALIGN_TOP_MID, 0, 4);
  overlayText = text(overlay, "", &lv_font_montserrat_16, C_TEXT);
  lv_obj_set_width(overlayText, 420);
  lv_label_set_long_mode(overlayText, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(overlayText, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(overlayText, LV_ALIGN_TOP_MID, 0, 48);
  lv_obj_t* ob = uiButton(overlay, "", C_ORANGE, onOverlayBtn, nullptr);
  lv_obj_set_size(ob, 200, 50);
  lv_obj_align(ob, LV_ALIGN_BOTTOM_MID, 0, 0);
  overlayBtnLabel = lv_obj_get_child(ob, 0);

  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) plants[r][c] = { P_NONE, 0, 0, nullptr };
  for (Enemy& e : enemies) { e.alive = false; e.obj = nullptr; }
  for (Shot& s : shots) { s.alive = false; s.obj = nullptr; }
  seeds = 100; wave = 1; score = 0;
  updateHud();

  showOverlay("Palay Defense",
              "Golden snails (Kuhol) and rats (Daga) are coming for the rice!\n"
              "Pick a plant above, then tap a square.\n"
              "Sili shoots seeds  |  Palay earns seeds  |  Bakod blocks.",
              LV_SYMBOL_PLAY "  Start");

  lastTick = millis();
  timer = lv_timer_create(tick, TICK_MS, nullptr);
  lv_timer_pause(timer);
}

void gamePause() {
  if (timer) lv_timer_pause(timer);
  if (running) {
    running = false;
    lv_label_set_text(lv_obj_get_child(btnPause, 0), LV_SYMBOL_PLAY);
    showOverlay("Paused", "Take a break! Your field is waiting.", LV_SYMBOL_PLAY "  Resume");
  }
}

void gameResume() {
  lastTick = millis();
  if (timer) lv_timer_resume(timer);
}
