#include <Arduino.h>
#include <esp_random.h>
#include "task_api.h"
#include "buttons.h"
#include "display.h"
#include "leds.h"
#include "players.h"

// Dino: automatically run, jump with A, duck with held B, and survive ten
// obstacles. Obstacles are either ground cacti or flying birds.
#define DINO_GROUND_Y 190
#define DINO_X        38
#define DINO_W        30
#define DINO_STAND_H  30
#define DINO_DUCK_H   16
#define DINO_JUMP_V   -8.5f
#define DINO_GRAVITY  0.42f
#define DINO_SPEED    3.3f
#define DINO_GOAL     10
#define DINO_FAIL_MS  5000

struct Obstacle {
  bool active;
  float x;
  int kind; // 0/1 cactus, 2 bird
  int w, h, y;
};

static float dinoBottom, dinoVelocity;
static bool ducking;
static Obstacle obstacle;
static unsigned long nextSpawnAt, failedAt;
static int passed, shownPassed;
static bool drawn, rendered, failed;

static uint16_t DINO_COLOR, DINO_VISOR, OBSTACLE_COLOR;

static int dinoHeight(bool isDuck) { return isDuck ? DINO_DUCK_H : DINO_STAND_H; }

static void resetRun() {
  dinoBottom = DINO_GROUND_Y;
  dinoVelocity = 0;
  ducking = false;
  obstacle = { false, 0, 0, 0, 0, 0 };
  nextSpawnAt = millis() + 700;
  passed = 0;
  shownPassed = -1;
  drawn = false;
  rendered = false;
  failed = false;
}

static void spawnObstacle() {
  obstacle.active = true;
  obstacle.x = 320;
  obstacle.kind = esp_random() % 3;
  if (obstacle.kind == 2) {
    obstacle.w = 28;
    obstacle.h = 12;
    obstacle.y = DINO_GROUND_Y - 38; // standing dino hits it; ducking dino passes
  } else {
    obstacle.w = 14 + esp_random() % 8;
    obstacle.h = obstacle.kind == 0 ? 27 : 39;
    obstacle.y = DINO_GROUND_Y - obstacle.h;
  }
}

static void drawDino(float bottom, bool isDuck) {
  int h = dinoHeight(isDuck);
  int top = (int)bottom - h;
  if (isDuck) {
    // A short, horizontal Among Us body while ducking.
    gfxFillRect(DINO_X + 5, top + 4, 22, h - 4, DINO_COLOR);
    gfxFillCircle(DINO_X + 6, top + 9, 5, DINO_COLOR);
    gfxFillCircle(DINO_X + 26, top + 9, 5, DINO_COLOR);
    gfxFillRect(DINO_X + 24, top + 5, 8, h - 5, DINO_COLOR); // backpack
    gfxFillRect(DINO_X + 10, top + 5, 14, 6, DINO_VISOR);
    gfxFillRect(DINO_X + 12, top + 5, 4, 2, WHITE);
  } else {
    // Small crewmate silhouette: backpack, rounded body, visor, and legs.
    gfxFillRect(DINO_X + 23, top + 9, 8, 18, DINO_COLOR); // backpack
    gfxFillCircle(DINO_X + 14, top + 12, 12, DINO_COLOR);
    gfxFillRect(DINO_X + 3, top + 12, 22, h - 12, DINO_COLOR);
    gfxFillRect(DINO_X + 8, top + 7, 15, 7, DINO_VISOR);
    gfxFillRect(DINO_X + 10, top + 7, 4, 2, WHITE);
    gfxFillRect(DINO_X + 7, (int)bottom - 5, 6, 7, DINO_COLOR);
    gfxFillRect(DINO_X + 20, (int)bottom - 5, 6, 7, DINO_COLOR);
  }
}

static void eraseDino(float bottom, bool isDuck) {
  int h = dinoHeight(isDuck);
  gfxFillRect(DINO_X - 3, (int)bottom - h - 2, DINO_W + 7, h + 4, NAVY);
}

static void drawObstacle(const Obstacle &o) {
  if (o.kind == 2) {
    gfxFillRect((int)o.x, o.y + 3, o.w, 7, OBSTACLE_COLOR);
    gfxFillTriangle((int)o.x + 8, o.y + 2, (int)o.x + 14, o.y - 5,
                    (int)o.x + 19, o.y + 2, OBSTACLE_COLOR);
    gfxFillRect((int)o.x + 4, o.y + 9, 5, 5, OBSTACLE_COLOR);
    gfxFillRect((int)o.x + 19, o.y + 9, 5, 5, OBSTACLE_COLOR);
  } else {
    gfxFillRect((int)o.x, o.y, o.w, o.h, OBSTACLE_COLOR);
    gfxFillRect((int)o.x - 5, o.y + 8, 6, 5, OBSTACLE_COLOR);
    if (o.h > 30) gfxFillRect((int)o.x + o.w - 1, o.y + 16, 6, 5, OBSTACLE_COLOR);
  }
}

