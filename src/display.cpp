#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "display.h"

#define TFT_MOSI 10
#define TFT_SCLK 1
#define TFT_CS   2
#define TFT_DC   0
#define TFT_RST  4

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

void setupDisplay() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  
  tft.init(240, 320); 
  tft.setRotation(3); 
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); 
  tft.setTextSize(3);
}

// Among Us-style palette
#define C_NAVY   tft.color565(10, 12, 34)
#define C_RED    tft.color565(197, 27, 27)
#define C_REDDK  tft.color565(120, 12, 12)
#define C_VISOR  tft.color565(150, 210, 236)

// A classic Among Us crewmate: body, backpack, legs, visor. (cx,cy) = body center.
static void drawCrewmate(int cx, int cy, uint16_t body) {
  // backpack (behind the body, same color as the crewmate)
  tft.fillRoundRect(cx + 18, cy - 16, 16, 36, 7, body);
  // body (tall rounded capsule)
  tft.fillRoundRect(cx - 26, cy - 38, 50, 76, 22, body);
  // leg gap carved out of the bottom
  tft.fillRect(cx - 4, cy + 24, 9, 16, C_NAVY);
  // visor
  tft.fillRoundRect(cx - 20, cy - 24, 38, 18, 9, C_VISOR);
  tft.fillRoundRect(cx - 15, cy - 21, 12, 7, 3, ST77XX_WHITE); // shine
}

