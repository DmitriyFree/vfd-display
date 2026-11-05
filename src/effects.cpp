#include "effects.h"
#include "config.h"
#include "font.h"
#include "vfd.h"
#include <vector>

namespace fx {

Settings cfg;

static Mode mode = M_TEXT;
static String curText;
static std::vector<uint8_t> cells;
static std::vector<std::vector<uint8_t>> pages;
static uint32_t tLast = 0, step = 0;
static uint8_t phase = 0;
static size_t pageIdx = 0;
static uint8_t rnd[NUM_DIGITS] = {0};

// ---------- text -> segment cells ----------
static std::vector<uint8_t> textToCells(const String& s) {
  std::vector<uint8_t> out;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (cfg.dotMerge && (c == '.' || c == ',') && !out.empty() && !(out.back() & 0x80)) {
      out.back() |= 0x80;  // attach dot to previous digit
      continue;
    }
    out.push_back(glyph(c));
  }
  return out;
}

static void place(const std::vector<uint8_t>& c, uint8_t* out) {
  memset(out, 0, NUM_DIGITS);
  int n = c.size();
  if (n >= NUM_DIGITS) {
    int off = cfg.align == 'r' ? n - NUM_DIGITS : cfg.align == 'c' ? (n - NUM_DIGITS) / 2 : 0;
    for (int k = 0; k < NUM_DIGITS; k++) out[k] = c[off + k];
  } else {
    int off = cfg.align == 'r' ? NUM_DIGITS - n : cfg.align == 'c' ? (NUM_DIGITS - n) / 2 : 0;
    for (int i = 0; i < n; i++) out[off + i] = c[i];
  }
}

// Pages for vertical scroll: split on '|', long pieces chunked into 16 digits.
static void buildPages(const String& t) {
  pages.clear();
  int from = 0;
  for (;;) {
    int idx = t.indexOf('|', from);
    String seg = idx < 0 ? t.substring(from) : t.substring(from, idx);
    std::vector<uint8_t> c = textToCells(seg);
    if (c.empty()) pages.push_back(c);
    for (size_t i = 0; i < c.size(); i += NUM_DIGITS) {
      size_t e = min(c.size(), i + (size_t)NUM_DIGITS);
      pages.emplace_back(c.begin() + i, c.begin() + e);
    }
    if (idx < 0) break;
    from = idx + 1;
  }
}

// ---------- vertical roll helpers ----------
// A digit has 5 rows: a | f,b | g | e,c | d(+dp). Rows move in steps of 2 so bar rows stay bar rows.
static void toRows(uint8_t g, uint8_t* r) {
  r[0] = (g & 0x01) ? 1 : 0;
  r[1] = ((g & 0x20) ? 1 : 0) | ((g & 0x02) ? 2 : 0);
  r[2] = (g & 0x40) ? 1 : 0;
  r[3] = ((g & 0x10) ? 1 : 0) | ((g & 0x04) ? 2 : 0);
  r[4] = ((g & 0x08) ? 1 : 0) | ((g & 0x80) ? 2 : 0);
}
static uint8_t fromRows(const uint8_t* r) {
  uint8_t g = 0;
  if (r[0] & 1) g |= 0x01;
  if (r[1] & 1) g |= 0x20;
  if (r[1] & 2) g |= 0x02;
  if (r[2] & 1) g |= 0x40;
  if (r[3] & 1) g |= 0x10;
  if (r[3] & 2) g |= 0x04;
  if (r[4] & 1) g |= 0x08;
  if (r[4] & 2) g |= 0x80;
  return g;
}
// strip = first(5 rows) + blank row + second(5 rows); window of 5 rows at offset k.
static void rollFrame(const uint8_t* cur, const uint8_t* nxt, int k, bool up, uint8_t* out) {
  for (int c = 0; c < NUM_DIGITS; c++) {
    uint8_t ra[5], rb[5], s[11], w[5];
    toRows(cur[c], ra);
    toRows(nxt[c], rb);
    const uint8_t* first  = up ? ra : rb;
    const uint8_t* second = up ? rb : ra;
    for (int i = 0; i < 5; i++) s[i] = first[i];
    s[5] = 0;
    for (int i = 0; i < 5; i++) s[6 + i] = second[i];
    for (int j = 0; j < 5; j++) w[j] = s[k + j];
    out[c] = fromRows(w);
  }
}

