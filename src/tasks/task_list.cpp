#include "task_api.h"

// Every minigame in the game. To add one: create src/tasks/<name>.cpp defining
// a `const TaskDef`, declare it here, and add it to TASK_LIST. The list order
// is the task index (used by the test menu).
extern const TaskDef TASK_WIRES;
extern const TaskDef TASK_GARBAGE;
extern const TaskDef TASK_WINDOW_WIPE;
extern const TaskDef TASK_RHYTHM;
extern const TaskDef TASK_SIMON;

const TaskDef *const TASK_LIST[] = {
  &TASK_WIRES,
  &TASK_GARBAGE,
  &TASK_WINDOW_WIPE,
  &TASK_RHYTHM,
  &TASK_SIMON,
};

const int TASK_LIST_COUNT = sizeof(TASK_LIST) / sizeof(TASK_LIST[0]);
