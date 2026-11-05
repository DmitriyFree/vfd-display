#pragma once
#include <Arduino.h>

namespace vfd {
void begin();                                        // starts the scan task (core 1)
void setFrame(const uint8_t* cells16, uint8_t sym);  // 16 segment bytes + 3-bit symbol mask
void setBrightness(uint8_t b);                       // 0..255
uint8_t brightness();
void setSlowScan(uint16_t msPerStep);                // 0 = normal speed; >0 = show real multiplex order
void setRaw(uint64_t mask, bool enable);             // hold chosen stream bits high (wiring debug)
}
