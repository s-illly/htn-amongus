#pragma once

// Crew tasks: four NFC-triggered minigames. Each task has its own NFC UID;
// replace the placeholders below with the UIDs printed by scanNFC().

#define NUM_TASKS 4

// UIDs are uppercase hex with no separators, matching scanNFC() output.
#define TASK_UID_WIRES       "04EAD297DD2A81"
#define TASK_UID_GARBAGE     "048F5798DD2A81"
#define TASK_UID_WINDOW_WIPE "047CBF97DD2A81"
#define TASK_UID_RHYTHM      "04D85798DD2A81"

void setupTasks();
void resetTasks();                    // new game: clear my completed tasks

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

// Test harness only: force-launch minigame `t` (0..NUM_TASKS-1), ignoring the
// done flag so it can be replayed.
void taskStartIndex(int t);
