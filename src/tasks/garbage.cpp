#include <Arduino.h>
#include <esp_random.h>
#include "task_api.h"
#include "buttons.h"
#include "display.h"
#include "leds.h"
#include "imu.h"

// Garbage: tilt the badge to roll the trash around the walls into the chute.
#define GARBAGE_HITS 3

static float navX, navY, navVX, navVY, navPX, navPY;
static int navTX, navTY, navHits, navHitsShown;
static bool drawn = false;

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

static void start() {
  navX = navPX = 55; navY = navPY = 185; navVX = navVY = 0; navHits = 0; navHitsShown = -1;
  genWalls();
  newChute();
  drawn = false;
}

static void run() {
  if (!drawn) {
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
    drawn = true;
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
    if (navHits >= GARBAGE_HITS) { taskFinish(); return; }
    newChute();
    drawn = false;
    return;
  }

  gfxFillCircle((int)navPX, (int)navPY, 9, NAVY);          // erase old trash
  gfxFillCircle((int)navX, (int)navY, BALL_R, gfxColor(150, 120, 80));  // trash
  navPX = navX; navPY = navY;
  if (navHits != navHitsShown) {
    navHitsShown = navHits;
    char h[12]; snprintf(h, sizeof(h), "%d/%d", navHits, GARBAGE_HITS);
    gfxFillRect(282, 34, 36, 20, NAVY);
    gfxText(284, 36, 2, GREEN, h);
  }
}

extern const TaskDef TASK_GARBAGE = { "GARBAGE", "048F5798DD2A81", start, run, 20000 };
