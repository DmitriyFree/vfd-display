#pragma once
#include <Arduino.h>

// ---------------- Pins (same as the old firmware) ----------------
constexpr uint8_t PIN_DIN   = 25;
constexpr uint8_t PIN_CLK   = 26;
constexpr uint8_t PIN_LOAD  = 27;
constexpr uint8_t PIN_BLANK = 14;   // MAX6921 BLANK: HIGH = all outputs off (used for brightness)

// ---------------- Display geometry ----------------
constexpr uint8_t NUM_DIGITS = 16;  // position 0 = leftmost
constexpr uint8_t NUM_GRIDS  = 8;   // each scan step lights 2 digits (one per MAX6921)

// ---------------- 40-bit stream layout ----------------
// Bit index in the stream (0 = first bit clocked in). Taken 1:1 from the old firmware.
constexpr uint8_t BIT_LOW_SEG   = 0;   // 8 bits, segments of digits 8..15
constexpr uint8_t BIT_LOW_GRID  = 12;  // 8 bits, grids of digits 8..15
constexpr uint8_t BIT_HIGH_SEG  = 20;  // 8 bits, segments of digits 0..7
constexpr uint8_t BIT_HIGH_GRID = 32;  // 8 bits, grids of digits 0..7

// 17th position (minus / small circle / big circle).
// ASSUMPTION: its grid is stream bit 29 (old code zeroed it). Segment lines are
// 25, 26, 28 (from the old "dop" code). Verify with the  /raw  command, see README text.
constexpr uint8_t BIT_SYM_GRID   = 29;
constexpr uint8_t BIT_SYM_SEG[3] = {25, 26, 28};

// ---------------- BLE ----------------
#define BLE_NAME      "June VFD"
#define BLE_SERVICE   "19B20000-E8F2-537E-4F6C-D104768A1214"
#define BLE_CHAR      "19B20001-E8F2-537E-4F6C-D104768A1214"
