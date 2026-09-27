#include <Arduino.h>
#include <esp_random.h>
#include "task_api.h"
#include "buttons.h"
#include "display.h"
#include "leds.h"
#include "imu.h"

// ---- window cleaning: wipe the badge back and forth like a squeegee ----
// After a short "get ready" countdown, every back-and-forth stroke scrubs some
// grime off; harder strokes scrub more. Holding the badge still -- in ANY
// orientation -- does nothing. Grime clears left to right in shades. Done at 85%.
//
// Stroke detection: a slow baseline tracks the raw accel vector (gravity, sensor
// offset, however the badge is held) and is subtracted off, leaving only motion.
// A stroke counts when that motion is strong enough AND points opposite to the
// previous stroke, so the badge has to actually reverse direction.
#define WIN_NCOLS 14
#define WIN_GMAX  3.0f
#define WX     20
#define WY     44
#define WW     280
#define WH     150
#define WCOLW  (WW / WIN_NCOLS)   // 20 px
#define WSQW    28
#define WIPE_BUFFER_MS 5000       // "get ready" countdown before wiping counts
#define WIPE_BASE_TAU  300.0f     // ms; baseline time constant (longer = slower to absorb a pose)
#define STROKE_G       0.5f       // motion (g) needed for a stroke
#define STROKE_REARM_G 0.25f      // motion must settle below this between strokes (hysteresis)
#define STROKE_MIN_MS  180        // min time between strokes -- one per half-swing, not per jitter
#define STROKE_MAX_X   1.5f       // a hard stroke scrubs up to this many times a gentle one
#define STROKE_CLEAN   1.2f       // grime scrubbed by a gentle stroke (window total = NCOLS*GMAX)

static float winGrime[WIN_NCOLS];   // remaining grime per column (0..WIN_GMAX)
static int   winShade[WIN_NCOLS];   // last-drawn shade per column
static float winSqX, winPrevSqX;    // squeegee left edge, px (prev = last drawn)
static float winCleaned;            // total grime scrubbed off (0 .. NCOLS*GMAX)
static int   winPct;               // last-drawn percent
static bool drawn = false;

static unsigned long winStartMs, winLastMs;
static int   winCountShown;         // last countdown second drawn (0 = wiping)
static bool  baseSeeded;
static float baseX, baseY, baseZ;   // slow-tracking accel baseline (g)
static bool  haveDir;               // has a stroke happened yet?
static float dirX, dirY, dirZ;      // unit direction of the last stroke
static bool  strokeArmed;           // motion has settled since the last stroke
static unsigned long lastStrokeMs;

static void start() {
  for (int i = 0; i < WIN_NCOLS; i++) { winGrime[i] = WIN_GMAX; winShade[i] = -1; }
  winSqX = 20; winPrevSqX = -1000; winCleaned = 0; winPct = -1;
  winStartMs = winLastMs = millis();
  winCountShown = -1;
  baseSeeded = false; haveDir = false;
  strokeArmed = true; lastStrokeMs = 0;
  drawn = false;
}

// Feed one accel sample; returns the scrub amount of a stroke this frame (0 if none).
static float detectStroke() {
  float ax = baseX, ay = baseY, az = baseZ;
  getAccel(ax, ay, az);   // leaves the values untouched on a bad read
  unsigned long now = millis();
  if (!baseSeeded) {
    baseX = ax; baseY = ay; baseZ = az;
    baseSeeded = true; winLastMs = now;
    return 0;
  }
  float dt = (float)(now - winLastMs); winLastMs = now;
  float k = dt / (WIPE_BASE_TAU + dt);
  baseX += (ax - baseX) * k; baseY += (ay - baseY) * k; baseZ += (az - baseZ) * k;

  float hx = ax - baseX, hy = ay - baseY, hz = az - baseZ;
  float mag = sqrtf(hx * hx + hy * hy + hz * hz);
  if (mag < STROKE_REARM_G) strokeArmed = true;
  if (!strokeArmed || mag < STROKE_G) return 0;
  if (now - lastStrokeMs < STROKE_MIN_MS) return 0;
  if (haveDir && hx * dirX + hy * dirY + hz * dirZ >= 0) return 0;  // same way as last stroke

  haveDir = true;
  strokeArmed = false;
  lastStrokeMs = now;
  dirX = hx / mag; dirY = hy / mag; dirZ = hz / mag;
  float x = mag / STROKE_G;
  if (x > STROKE_MAX_X) x = STROKE_MAX_X;
  return STROKE_CLEAN * x;
}

// the "get ready" countdown, drawn over the middle of the glass
static void drawCountdown(int secs) {
  gfxFillRect(90, 80, 140, 80, NAVY);
  gfxRectOutline(90, 80, 140, 80, WHITE);
  gfxText(107, 92, 2, WHITE, "GET READY");
  char b[4]; snprintf(b, sizeof(b), "%d", secs);
  gfxText(151, 118, 4, YELLOW, b);
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
    gfxText(30, 224, 2, gfxColor(140, 140, 160), "wipe back and forth");
    gfxRectOutline(WX - 3, WY - 3, WW + 6, WH + 6, gfxColor(90, 90, 110));
    for (int c = 0; c < WIN_NCOLS; c++) {
      int s = grimeShade(winGrime[c]);
      gfxFillRect(WX + c * WCOLW, WY, WCOLW, WH, grimeColor(s));
      winShade[c] = s;
    }
    winPct = -1;
    drawn = true;
  }

  // keep the baseline settling during the countdown, but don't scrub yet
  float scrub = detectStroke();
  unsigned long elapsed = millis() - winStartMs;
  if (elapsed < WIPE_BUFFER_MS) {
    int secs = (int)((WIPE_BUFFER_MS - elapsed + 999) / 1000);
    if (secs != winCountShown) { winCountShown = secs; drawCountdown(secs); }
    return;
  }
  if (winCountShown != 0) {           // countdown over: uncover the glass
    winCountShown = 0;
    winRepaint(WX, WX + WW);
    haveDir = false;                  // first real stroke may go either way
    scrub = 0;
    flashLEDs(0, 120, 200, 150);
  }

  if (scrub > 0) {
    winCleaned += scrub;
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

extern const TaskDef TASK_WINDOW_WIPE = { "WINDOW WIPE", "047CBF97DD2A81", start, run, WIPE_BUFFER_MS + 20000 };
