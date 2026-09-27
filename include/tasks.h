#pragma once

// Crew tasks: four NFC-triggered minigames. Any tag scan opens the next
// uncompleted task, in fixed order (0 Wires, 1 Garbage, 2 Window Wipe,
// 3 Rhythm) -- which physical tag was scanned doesn't matter, so this doesn't
// depend on reliably reading/pairing individual tag UIDs. Wires and Garbage
// are first so a time-boxed demo (e.g. one showcased per judge) always
// reaches those two; Window Wipe and Rhythm exist to show there's more depth,
// without needing to actually be reached live. Role permissions and progress
// accounting are handled by game.cpp.

#define NUM_TASKS 4

void setupTasks();
void resetTasks();                    // new game: clear my completed tasks

// A tag was scanned: starts the next uncompleted minigame, in fixed order.
// Returns true if one was started (false if one's already active, or every
// task is already done).
bool taskTryStart(const char *uid);
bool taskActive();
void taskUpdate();                    // run + render the current minigame
void taskCancel();                    // abort (meeting called, B pressed, etc.)

int taskJustCompleted();              // task index finished this frame, else -1

// Test harness only: force-launch minigame `t` (0..NUM_TASKS-1), ignoring the
// done flag so it can be replayed.
void taskStartIndex(int t);
