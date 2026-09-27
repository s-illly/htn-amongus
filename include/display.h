#pragma once

void setupDisplay();

// Emergency meeting screen: draw the static banner once, then either the
// "waiting for A" prompt (gather) or the countdown (discuss). clearScreen()
// blanks it so the tilt view can resume.
void showMeetingScreen();
void showMeetingWaiting();
void showMeetingCountdown(int secondsLeft);
void clearScreen();

// Role reveal card: a crewmate in the player's color, CREWMATE (blue) or
// IMPOSTER (red) banner. If imposter, small crewmate icons for each teammate
// color are drawn on the left so you know your fellow imposters, plus a kill
// cooldown readout (killCooldownSecs seconds remaining, or "Ready to kill"
// at 0). killCooldownSecs is ignored when not an imposter.
void showRoleCard(int colorR, int colorG, int colorB, bool isImposter,
                  int nTeam, const uint8_t *teamR, const uint8_t *teamG,
                  const uint8_t *teamB, int killCooldownSecs);

// Lobby: AMONG US logo, crewmate in your color, player count, and the
// host-adjustable settings list with a cursor on row `sel`.
void showLobby(int players, int imp, int disc, int vote, int meet, int sel,
               int colorR, int colorG, int colorB);

// In-game status: your crewmate + color, alive count (or GHOST if dead).
// Role is NOT shown here -- hold START to see it. bodyNearby shows a
// "hold B to report" hint whenever a dead player is within report range.
void showHUD(bool alive, int aliveCount, int colorR, int colorG, int colorB,
             bool bodyNearby);

// Temporary notice shown after scanning a task tag whose task is already done.
void showTaskAlreadyCompleted(int taskIndex);

// Voting screen: the current pick (a color, or SKIP), countdown, and hints.
void showVote(const char *name, int colorR, int colorG, int colorB,
              bool isSkip, int secondsLeft, bool alreadyVoted);

// Result of a vote: who was ejected (+ imposter reveal, or skipped), plus a
// tally row of mini color icons with how many votes each color got.
void showResult(const char *ejName, int ejR, int ejG, int ejB,
                bool skipped, bool wasImposter,
                int nTally, const uint8_t *talR, const uint8_t *talG,
                const uint8_t *talB, const int *talCounts, int skipCount);

// Winner screen.
void showGameOver(bool crewWon);

// ---- generic drawing primitives (for task minigames) ----
uint16_t gfxColor(uint8_t r, uint8_t g, uint8_t b);
void gfxClear(uint16_t color);
void gfxText(int x, int y, int size, uint16_t color, const char *s);
void gfxRectOutline(int x, int y, int w, int h, uint16_t color);
void gfxFillRect(int x, int y, int w, int h, uint16_t color);
void gfxFillCircle(int x, int y, int r, uint16_t color);
void gfxFillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);

// Thin crew task-progress bar across the top (pct 0-100).
void drawTaskBar(int pct);
