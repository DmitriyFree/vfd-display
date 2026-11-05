#pragma once
#include <Arduino.h>

namespace fx {

enum Mode : uint8_t { M_TEXT, M_SCROLL, M_VSCROLL, M_RANDOM };

struct Settings {
  uint16_t speed    = 150;   // ms per animation step
  uint16_t hold     = 1500;  // ms a page stays still (vscroll)
  char     dir      = 'l';   // l r u d  (direction of motion)
  char     align    = 'c';   // l c r
  uint16_t blinkMs  = 0;     // 0 = no blinking (overlay on any mode)
  uint8_t  density  = 25;    // random mode, % of segments lit
  bool     dotMerge = true;  // '.' and ',' light the dp of the previous digit
  uint8_t  sym      = 0;     // 17th position bitmask (0..7)
};

extern Settings cfg;

void setText(Mode m, const String& text);  // text modes: TEXT, SCROLL, VSCROLL
void setMode(Mode m);                      // text-less mode: RANDOM
const String& currentText();
Mode currentMode();
void update();                             // call often from loop()

}  // namespace fx
