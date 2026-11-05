#include "vfd.h"
#include "config.h"

namespace vfd {

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t  g_cells[NUM_DIGITS] = {0};
static uint8_t  g_sym    = 0;
static uint16_t g_slow   = 0;   // ms per scan step, 0 = normal speed

// Clock 40 bits into the two daisy-chained MAX6921, bit 0 first, then latch.
static void shift40(uint64_t bits) {
  for (uint8_t i = 0; i < 40; i++) {
    digitalWrite(PIN_DIN, (bits >> i) & 1);
    digitalWrite(PIN_CLK, HIGH);
    digitalWrite(PIN_CLK, LOW);
  }
  digitalWrite(PIN_LOAD, HIGH);
  digitalWrite(PIN_LOAD, LOW);
}

// One scan step: grid i lights digit (7-i) on the "high" chip and digit (15-i) on the "low" chip.
// Step 8 (only if symbols are on) lights the 17th position.
static uint64_t buildStep(uint8_t step, const uint8_t* cells, uint8_t sym) {
  uint64_t b = 0;
  if (step < NUM_GRIDS) {
    uint64_t grid = 1ULL << step;
    b |= (uint64_t)cells[15 - step] << BIT_LOW_SEG;
    b |= grid << BIT_LOW_GRID;
    b |= (uint64_t)cells[7 - step] << BIT_HIGH_SEG;
    b |= grid << BIT_HIGH_GRID;
  } else {
    b |= 1ULL << BIT_SYM_GRID;
    for (uint8_t k = 0; k < 3; k++)
      if (sym & (1 << k)) b |= 1ULL << BIT_SYM_SEG[k];
  }
  return b;
}

static void scanTask(void*) {
  TickType_t last = xTaskGetTickCount();
  uint8_t step = 0;

  for (;;) {
    uint8_t cells[NUM_DIGITS];
    uint8_t sym;
    uint16_t slow;

    portENTER_CRITICAL(&mux);
    memcpy(cells, g_cells, NUM_DIGITS);
    sym = g_sym; slow = g_slow;
    portEXIT_CRITICAL(&mux);

    uint8_t steps = sym ? NUM_GRIDS + 1 : NUM_GRIDS;
    if (step >= steps) step = 0;

    digitalWrite(PIN_BLANK, HIGH);            // blank while data changes: no ghosting
    shift40(buildStep(step, cells, sym));
    digitalWrite(PIN_BLANK, LOW);
    if (slow) {                               // human speed: one lit digit per half, then the next pair
      vTaskDelay(pdMS_TO_TICKS(slow));
      last = xTaskGetTickCount();
    } else {
      vTaskDelayUntil(&last, 1);
    }
    step++;
  }
}

void begin() {
  pinMode(PIN_DIN, OUTPUT);
  pinMode(PIN_CLK, OUTPUT);
  pinMode(PIN_LOAD, OUTPUT);
  pinMode(PIN_BLANK, OUTPUT);
  digitalWrite(PIN_CLK, LOW);
  digitalWrite(PIN_LOAD, LOW);
  digitalWrite(PIN_BLANK, HIGH);
  xTaskCreatePinnedToCore(scanTask, "vfd", 3072, nullptr, 3, nullptr, 1);
}

void setFrame(const uint8_t* cells16, uint8_t sym) {
  portENTER_CRITICAL(&mux);
  memcpy(g_cells, cells16, NUM_DIGITS);
  g_sym = sym & 7;
  portEXIT_CRITICAL(&mux);
}

void setSlowScan(uint16_t ms) { g_slow = ms; }

}  // namespace vfd
