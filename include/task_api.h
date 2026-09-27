#pragma once
#include <stdint.h>

// What a single minigame provides. Each task lives in its own file under
// src/tasks/ and defines one of these; src/tasks/task_list.cpp lists them.
struct TaskDef {
  const char *name;   // shown in the test menu and "ALREADY COMPLETED" notice
  const char *uid;    // NFC tag UID: uppercase hex, no separators (as scanNFC() prints)
  void (*start)();    // reset all state; the next run() should redraw from scratch
  void (*run)();      // one frame of input + rendering; call taskFinish() when won
  unsigned long timeoutMs; // maximum time allowed for the whole task
};

// ---- helpers shared by every minigame ----

// The player won: marks the task done and closes the minigame.
void taskFinish();

// Shared palette, set up in setupTasks().
extern uint16_t NAVY, WHITE, GREEN, RED, YELLOW, DIM;

// Draw one input glyph centered at (cx,cy): 0 UP, 1 DOWN, 2 LEFT, 3 RIGHT
// (arrows), 4 A, 5 B (letters).
void drawGlyph(int cx, int cy, int type, uint16_t c);

// The task list itself (defined in src/tasks/task_list.cpp).
extern const TaskDef *const TASK_LIST[];
extern const int TASK_LIST_COUNT;
