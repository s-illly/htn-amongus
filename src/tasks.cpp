#include <Arduino.h>
#include <esp_random.h>
#include <string.h>
#include "tasks.h"
#include "buttons.h"
#include "imu.h"
#include "display.h"
#include "leds.h"

#define TASK_TIMEOUT_MS 20000

static bool doneTask[NUM_TASKS];
static int curTask = -1;          // which minigame is running (-1 = none)
static bool drewStatic = false;
static unsigned long taskStart = 0;
static int justCompleted = -1;

// per-minigame state
static int seq[6], seqPos, wireRound;      // 0: wires (multiple sequences)
static int wireShownPos = -1;              // last seqPos actually drawn
// 1: window cleaning -- wipe the badge like a sponge; harder wipe = faster squeegee + faster clean
#define WIN_NCOLS 14
#define WIN_GMAX  3.0f
static float winGrime[WIN_NCOLS];   // remaining grime per column (0..WIN_GMAX)
static int   winShade[WIN_NCOLS];   // last-drawn shade per column
static float winSqX, winPrevSqX;    // squeegee left edge, px (prev = last drawn)
static float winCleaned;            // total grime scrubbed off (0 .. NCOLS*GMAX)
static int   winPct;               // last-drawn percent
static float navX, navY, navVX, navVY, navPX, navPY;   // 2: navigate (tilt-roll)
static int navTX, navTY, navHits, navHitsShown;
// 3: rhythm (DDR) -- arrows fall in 4 lanes, hit the matching d-pad at the line
#define DDR_MAX 8
struct Arrow { bool on; int lane; float y, py; };
static Arrow ddr[DDR_MAX];
static int ddrHits, ddrShown;
static unsigned long ddrLastSpawn;

static uint16_t NAVY, WHITE, GREEN, RED, YELLOW, DIM;

static const char *taskUids[NUM_TASKS] = {
  TASK_UID_WIRES,
  TASK_UID_GARBAGE,
  TASK_UID_WINDOW_WIPE,
  TASK_UID_RHYTHM
};

void setupTasks() {
  NAVY = gfxColor(10, 12, 34); WHITE = gfxColor(255, 255, 255);
  GREEN = gfxColor(70, 210, 90); RED = gfxColor(220, 60, 60);
  YELLOW = gfxColor(240, 220, 60); DIM = gfxColor(150, 150, 170);
  resetTasks();
}

void resetTasks() {
  for (int i = 0; i < NUM_TASKS; i++) doneTask[i] = false;
  curTask = -1;
}

static void newChute();  // defined with the garbage minigame below
static void genWalls();

static void enterTask(int t) {
  curTask = t;
  drewStatic = false;
  taskStart = millis();
  justCompleted = -1;
  if (t == 0) { for (int i = 0; i < 5; i++) seq[i] = esp_random() % 6; seqPos = 0; wireRound = 0; }
  else if (t == 1) {
    navX = navPX = 55; navY = navPY = 185; navVX = navVY = 0; navHits = 0; navHitsShown = -1;
    genWalls();
    newChute();
  }
  else if (t == 2) {
    for (int i = 0; i < WIN_NCOLS; i++) { winGrime[i] = WIN_GMAX; winShade[i] = -1; }
    winSqX = 20; winPrevSqX = -1000; winCleaned = 0; winPct = -1;
  }
  else if (t == 3) { for (int i = 0; i < DDR_MAX; i++) ddr[i].on = false; ddrHits = 0; ddrShown = -1; ddrLastSpawn = 0; }
}

int taskIndexForUid(const char *uid) {
  if (!uid) return -1;
  for (int t = 0; t < NUM_TASKS; t++)
    if (strcmp(uid, taskUids[t]) == 0) return t;
  return -1;
}

bool taskIsCompleted(int t) {
  return t >= 0 && t < NUM_TASKS && doneTask[t];
}

bool taskTryStart(const char *uid) {
  if (curTask >= 0) return false;
  int t = taskIndexForUid(uid);
  if (t < 0 || doneTask[t]) return false;
  enterTask(t);
  return true;
}

void taskStartIndex(int t) {
  if (t < 0 || t >= NUM_TASKS) return;
  doneTask[t] = false;
  enterTask(t);
}

bool taskActive() { return curTask >= 0; }

