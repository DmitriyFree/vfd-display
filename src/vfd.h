#pragma once
#include <Arduino.h>

namespace vfd {
void begin();                                        // starts the scan task (core 1)
void setFrame(const uint8_t* cells16, uint8_t sym);  // 16 segment bytes + 3-bit symbol mask
void setSlowScan(uint16_t msPerStep);                // 0 = normal; >0 = hold each scan step (one digit per half)
}
