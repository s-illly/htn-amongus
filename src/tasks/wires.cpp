#include <Arduino.h>
#include <esp_random.h>
#include "task_api.h"
#include "buttons.h"
#include "display.h"
#include "leds.h"

// Wires: repeat the shown sequence of arrows / A / B, several rounds.
// input types: 0 UP, 1 DOWN, 2 LEFT, 3 RIGHT, 4 A, 5 B
#define WIRE_LEN    5   // glyphs per sequence
#define WIRE_ROUNDS 3   // sequences to complete for the whole task

static int seq[WIRE_LEN], seqPos, wireRound;
static int wireShownPos = -1;              // last seqPos actually drawn
static bool drawn = false;

static void start() {
  for (int i = 0; i < WIRE_LEN; i++) seq[i] = esp_random() % 6;
  seqPos = 0; wireRound = 0;
  drawn = false;
}

static void run() {
  if (!drawn) {
    gfxClear(NAVY);
    gfxText(90, 14, 3, WHITE, "WIRES");
    char r[16]; snprintf(r, sizeof(r), "Round %d/%d", wireRound + 1, WIRE_ROUNDS);
    gfxText(105, 190, 2, DIM, r);
    drawn = true;
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
    if (wireRound >= WIRE_ROUNDS) { taskFinish(); return; }
    for (int i = 0; i < WIRE_LEN; i++) seq[i] = esp_random() % 6;  // next sequence
    seqPos = 0;
    drawn = false;   // redraw title + new round number
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

extern const TaskDef TASK_WIRES = { "WIRES", "04EAD297DD2A81", start, run, 20000 };