void taskCancel() { curTask = -1; }

int taskJustCompleted() { int j = justCompleted; justCompleted = -1; return j; }

static void finish() {
  doneTask[curTask] = true;
  justCompleted = curTask;
  curTask = -1;
}

// ---- minigame renderers/updaters ----
// input types: 0 UP, 1 DOWN, 2 LEFT, 3 RIGHT, 4 A, 5 B
#define WIRE_LEN    5   // glyphs per sequence
#define WIRE_ROUNDS 3   // sequences to complete for the whole task

// draw one sequence glyph centered at (cx,cy): arrows for 0-3, letters for A/B
static void drawGlyph(int cx, int cy, int type, uint16_t c) {
  int s = 12;
  switch (type) {
    case 0: gfxFillTriangle(cx, cy - s, cx - s, cy + s, cx + s, cy + s, c); break;  // up
    case 1: gfxFillTriangle(cx, cy + s, cx - s, cy - s, cx + s, cy - s, c); break;  // down
    case 2: gfxFillTriangle(cx - s, cy, cx + s, cy - s, cx + s, cy + s, c); break;  // left
    case 3: gfxFillTriangle(cx + s, cy, cx - s, cy - s, cx - s, cy + s, c); break;  // right
    case 4: gfxText(cx - 8, cy - 10, 3, c, "A"); break;
    case 5: gfxText(cx - 8, cy - 10, 3, c, "B"); break;
  }
}

static void runWires() {
  if (!drewStatic) {
    gfxClear(NAVY);
    gfxText(90, 14, 3, WHITE, "WIRES");
    char r[16]; snprintf(r, sizeof(r), "Round %d/%d", wireRound + 1, WIRE_ROUNDS);
    gfxText(105, 190, 2, DIM, r);
    drewStatic = true;
    wireShownPos = -1;  // force the glyph row to redraw after the clear
  }
  int d = -1;
  if (isButtonPressed(BTN_UP)) d = 0;
  else if (isButtonPressed(BTN_DOWN)) d = 1;
  else if (isButtonPressed(BTN_LEFT)) d = 2;
  else if (isButtonPressed(BTN_RIGHT)) d = 3;
  else if (isButtonPressed(BTN_A)) d = 4;
  else if (isButtonPressed(BTN_B)) d = 5;
  if (d >= 0 && d == seq[seqPos]) seqPos++;
  if (seqPos >= WIRE_LEN) {
    wireRound++;
    if (wireRound >= WIRE_ROUNDS) { finish(); return; }
    for (int i = 0; i < WIRE_LEN; i++) seq[i] = esp_random() % 6;  // next sequence
    seqPos = 0;
    drewStatic = false;   // redraw title + new round number
    return;
  }
  // arrow/button row -- only touch the display when the position actually moved
  if (seqPos != wireShownPos) {
    wireShownPos = seqPos;
    gfxFillRect(10, 95, 300, 60, NAVY);
    for (int i = 0; i < WIRE_LEN; i++) {
      uint16_t c = (i < seqPos) ? GREEN : (i == seqPos ? YELLOW : DIM);
      drawGlyph(35 + i * 58, 125, seq[i], c);
    }
  }
}

// ---- window cleaning: hold the badge upright and fan-wipe like a squeegee ----
// The squeegee follows the badge's tilt ANGLE (fan it side to side) and only
// scrubs while you're wiping with FORCE. Grime clears in shades so you see where
// you've wiped. Done at 85%. Hold the screen up (not flat) so gravity registers
// the tilt; wherever you start becomes the fan center.
#define WX     20
#define WY     44
#define WW     280
#define WH     150
#define WCOLW  (WW / WIN_NCOLS)   // 20 px
#define WSQW    28
#define WIPE_DEAD 0.18f           // ignore gentle handling below this shake force (g)
#define WIPE_RATE 0.55f           // grime scrubbed per unit of shake force, per frame

static uint16_t grimeColor(int shade) {
  switch (shade) {
    case 3: return gfxColor(74, 66, 40);      // caked grime
    case 2: return gfxColor(120, 110, 70);
    case 1: return gfxColor(170, 165, 130);   // filmy
    default: return gfxColor(150, 205, 235);  // 0 = clear glass
  }
}

