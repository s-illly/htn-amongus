#include <Arduino.h>
#include <esp_random.h>
#include "task_api.h"
#include "buttons.h"
#include "display.h"
#include "leds.h"
#include "imu.h"

// ---- window cleaning: hold the badge upright and fan-wipe like a squeegee ----
// The squeegee follows the badge's tilt ANGLE (fan it side to side) and only
// scrubs while you're wiping with FORCE. Grime clears in shades so you see where
// you've wiped. Done at 85%. Hold the screen up (not flat) so gravity registers
// the tilt; wherever you start becomes the fan center.
#define WIN_NCOLS 14
#define WIN_GMAX  3.0f
#define WX     20
#define WY     44
#define WW     280
#define WH     150
#define WCOLW  (WW / WIN_NCOLS)   // 20 px
#define WSQW    28
#define WIPE_DEAD 0.18f           // ignore gentle handling below this shake force (g)
#define WIPE_RATE 0.55f           // grime scrubbed per unit of shake force, per frame

static float winGrime[WIN_NCOLS];   // remaining grime per column (0..WIN_GMAX)
static int   winShade[WIN_NCOLS];   // last-drawn shade per column
static float winSqX, winPrevSqX;    // squeegee left edge, px (prev = last drawn)
static float winCleaned;            // total grime scrubbed off (0 .. NCOLS*GMAX)
static int   winPct;               // last-drawn percent
static bool drawn = false;

static void start() {
  for (int i = 0; i < WIN_NCOLS; i++) { winGrime[i] = WIN_GMAX; winShade[i] = -1; }
  winSqX = 20; winPrevSqX = -1000; winCleaned = 0; winPct = -1;
  drawn = false;
}

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

static void run() {
  if (!drawn) {
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
    drawn = true;
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
  if (pct >= 85) { flashLEDs(0, 220, 0, 400); taskFinish(); return; }
}

extern const TaskDef TASK_WINDOW_WIPE = { "WINDOW WIPE", "047CBF97DD2A81", start, run };