void showMeetingScreen() {
  tft.fillScreen(C_NAVY);
  drawCrewmate(58, 128, C_RED);

  // red title banner on the right
  tft.fillRoundRect(104, 44, 208, 78, 10, C_RED);
  tft.drawRoundRect(104, 44, 208, 78, 10, C_REDDK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.setCursor(118, 56);
  tft.print("EMERGENCY");
  tft.setCursor(142, 88);
  tft.print("MEETING");
}

void showMeetingWaiting() {
  tft.setTextColor(ST77XX_WHITE, C_NAVY);
  tft.setTextSize(2);
  tft.setCursor(118, 146);
  tft.print("Everyone here?");
  tft.setCursor(118, 170);
  tft.print("A=start B=cancel");
}

void showMeetingCountdown(int secondsLeft) {
  // pulsing alarm border (alternates each second)
  uint16_t border = (secondsLeft % 2 == 0) ? C_RED : tft.color565(240, 170, 20);
  for (int i = 0; i < 3; i++) {
    tft.drawRect(i, i, 320 - 2 * i, 240 - 2 * i, border);
  }
  // countdown + end hint (fixed width so old digits don't ghost)
  tft.setTextColor(ST77XX_WHITE, C_NAVY);
  tft.setTextSize(2);
  char buf[24];
  snprintf(buf, sizeof(buf), "Discuss: %2ds  ", secondsLeft);
  tft.setCursor(118, 146);
  tft.print(buf);
  tft.setCursor(118, 170);
  tft.print("B = end early ");
}

void clearScreen() {
  tft.fillScreen(ST77XX_BLACK);
}

// small crewmate glyph for the teammate list
static void drawMiniMate(int cx, int cy, uint16_t body) {
  tft.fillRoundRect(cx - 8, cy - 11, 16, 22, 6, body);
  tft.fillRoundRect(cx - 6, cy - 7, 11, 5, 2, C_VISOR);
}

void showRoleCard(int colorR, int colorG, int colorB, bool isImpostor,
                  int nTeam, const uint8_t *teamR, const uint8_t *teamG,
                  const uint8_t *teamB, int killCooldownSecs) {
  uint16_t bg = isImpostor ? tft.color565(40, 0, 0) : tft.color565(0, 10, 30);
  uint16_t banner = isImpostor ? C_RED : tft.color565(40, 90, 220);
  tft.fillScreen(bg);

  // you, in your color
  drawCrewmate(70, 120, tft.color565(colorR, colorG, colorB));

  tft.fillRoundRect(140, 40, 170, 46, 8, banner);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.setCursor(150, 52);
  tft.print(isImpostor ? "IMPOSTOR" : "CREW");
  tft.setTextColor(ST77XX_WHITE, bg);
  tft.setTextSize(2);
  tft.setCursor(140, 100);
  tft.print(isImpostor ? "Sabotage & kill" : "Do your tasks");

  // how to use the B button, since it does double duty (kill vs. report)
  tft.setTextColor(tft.color565(200, 200, 210), bg);
  tft.setCursor(140, 120);
  tft.print(isImpostor ? "Hold B = kill" : "Hold B = report");

  // fellow impostors, if any
  if (isImpostor && nTeam > 0) {
    tft.setTextColor(tft.color565(255, 170, 170), bg);
    tft.setTextSize(2);
    tft.setCursor(140, 140);
    tft.print("Team:");
    for (int i = 0; i < nTeam && i < 6; i++) {
      drawMiniMate(230 + i * 26, 150, tft.color565(teamR[i], teamG[i], teamB[i]));
    }
  }

  // kill cooldown, so the impostor knows when they can attack again
  if (isImpostor) {
    tft.setTextSize(2);
    tft.setCursor(140, 185);
    if (killCooldownSecs > 0) {
      tft.setTextColor(tft.color565(255, 140, 60), bg);
      char buf[20]; snprintf(buf, sizeof(buf), "Cooldown: %ds ", killCooldownSecs);
      tft.print(buf);
    } else {
      tft.setTextColor(tft.color565(120, 230, 140), bg);
      tft.print("Ready to kill ");
    }
  }
}

void showLobby(int players, int imp, int disc, int vote, int meet, int sel,
               int colorR, int colorG, int colorB) {
  tft.fillScreen(C_NAVY);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.setCursor(20, 10);
  tft.print("AMONG US");
  drawCrewmate(285, 30, tft.color565(colorR, colorG, colorB));

  const char *labels[4];
  char rows[4][20];
  snprintf(rows[0], 20, "Impostors: %d", imp);
  snprintf(rows[1], 20, "Discuss: %ds", disc);
  snprintf(rows[2], 20, "Vote: %ds", vote);
  snprintf(rows[3], 20, "Meetings: %d", meet);
  for (int i = 0; i < 4; i++) labels[i] = rows[i];

  tft.setTextSize(2);
  for (int i = 0; i < 4; i++) {
    int y = 55 + i * 26;
    if (i == sel) { tft.setTextColor(tft.color565(240, 220, 60), C_NAVY); tft.setCursor(6, y); tft.print(">"); }
    else tft.setTextColor(tft.color565(190, 190, 210), C_NAVY);
    tft.setCursor(24, y);
    tft.print(labels[i]);
  }

  tft.setTextColor(ST77XX_WHITE, C_NAVY);
  tft.setCursor(20, 178);
  char f[24]; snprintf(f, sizeof(f), "Players: %d", players);
  tft.print(f);
  tft.setTextColor(tft.color565(240, 220, 60), C_NAVY);
  tft.setCursor(20, 205);
  tft.print("START = play");
}

void showHUD(bool alive, int aliveCount, int colorR, int colorG, int colorB,
             bool bodyNearby) {
  tft.fillScreen(C_NAVY);
  if (!alive) {
    drawCrewmate(80, 120, tft.color565(70, 70, 80));  // grey ghost
    tft.setTextColor(tft.color565(160, 160, 175), C_NAVY);
    tft.setTextSize(3);
    tft.setCursor(150, 100);
    tft.print("GHOST");
    tft.setTextSize(2);
    tft.setCursor(150, 140);
    tft.print("spectating");
    return;
  }
  drawCrewmate(70, 120, tft.color565(colorR, colorG, colorB));
  // show only the player's color, never the secret role
  tft.setTextColor(tft.color565(colorR, colorG, colorB), C_NAVY);
  tft.setTextSize(3);
  tft.setCursor(150, 70);
  tft.print("YOU");
  tft.setTextColor(tft.color565(190, 190, 210), C_NAVY);
  tft.setTextSize(2);
  char b[20]; snprintf(b, sizeof(b), "Alive: %d ", aliveCount);
  tft.setCursor(150, 115);
  tft.print(b);

  if (bodyNearby) {
    tft.setTextColor(C_RED, C_NAVY);
    tft.setTextSize(2);
    tft.setCursor(150, 148);
    tft.print("BODY NEARBY");
    tft.setCursor(150, 170);
    tft.print("HOLD B=report");
  }

  tft.setTextColor(tft.color565(150, 150, 170), C_NAVY);
  tft.setCursor(14, 200);
  tft.print("tap HOME=meeting");
  tft.setCursor(14, 220);
  tft.print("hold START=role");
}

void showTaskAlreadyCompleted(const char *name) {
  tft.fillScreen(C_NAVY);
  tft.setTextColor(tft.color565(240, 220, 60), C_NAVY);
  tft.setTextSize(3);
  int nameWidth = strlen(name) * 18;
  tft.setCursor((320 - nameWidth) / 2, 80);
  tft.print(name);
  tft.setTextSize(2);
  tft.setCursor(58, 130);
  tft.print("ALREADY COMPLETED");
}

uint16_t gfxColor(uint8_t r, uint8_t g, uint8_t b) { return tft.color565(r, g, b); }
void gfxClear(uint16_t color) { tft.fillScreen(color); }
void gfxText(int x, int y, int size, uint16_t color, const char *s) {
  tft.setTextColor(color); tft.setTextSize(size); tft.setCursor(x, y); tft.print(s);
}
void gfxRectOutline(int x, int y, int w, int h, uint16_t color) { tft.drawRect(x, y, w, h, color); }
void gfxFillRect(int x, int y, int w, int h, uint16_t color) { tft.fillRect(x, y, w, h, color); }
void gfxFillCircle(int x, int y, int r, uint16_t color) { tft.fillCircle(x, y, r, color); }
void gfxFillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
  tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
}