// ---------- public API ----------
void setMode(Mode m) {
  mode = m;
  step = 0;
  phase = 0;
  tLast = millis();
}

void setText(Mode m, const String& text) {
  curText = text;
  cells = textToCells(text);
  if (m == M_VSCROLL) buildPages(text);
  pageIdx = 0;
  setMode(m);
}

void refresh() { setText(mode, curText); }
const String& currentText() { return curText; }

void update() {
  uint32_t now = millis();
  uint32_t sp = cfg.speed ? cfg.speed : 1;
  uint8_t out[NUM_DIGITS] = {0};
  uint8_t sym = cfg.sym;

  switch (mode) {
    case M_TEXT:
      place(cells, out);
      break;

    case M_SCROLL: {
      int n = cells.size();
      int L = n + NUM_DIGITS;  // text + a blank screen, so it enters and leaves fully
      if (now - tLast >= sp) { tLast = now; step = (step + 1) % L; }
      int shift = (cfg.dir == 'r') ? (((n - 1 - (int)step) % L) + L) % L
                                   : ((int)step + L - NUM_DIGITS) % L;
      for (int k = 0; k < NUM_DIGITS; k++) {
        int idx = (shift + k) % L;
        out[k] = idx < n ? cells[idx] : 0;
      }
      break;
    }

    case M_VSCROLL: {
      if (pages.empty()) break;
      size_t nxt = (pageIdx + 1) % pages.size();
      if (pages.size() > 1) {
        if (phase == 0) {
          if (now - tLast >= cfg.hold) { phase = 1; tLast = now; }
        } else if (now - tLast >= sp) {
          tLast = now;
          if (++phase > 3) { phase = 0; pageIdx = nxt; nxt = (pageIdx + 1) % pages.size(); }
        }
      }
      uint8_t a[NUM_DIGITS], b[NUM_DIGITS];
      place(pages[pageIdx], a);
      place(pages[nxt], b);
      if (phase == 0) {
        memcpy(out, a, NUM_DIGITS);
      } else {
        bool up = cfg.dir != 'd';
        int k = up ? 2 * phase : 6 - 2 * phase;
        rollFrame(a, b, k, up, out);
      }
      break;
    }

    case M_RUNDOT: {
      place(cells, out);
      if (now - tLast >= sp) { tLast = now; step++; }
      int p = (cfg.dir == 'r') ? step % NUM_DIGITS : (NUM_DIGITS - 1) - (step % NUM_DIGITS);
      out[p] |= 0x80;
      break;
    }

    case M_RANDOM:
      if (now - tLast >= sp) {
        tLast = now;
        for (int k = 0; k < NUM_DIGITS; k++) {
          uint8_t v = 0;
          for (int b = 0; b < 8; b++)
            if ((uint8_t)random(100) < cfg.density) v |= (1 << b);
          rnd[k] = v;
        }
      }
      memcpy(out, rnd, NUM_DIGITS);
      break;

    case M_SWEEP: {
      const int total = NUM_DIGITS * 8 + 3;  // every segment of every digit, then the 3 symbols
      if (now - tLast >= sp) { tLast = now; step = (step + 1) % total; }
      int s = step;
      sym = 0;
      if (!cfg.acc) {
        if (s < NUM_DIGITS * 8) out[s / 8] = 1 << (s % 8);  // order: a b c d e f g dp, left to right
        else sym = 1 << (s - NUM_DIGITS * 8);
      } else {
        for (int i = 0; i <= s && i < NUM_DIGITS * 8; i++) out[i / 8] |= 1 << (i % 8);
        if (s >= NUM_DIGITS * 8) sym = (2 << (s - NUM_DIGITS * 8)) - 1;
      }
      break;
    }
  }

  if (cfg.blinkMs && ((now / cfg.blinkMs) & 1)) {
    memset(out, 0, NUM_DIGITS);
    sym = 0;
  }
  vfd::setFrame(out, sym);
}

}  // namespace fx
