#include <Arduino.h>
#include "tasktest.h"
#include "tasks.h"
#include "buttons.h"
#include "display.h"
#include "leds.h"

enum Mode { MENU, TASK };
static Mode mode = MENU;
static int  sel = 0;
static bool drawn = false;

static void drawMenu() {
  uint16_t bg = gfxColor(12, 14, 30), white = gfxColor(255, 255, 255);
  uint16_t dim = gfxColor(140, 140, 160), hi = gfxColor(70, 210, 90);
  gfxClear(bg);
  gfxText(60, 10, 3, white, "TASK TEST");
  for (int i = 0; i < taskCount(); i++) {
    int y = 54 + i * 30;
    bool s = (i == sel);
    if (s) gfxFillRect(28, y - 3, 264, 26, gfxColor(40, 60, 40));
    gfxText(44, y, 3, s ? hi : dim, taskName(i));
  }
  gfxText(30, 224, 2, dim, "A=play  START=exit");
  drawn = true;
}

void taskTestLoop() {
  switch (mode) {
    case TASK:
      if (isButtonPressed(BTN_START)) taskCancel();
      taskUpdate();
      if (!taskActive()) {
        if (taskJustCompleted() >= 0) flashLEDs(0, 220, 0, 300);
        mode = MENU; drawn = false;
      }
      return;

    case MENU:
    default:
      if (!drawn) drawMenu();
      if (isButtonPressed(BTN_UP))   { sel = (sel + taskCount() - 1) % taskCount(); drawn = false; }
      if (isButtonPressed(BTN_DOWN)) { sel = (sel + 1) % taskCount(); drawn = false; }
      if (isButtonPressed(BTN_A))    { taskStartIndex(sel); mode = TASK; }
      return;
  }
}