void drawTaskBar(int pct) {
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  int w = 300, x = 10, y = 6, h = 12;
  tft.drawRect(x, y, w, h, ST77XX_WHITE);
  tft.fillRect(x + 1, y + 1, (w - 2) * pct / 100, h - 2, tft.color565(70, 210, 90));
}

void showGameOver(bool crewWon) {
  tft.fillScreen(crewWon ? tft.color565(0, 20, 45) : tft.color565(0, 35, 15));
  tft.setTextColor(crewWon ? tft.color565(70, 160, 240) : tft.color565(70, 220, 110));
  tft.setTextSize(4);
  if (crewWon) { tft.setCursor(40, 70); tft.print("CREW"); tft.setCursor(40, 115); tft.print("WINS!"); }
  else { tft.setCursor(10, 70); tft.print("IMPOSTOR"); tft.setCursor(70, 115); tft.print("WINS"); }
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 205);
  tft.print("START = new lobby");
}

void showVote(const char *name, int colorR, int colorG, int colorB,
              bool isSkip, int secondsLeft, bool alreadyVoted) {
  tft.fillScreen(C_NAVY);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.setCursor(70, 18);
  tft.print("VOTE OUT");

  if (alreadyVoted) {
    tft.setTextColor(tft.color565(185, 185, 205), C_NAVY);
    tft.setTextSize(3);
    tft.setCursor(70, 110);
    tft.print("VOTED");
  } else if (isSkip) {
    tft.setTextSize(4);
    tft.setCursor(100, 100);
    tft.print("SKIP");
  } else {
    drawCrewmate(80, 132, tft.color565(colorR, colorG, colorB));
    tft.setTextColor(tft.color565(colorR, colorG, colorB), C_NAVY);
    tft.setTextSize(3);
    tft.setCursor(150, 120);
    tft.print(name);
  }

  tft.setTextColor(tft.color565(185, 185, 205), C_NAVY);
  tft.setTextSize(2);
  char buf[28];
  snprintf(buf, sizeof(buf), "%2ds  L/R  A=vote ", secondsLeft);
  tft.setCursor(24, 205);
  tft.print(buf);
}

void showResult(const char *ejName, int ejR, int ejG, int ejB,
                bool skipped, bool wasImpostor,
                int nTally, const uint8_t *talR, const uint8_t *talG,
                const uint8_t *talB, const int *talCounts, int skipCount) {
  tft.fillScreen(C_NAVY);
  // ejection line at the top
  tft.setTextSize(2);
  if (skipped) {
    tft.setTextColor(ST77XX_WHITE, C_NAVY);
    tft.setCursor(20, 18);
    tft.print("No one was ejected");
  } else {
    tft.setTextColor(tft.color565(ejR, ejG, ejB), C_NAVY);
    tft.setCursor(20, 14);
    tft.print(ejName);
    tft.setTextColor(ST77XX_WHITE, C_NAVY);
    tft.setCursor(20, 40);
    tft.print("ejected -");
    tft.setTextColor(wasImpostor ? tft.color565(80, 220, 120) : C_RED, C_NAVY);
    tft.setCursor(140, 40);
    tft.print(wasImpostor ? "Impostor" : "innocent");
  }
  // tally row
  tft.setTextColor(tft.color565(185, 185, 205), C_NAVY);
  tft.setCursor(20, 90);
  tft.print("Votes");
  int x = 20, y = 135;
  for (int i = 0; i < nTally && i < 6; i++) {
    drawMiniMate(x + 12, y, tft.color565(talR[i], talG[i], talB[i]));
    tft.setTextColor(ST77XX_WHITE, C_NAVY);
    tft.setCursor(x + 6, y + 20);
    char b[6]; snprintf(b, sizeof(b), "%d", talCounts[i]);
    tft.print(b);
    x += 46;
  }
  if (skipCount > 0) {
    tft.setTextColor(tft.color565(150, 150, 170), C_NAVY);
    tft.setCursor(x, y - 4);
    tft.print("skip");
    tft.setCursor(x + 6, y + 20);
    char b[6]; snprintf(b, sizeof(b), "%d", skipCount);
    tft.print(b);
  }
}