static int grimeShade(float g) {
  int s = (int)floorf((g / WIN_GMAX) * 3.0f + 0.5f);
  if (s < 0) s = 0;
  if (s > 3) s = 3;
  return s;
}

// repaint a glass span [x0,x1) with each column's current grime shade
static void winRepaint(int x0, int x1) {
  if (x0 < WX) x0 = WX;
  if (x1 > WX + WW) x1 = WX + WW;
  if (x1 <= x0) return;
  int cA = (x0 - WX) / WCOLW;
  int cB = (x1 - 1 - WX) / WCOLW;
  for (int c = cA; c <= cB; c++) {
    int a = WX + c * WCOLW;       if (a < x0) a = x0;
    int b = WX + (c + 1) * WCOLW; if (b > x1) b = x1;
    if (b > a) gfxFillRect(a, WY, b - a, WH, grimeColor(winShade[c]));
  }
}

// draw the squeegee across a span [x0,x1)
static void winDrawBar(int x0, int x1) {
  if (x0 < WX) x0 = WX;
  if (x1 > WX + WW) x1 = WX + WW;
  if (x1 <= x0) return;
  gfxFillRect(x0, WY, x1 - x0, WH, gfxColor(210, 225, 245));            // blade
  gfxFillRect(x0, WY, x1 - x0, 8, gfxColor(240, 210, 70));             // handle
  gfxFillRect(x0, WY + WH - 6, x1 - x0, 6, gfxColor(70, 80, 100));     // rubber
}

static void runWindow() {
  if (!drewStatic) {
    gfxClear(NAVY);
    gfxText(48, 10, 3, WHITE, "WINDOW WIPE");
    gfxText(30, 224, 2, gfxColor(140, 140, 160), "shake hard to wipe");
    gfxRectOutline(WX - 3, WY - 3, WW + 6, WH + 6, gfxColor(90, 90, 110));
    for (int c = 0; c < WIN_NCOLS; c++) {
      int s = grimeShade(winGrime[c]);
      gfxFillRect(WX + c * WCOLW, WY, WCOLW, WH, grimeColor(s));
      winShade[c] = s;
    }
    winPct = -1;
    drewStatic = true;
  }

  // clean in proportion to how HARD you shake (force above a small deadband)
  float mag = getAccelMagnitude();
  float force = mag - 1.0f - WIPE_DEAD;
  if (force > 0) {
    winCleaned += force * WIPE_RATE;
    if (winCleaned > WIN_NCOLS * WIN_GMAX) winCleaned = WIN_NCOLS * WIN_GMAX;
  }

  // fill the window left -> right; flash green as each pane comes fully clean
  for (int c = 0; c < WIN_NCOLS; c++) {
    float rem = WIN_GMAX - (winCleaned - c * WIN_GMAX);
    if (rem < 0) rem = 0;
    if (rem > WIN_GMAX) rem = WIN_GMAX;
    winGrime[c] = rem;
    int s = grimeShade(rem);
    if (s != winShade[c]) {
      if (s == 0 && winShade[c] != 0) flashLEDs(0, 190, 0, 90);
      winShade[c] = s;
    }
  }

  // squeegee rides the clean edge; low-passed for smooth motion
  float frac = winCleaned / (WIN_NCOLS * WIN_GMAX);
  float targetX = WX + frac * (WW - WSQW);
  winSqX += (targetX - winSqX) * 0.30f;

  // move the squeegee by touching only the exposed/entered edges -> no flicker
  int nx = (int)(winSqX + 0.5f);
  int ox = (int)winPrevSqX;
  if (ox < -100) {                        // first frame: draw the whole bar
    winDrawBar(nx, nx + WSQW);
    winPrevSqX = nx;
  } else if (nx != ox) {
    int dx = nx - ox;
    if (dx >= WSQW || dx <= -WSQW) {       // jumped clear: no overlap
      winRepaint(ox, ox + WSQW);
      winDrawBar(nx, nx + WSQW);
    } else if (dx > 0) {                   // moved right
      winRepaint(ox, nx);                  // expose trailing edge
      winDrawBar(ox + WSQW, nx + WSQW);    // fill leading edge
    } else {                              // moved left
      winRepaint(nx + WSQW, ox + WSQW);
      winDrawBar(nx, ox);
    }
    winPrevSqX = nx;
  }

  // progress
  int pct = (int)(frac * 100.0f);
  if (pct != winPct) {
    winPct = pct;
    gfxFillRect(31, 206, 258, 14, gfxColor(40, 40, 60));
    gfxFillRect(31, 206, 258 * pct / 100, 14, GREEN);
    gfxFillRect(250, 10, 70, 18, NAVY);
    char b[8]; snprintf(b, sizeof(b), "%d%%", pct);
    gfxText(250, 10, 2, WHITE, b);
  }
  if (pct >= 85) { flashLEDs(0, 220, 0, 400); finish(); return; }
}

