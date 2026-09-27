#include <Arduino.h>
#include <esp_random.h>
#include "task_api.h"
#include "buttons.h"
#include "display.h"
#include "leds.h"

// 3: rhythm (DDR) -- arrows fall in 4 lanes, hit the matching d-pad at the line
#define DDR_MAX 8
struct Arrow { bool on; int lane; float y, py; };
#define DDR_LANES 4
#define DDR_HITS  8
#define DDR_HITY  188
#define DDR_TOL   30
#define DDR_SPEED 1.7f
#define DDR_SPAWN 950   // ms between new arrows

static Arrow ddr[DDR_MAX];
static int ddrHits, ddrShown;
static unsigned long ddrLastSpawn;
static bool drawn = false;

static void start() {
  for (int i = 0; i < DDR_MAX; i++) ddr[i].on = false;
  ddrHits = 0; ddrShown = -1; ddrLastSpawn = 0;
  drawn = false;
}

static int ddrLaneX(int l) { return 52 + l * 72; }   // 52, 124, 196, 268

// Arrows only touch the screen inside this band, so the title/counter header
// and the hint footer are never erased. Arrows still spawn and move above/below
// it on the same timing -- they're just invisible there.
#define DDR_FIELD_TOP 40    // below the title (size-3 text at y=10)
#define DDR_FIELD_BOT 220   // above the footer hint (y=222)
static bool ddrVisible(float y) {
  return (int)y - 14 >= DDR_FIELD_TOP && (int)y + 14 <= DDR_FIELD_BOT;
}

static void run() {
  if (!drawn) {
    gfxClear(NAVY);
    gfxText(64, 10, 3, WHITE, "RHYTHM");
    gfxText(28, 222, 2, DIM, "hit arrows at the line");
    drawn = true;
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
      if (ddrVisible(ddr[best].py))
        gfxFillRect(ddrLaneX(d) - 14, (int)ddr[best].py - 14, 28, 28, NAVY);
      ddr[best].on = false; ddrHits++; flashLEDs(0, 200, 0, 80);
    } else {
      flashLEDs(200, 0, 0, 70);   // whiffed
    }
  }

  // erase arrows at their previous spot
  for (int i = 0; i < DDR_MAX; i++)
    if (ddr[i].on && ddrVisible(ddr[i].py)) gfxFillRect(ddrLaneX(ddr[i].lane) - 14, (int)ddr[i].py - 14, 28, 28, NAVY);

  // redraw the dim lane targets + hit line each frame (trails erased above)
  for (int l = 0; l < DDR_LANES; l++) drawGlyph(ddrLaneX(l), DDR_HITY, l, gfxColor(70, 70, 95));
  gfxFillRect(18, DDR_HITY + 16, 284, 2, gfxColor(90, 90, 115));

  // move + draw arrows on top
  for (int i = 0; i < DDR_MAX; i++) if (ddr[i].on) {
    ddr[i].y += DDR_SPEED;
    if (ddr[i].y > DDR_HITY + DDR_TOL + 10) { ddr[i].on = false; continue; }  // missed
    if (ddrVisible(ddr[i].y))
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
  if (ddrHits >= DDR_HITS) { flashLEDs(0, 220, 0, 400); taskFinish(); return; }
}

extern const TaskDef TASK_RHYTHM = { "RHYTHM", "04D85798DD2A81", start, run };
