#include <Arduino.h>
#include <esp_random.h>
#include "task_api.h"
#include "buttons.h"
#include "display.h"
#include "leds.h"

// Simon Says: remember and repeat progressively longer button sequences.
#define SIMON_LEVELS       3
#define SIMON_MAX_COMMANDS 6
#define SIMON_SHOW_MS      3000
#define SIMON_FAIL_MS      1000

static const int simonLengths[SIMON_LEVELS] = { 2, 4, 6 };
static int simonLevel;
static int simonSequences[SIMON_LEVELS][SIMON_MAX_COMMANDS];
static int simonTyped[SIMON_MAX_COMMANDS];
static int simonLength;
static int simonTypedCount;
static unsigned long simonPhaseAt;
static bool simonDrawn;

enum SimonPhase { SIMON_READY, SIMON_SHOW, SIMON_GO, SIMON_FAIL };
static SimonPhase simonPhase;

static void generateSequences() {
  for (int level = 0; level < SIMON_LEVELS; level++)
    for (int i = 0; i < simonLengths[level]; i++)
      simonSequences[level][i] = esp_random() % 6;
}

static void startLevel(int level) {
  simonLevel = level;
  simonLength = simonLengths[level];
  simonTypedCount = 0;
  simonPhase = SIMON_READY;
  simonPhaseAt = millis();
  simonDrawn = false;
}

static int readCommand() {
  if (isButtonPressed(BTN_UP)) return 0;
  if (isButtonPressed(BTN_DOWN)) return 1;
  if (isButtonPressed(BTN_LEFT)) return 2;
  if (isButtonPressed(BTN_RIGHT)) return 3;
  if (isButtonPressed(BTN_A)) return 4;
  if (isButtonPressed(BTN_B)) return 5;
  return -1;
}

static void drawCommandRow(const int *commands, int count, uint16_t color) {
  int firstX = 160 - ((count - 1) * 50) / 2;
  for (int i = 0; i < count; i++)
    drawGlyph(firstX + i * 50, 125, commands[i], color);
}

static void drawReady() {
  gfxClear(NAVY);
  char title[20];
  snprintf(title, sizeof(title), "LEVEL %d", simonLevel + 1);
  gfxText(105, 25, 3, WHITE, title);

  char prompt[32];
  snprintf(prompt, sizeof(prompt), "remember %d commands", simonLength);
  gfxText(40, 78, 2, DIM, prompt);
  gfxText(48, 170, 2, YELLOW, "press B when ready");
}

static void drawShow() {
  gfxClear(NAVY);
  gfxText(108, 25, 3, WHITE, "REMEMBER");
  drawCommandRow(simonSequences[simonLevel], simonLength, YELLOW);
  gfxText(96, 190, 2, DIM, "3 seconds");
}

static void drawGo() {
  gfxClear(NAVY);
  gfxText(128, 25, 3, GREEN, "GO!");
  if (simonTypedCount > 0) drawCommandRow(simonTyped, simonTypedCount, GREEN);
  gfxText(80, 190, 2, DIM, "repeat the sequence");
}

static void drawFail() {
  gfxClear(NAVY);
  gfxText(112, 25, 3, RED, "WRONG!");
  drawCommandRow(simonTyped, simonTypedCount, RED);
  gfxText(78, 190, 2, DIM, "back to level 1");
}

static void start() {
  generateSequences();
  startLevel(0);
}

static void run() {
  switch (simonPhase) {
    case SIMON_READY:
      if (!simonDrawn) { drawReady(); simonDrawn = true; }
      if (isButtonPressed(BTN_B)) {
        simonPhase = SIMON_SHOW;
        simonPhaseAt = millis();
        simonDrawn = false;
      }
      break;

    case SIMON_SHOW:
      if (!simonDrawn) { drawShow(); simonDrawn = true; }
      if (millis() - simonPhaseAt >= SIMON_SHOW_MS) {
        simonPhase = SIMON_GO;
        simonPhaseAt = millis();
        simonTypedCount = 0;
        simonDrawn = false;
      }
      break;

    case SIMON_GO: {
      if (!simonDrawn) { drawGo(); simonDrawn = true; }
      int command = readCommand();
      if (command < 0) break;

      simonTyped[simonTypedCount++] = command;
      simonDrawn = false;
      if (command != simonSequences[simonLevel][simonTypedCount - 1]) {
        simonPhase = SIMON_FAIL;
        simonPhaseAt = millis();
        flashLEDs(220, 0, 0, 180);
        break;
      }

      if (simonTypedCount >= simonLength) {
        if (simonLevel + 1 >= SIMON_LEVELS) {
          flashLEDs(0, 220, 0, 400);
          taskFinish();
        } else {
          startLevel(simonLevel + 1);
        }
      }
      break;
    }

    case SIMON_FAIL:
      if (!simonDrawn) { drawFail(); simonDrawn = true; }
      if (millis() - simonPhaseAt >= SIMON_FAIL_MS) {
        generateSequences();
        startLevel(0);
      }
      break;
  }
}

extern const TaskDef TASK_SIMON = { "SIMON SAYS", "049F4498DD2A81", start, run, 60000 };