// walls the trash must navigate around (randomized each attempt)
struct Wall { int x, y, w, h; };
#define NWALLS 3
#define BALL_R 8
static Wall walls[NWALLS];

static void genWalls() {
  for (int i = 0; i < NWALLS; i++) {
    for (int tries = 0; tries < 12; tries++) {
      bool horiz = esp_random() % 2;
      int w = horiz ? (80 + esp_random() % 60) : 14;
      int h = horiz ? 14 : (50 + esp_random() % 50);
      int x = 15 + esp_random() % (306 - w - 15);
      int y = 52 + esp_random() % (196 - h - 52);
      if (x < 100 && y > 150) continue;  // keep the ball's start corner clear
      walls[i] = { x, y, w, h };
      break;
    }
  }
}

static bool hitsWall(float x, float y) {
  for (int i = 0; i < NWALLS; i++) {
    const Wall &w = walls[i];
    if (x + BALL_R > w.x && x - BALL_R < w.x + w.w &&
        y + BALL_R > w.y && y - BALL_R < w.y + w.h) return true;
  }
  return false;
}

static void newChute() {
  do {
    navTX = 40 + esp_random() % 240;
    navTY = 60 + esp_random() % 140;
  } while (hitsWall(navTX, navTY) || (abs(navTX - (int)navX) < 40 && abs(navTY - (int)navY) < 40));
}

static void runGarbage() {
  if (!drewStatic) {
    gfxClear(NAVY);
    gfxText(30, 8, 3, WHITE, "GARBAGE");
    gfxText(10, 222, 2, DIM, "tilt trash to the chute");
    // walls and the chute target are fixed for the whole chute -- draw them
    // once here instead of every frame; the ball's physics (hitsWall) always
    // keeps clearance from walls, and completion triggers before the ball is
    // ever drawn overlapping the chute, so neither gets scuffed by the ball's
    // per-frame erase.
    for (int i = 0; i < NWALLS; i++) gfxFillRect(walls[i].x, walls[i].y, walls[i].w, walls[i].h, gfxColor(110, 110, 125));
    gfxFillRect(navTX - 14, navTY - 14, 28, 28, gfxColor(30, 120, 40));  // chute
    gfxRectOutline(navTX - 14, navTY - 14, 28, 28, GREEN);
    drewStatic = true;
  }
  float r, p; getRollPitch(r, p);
  navVX += p * 0.06f; navVY += r * 0.06f;   // tilt accelerates the trash
  navVX *= 0.90f; navVY *= 0.90f;           // friction

  // move per-axis so the trash slides along walls instead of sticking
  float nx = navX + navVX;
  if (nx < 18) { nx = 18; navVX = -navVX * 0.5f; }
  if (nx > 302) { nx = 302; navVX = -navVX * 0.5f; }
  if (!hitsWall(nx, navY)) navX = nx; else navVX = -navVX * 0.3f;
  float ny = navY + navVY;
  if (ny < 40) { ny = 40; navVY = -navVY * 0.5f; }
  if (ny > 210) { ny = 210; navVY = -navVY * 0.5f; }
  if (!hitsWall(navX, ny)) navY = ny; else navVY = -navVY * 0.3f;

  float dx = navX - navTX, dy = navY - navTY;
  if (dx * dx + dy * dy < 22 * 22) {        // trash reached the chute
    navHits++;
    flashLEDs(0, 200, 0, 250);
    if (navHits >= 3) { finish(); return; }
    newChute();
    drewStatic = false;
    return;
  }

  gfxFillCircle((int)navPX, (int)navPY, 9, NAVY);          // erase old trash
  gfxFillCircle((int)navX, (int)navY, BALL_R, gfxColor(150, 120, 80));  // trash
  navPX = navX; navPY = navY;
  if (navHits != navHitsShown) {
    navHitsShown = navHits;
    char h[12]; snprintf(h, sizeof(h), "%d/3", navHits);
    gfxFillRect(282, 34, 36, 20, NAVY);
    gfxText(284, 36, 2, GREEN, h);
  }
}

