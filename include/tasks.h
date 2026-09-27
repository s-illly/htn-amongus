#pragma once

// Crew tasks: NFC-triggered minigames. Each minigame is its own file under
// src/tasks/ (with its tag UID), and src/tasks/task_list.cpp lists them all.

#define MAX_TASKS 32   // upper bound on the task list, for per-task arrays

void setupTasks();
void resetTasks();                    // new game: clear my completed tasks

int taskCount();                      // number of minigames in the task list
const char *taskName(int t);          // display name, or "TASK" if out of range

// Returns the fixed task index for a configured UID, or -1 for an unknown tag.
int taskIndexForUid(const char *uid);
bool taskIsCompleted(int t);

// A tag was scanned: starts that tag's configured minigame.
// Returns true if one was started (false if unknown, already active, or done).
bool taskTryStart(const char *uid);
bool taskActive();
void taskUpdate();                    // run + render the current minigame
void taskCancel();                    // abort (meeting called, B pressed, etc.)

int taskJustCompleted();              // task index finished this frame, else -1

// Test harness only: force-launch minigame `t` (0..taskCount()-1), ignoring the
// done flag so it can be replayed.
void taskStartIndex(int t);
