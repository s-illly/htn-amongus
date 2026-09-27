#include "buttons.h"

#define HC165_DATA 7
#define HC165_LOAD 20
#define HC165_CLK  21
#define START_BTN  9

static uint16_t debouncedState = 0;
static uint16_t pressedEdges = 0;   // buttons that went down this loop iteration
static uint16_t lastRawState = 0;
static unsigned long lastDebounceTime = 0;

void setupButtons() {
  pinMode(HC165_DATA, INPUT);
  pinMode(HC165_LOAD, OUTPUT);
  pinMode(HC165_CLK, OUTPUT);
  
  // Start button is active-low. Use internal pullup to ensure it stays 
  // high (unpressed) when floating, which is also required for standard boot.
  pinMode(START_BTN, INPUT_PULLUP);
  
  // Set default idle states for the shift register lines
  digitalWrite(HC165_LOAD, HIGH);
  digitalWrite(HC165_CLK, LOW);
}

void updateButtons() {
  // A press is reported for exactly one loop iteration: the loop runs many
  // times per 10ms poll, and a lingering edge would let one physical press
  // fire twice (e.g. game over -> lobby -> straight into a new round).
  pressedEdges = 0;

  // Enforce a ~10ms polling and debounce interval
  if (millis() - lastDebounceTime < 10) {
    return;
  }
  lastDebounceTime = millis();

  // 1. Latch the shift register inputs (LOAD low then high)
  digitalWrite(HC165_LOAD, LOW);
  digitalWrite(HC165_LOAD, HIGH);

  uint16_t currentRawState = 0;

  // 2. Read 8 bits from the HC165
  // A shifts out first, followed by B, Home, Down, Left, Right, Up, Aux1
  for (int i = 0; i < 8; i++) {
    // Sample DATA
    if (digitalRead(HC165_DATA) == LOW) {
      currentRawState |= (1 << i); // Invert active-low to positive logic
    }

    // Pulse CLK high-low to shift the next bit out
    digitalWrite(HC165_CLK, HIGH);
    digitalWrite(HC165_CLK, LOW);
  }

  // 3. Read the standalone START button on GPIO 9
  if (digitalRead(START_BTN) == LOW) {
    currentRawState |= BTN_START;
  }

  // 4. Debounce evaluation
  // If the raw state hasn't changed since the last 10ms check, it's considered stable.
  if (currentRawState == lastRawState) {
    pressedEdges = currentRawState & ~debouncedState;
    debouncedState = currentRawState;
  }
  
  lastRawState = currentRawState;
}

uint16_t getButtons() {
  return debouncedState;
}

bool isButtonPressed(uint16_t buttonMask) {
  // Returns true if the bit is set in the current state but was NOT set in the previous state
  return (pressedEdges & buttonMask) != 0;
}

bool isButtonHeld(uint16_t buttonMask) {
  return (debouncedState & buttonMask) != 0;
}