#define DDR_LANES 4
#define DDR_HITS  8
#define DDR_HITY  188
#define DDR_TOL   30
#define DDR_SPEED 1.7f
#define DDR_SPAWN 950   // ms between new arrows

static int ddrLaneX(int l) { return 52 + l * 72; }   // 52, 124, 196, 268

static void runDDR() {
  if (!drewStatic) {
    gfxClear(NAVY);
    gfxText(64, 10, 3, WHITE, "RHYTHM");
    gfxText(28, 222, 2, DIM, "hit arrows at the line");
    drewStatic = true;
  }

  // spawn arrows on a cadence
  if (millis() - ddrLastSpawn > DDR_SPAWN) {
    for (int i = 0; i < DDR_MAX; i++) if (!ddr[i].on) {
      ddr[i].on = true; ddr[i].lane = esp_random() % DDR_LANES;
      ddr[i].y = 22; ddr[i].py = 22;
      break;
    }
    ddrLastSpawn = millis();
  }

  // input: each d-pad press hits the nearest arrow in that lane inside the zone
  for (int d = 0; d < 4; d++) {
    uint16_t mask = d == 0 ? BTN_UP : d == 1 ? BTN_DOWN : d == 2 ? BTN_LEFT : BTN_RIGHT;
    if (!isButtonPressed(mask)) continue;
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < DDR_MAX; i++)
      if (ddr[i].on && ddr[i].lane == d) {
        float dist = fabsf(ddr[i].y - DDR_HITY);
        if (dist <= DDR_TOL && dist < bd) { bd = dist; best = i; }
      }
    if (best >= 0) {
      gfxFillRect(ddrLaneX(d) - 14, (int)ddr[best].y - 14, 28, 28, NAVY);
      ddr[best].on = false; ddrHits++; flashLEDs(0, 200, 0, 80);
    } else {
      flashLEDs(200, 0, 0, 70);   // whiffed
    }
  }

  // erase arrows at their previous spot
  for (int i = 0; i < DDR_MAX; i++)
    if (ddr[i].on) gfxFillRect(ddrLaneX(ddr[i].lane) - 14, (int)ddr[i].py - 14, 28, 28, NAVY);

  // redraw the dim lane targets + hit line each frame (trails erased above)
  for (int l = 0; l < DDR_LANES; l++) drawGlyph(ddrLaneX(l), DDR_HITY, l, gfxColor(70, 70, 95));
  gfxFillRect(18, DDR_HITY + 16, 284, 2, gfxColor(90, 90, 115));

  // move + draw arrows on top
  for (int i = 0; i < DDR_MAX; i++) if (ddr[i].on) {
    ddr[i].y += DDR_SPEED;
    if (ddr[i].y > DDR_HITY + DDR_TOL + 10) { ddr[i].on = false; continue; }  // missed
    drawGlyph(ddrLaneX(ddr[i].lane), (int)ddr[i].y, ddr[i].lane, WHITE);
    ddr[i].py = ddr[i].y;
  }

  // progress
  if (ddrHits != ddrShown) {
    ddrShown = ddrHits;
    gfxFillRect(250, 10, 70, 20, NAVY);
    char b[12]; snprintf(b, sizeof(b), "%d/%d", ddrHits, DDR_HITS);
    gfxText(252, 12, 2, GREEN, b);
  }
  if (ddrHits >= DDR_HITS) { flashLEDs(0, 220, 0, 400); finish(); return; }
}

void taskUpdate() {
  if (curTask < 0) return;
  // no button cancel: A/B/d-pad are all game inputs. Ends on completion,
  // a 20s timeout, or a meeting (game.cpp cancels on phase change).
  if (millis() - taskStart > TASK_TIMEOUT_MS) { taskCancel(); return; }
  switch (curTask) {
    case 0: runWires(); break;
    case 1: runGarbage(); break;
    case 2: runWindow(); break;
    case 3: runDDR(); break;
  }
}
