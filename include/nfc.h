#pragma once
#include <Arduino.h>

// Initialize the reader once while leaving its antenna in low-power standby.
void setupNFC();

// Call this ONLY when the device is ready to accept a task tag scan
void beginNFCScan();

// Poll this in your main loop while scanning is active
String scanNFC();

// Enter low-power standby without cutting reader power; registers are retained
// so beginNFCScan() can wake it quickly.
void powerDownNFC();