static void eraseObstacle(const Obstacle &o) {
  if (!o.active) return;
  int extra = o.kind == 2 ? 6 : 5;
  gfxFillRect((int)o.x - extra, o.y - 7, o.w + extra * 2, o.h + 14, NAVY);
}

static bool collides() {
  int dinoTop = (int)dinoBottom - dinoHeight(ducking) + 3;
  int dinoLeft = DINO_X + 5;
  int dinoRight = DINO_X + DINO_W - 2;
  int obstacleLeft = (int)obstacle.x + 2;
  int obstacleRight = (int)obstacle.x + obstacle.w - 2;
  int obstacleBottom = obstacle.kind == 2 ? obstacle.y + obstacle.h + 2 : DINO_GROUND_Y;
  return dinoLeft < obstacleRight && dinoRight > obstacleLeft &&
         dinoTop < obstacleBottom && (int)dinoBottom > obstacle.y + 2;
}

static void drawStatic() {
  gfxClear(NAVY);
  gfxText(112, 8, 3, WHITE, "DINO");
  gfxText(30, 222, 2, DIM, "A=jump  hold B=duck");
  gfxFillRect(0, DINO_GROUND_Y, 320, 2, DIM);
  drawn = true;
}

static void drawCounter() {
  if (passed == shownPassed) return;
  shownPassed = passed;
  gfxFillRect(250, 30, 66, 20, NAVY);
  char count[12];
  snprintf(count, sizeof(count), "%d/%d", passed, DINO_GOAL);
  gfxText(260, 32, 2, GREEN, count);
}

static void drawFailed() {
  gfxClear(NAVY);
  gfxText(92, 70, 3, RED, "GAME OVER");
  gfxText(75, 125, 2, DIM, "A=restart / wait");
  gfxText(80, 190, 2, DIM, "A=jump  hold B=duck");
  drawn = true;
}

static void start() {
  const PlayerColor &playerColor = colorByIndex(myColorIndex());
  DINO_COLOR = gfxColor(playerColor.r, playerColor.g, playerColor.b);
  DINO_VISOR = gfxColor(150, 210, 236);
  OBSTACLE_COLOR = gfxColor(230, 170, 60);
  resetRun();
}

static void run() {
  if (failed) {
    if (!drawn) drawFailed();
    if (isButtonPressed(BTN_A) || millis() - failedAt >= DINO_FAIL_MS) resetRun();
    return;
  }

  if (!drawn) drawStatic();

  if (rendered) {
    eraseDino(dinoBottom, ducking);
    eraseObstacle(obstacle);
    gfxFillRect(0, DINO_GROUND_Y, 320, 2, DIM);
  }

  ducking = isButtonHeld(BTN_B);
  bool onGround = dinoBottom >= DINO_GROUND_Y - 0.1f;
  if (isButtonPressed(BTN_A) && onGround) dinoVelocity = DINO_JUMP_V;

  dinoBottom += dinoVelocity;
  dinoVelocity += DINO_GRAVITY;
  if (dinoBottom >= DINO_GROUND_Y) {
    dinoBottom = DINO_GROUND_Y;
    dinoVelocity = 0;
  }

  if (!obstacle.active && millis() >= nextSpawnAt) spawnObstacle();
  if (obstacle.active) {
    obstacle.x -= DINO_SPEED + passed * 0.08f;
    if (obstacle.x + obstacle.w < 0) {
      obstacle.active = false;
      passed++;
      flashLEDs(0, 180, 0, 70);
      if (passed >= DINO_GOAL) { taskFinish(); return; }
      nextSpawnAt = millis() + 500 + esp_random() % 700;
    }
  }

  if (obstacle.active && collides()) {
    failed = true;
    failedAt = millis();
    drawn = false;
    rendered = false;
    flashLEDs(220, 0, 0, 220);
    return;
  }

  drawDino(dinoBottom, ducking);
  if (obstacle.active) drawObstacle(obstacle);
  drawCounter();
  rendered = true;
}

extern const TaskDef TASK_DINO = { "DINO", "049BC697DD2A81", start, run, 60000 };
