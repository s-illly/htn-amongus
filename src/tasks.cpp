#include <Arduino.h>
#include <string.h>
#include "tasks.h"
#include "task_api.h"
#include "display.h"

// Task framework: tracks which minigame is running and which are done, and
// dispatches to the TaskDefs in src/tasks/task_list.cpp. The minigames
// themselves live in src/tasks/.

#define TASK_TIMEOUT_MS 20000

static bool doneTask[MAX_TASKS];
static int curTask = -1;          // which minigame is running (-1 = none)
static unsigned long taskStart = 0;
static int justCompleted = -1;

uint16_t NAVY, WHITE, GREEN, RED, YELLOW, DIM;

int taskCount() {
  return TASK_LIST_COUNT < MAX_TASKS ? TASK_LIST_COUNT : MAX_TASKS;
}

const char *taskName(int t) {
  return (t >= 0 && t < taskCount()) ? TASK_LIST[t]->name : "TASK";
}

void setupTasks() {
  NAVY = gfxColor(10, 12, 34); WHITE = gfxColor(255, 255, 255);
  GREEN = gfxColor(70, 210, 90); RED = gfxColor(220, 60, 60);
  YELLOW = gfxColor(240, 220, 60); DIM = gfxColor(150, 150, 170);
  if (TASK_LIST_COUNT > MAX_TASKS) Serial.println("Task list exceeds MAX_TASKS; extra tasks ignored");
  resetTasks();
}

void resetTasks() {
  for (int i = 0; i < MAX_TASKS; i++) doneTask[i] = false;
  curTask = -1;
}

static void enterTask(int t) {
  curTask = t;
  taskStart = millis();
  justCompleted = -1;
  TASK_LIST[t]->start();
}

int taskIndexForUid(const char *uid) {
  if (!uid) return -1;
  for (int t = 0; t < taskCount(); t++)
    if (strcmp(uid, TASK_LIST[t]->uid) == 0) return t;
  return -1;
}

bool taskIsCompleted(int t) {
  return t >= 0 && t < taskCount() && doneTask[t];
}

bool taskTryStart(const char *uid) {
  if (curTask >= 0) return false;
  int t = taskIndexForUid(uid);
  if (t < 0 || doneTask[t]) return false;
  enterTask(t);
  return true;
}

void taskStartIndex(int t) {
  if (t < 0 || t >= taskCount()) return;
  doneTask[t] = false;
  enterTask(t);
}

bool taskActive() { return curTask >= 0; }

void taskCancel() { curTask = -1; }

int taskJustCompleted() { int j = justCompleted; justCompleted = -1; return j; }

void taskFinish() {
  if (curTask < 0) return;
  doneTask[curTask] = true;
  justCompleted = curTask;
  curTask = -1;
}

// draw one sequence glyph centered at (cx,cy): arrows for 0-3, letters for A/B
void drawGlyph(int cx, int cy, int type, uint16_t c) {
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

void taskUpdate() {
  if (curTask < 0) return;
  // no button cancel: A/B/d-pad are all game inputs. Ends on completion,
  // a 20s timeout, or a meeting (game.cpp cancels on phase change).
  if (millis() - taskStart > TASK_TIMEOUT_MS) { taskCancel(); return; }
  TASK_LIST[curTask]->run();
}
