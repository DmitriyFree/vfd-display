#include <Arduino.h>
#include <vector>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include "config.h"
#include "vfd.h"
#include "effects.h"

static Preferences prefs;
static const char* DEFAULT_TEXT = " - june youtube - ";
static String rxBuf;
static uint32_t rxLast = 0;

static void handleLine(String line);   // forward declaration

// ---------------- sequencer state ----------------
struct Block { uint32_t ms; String cmds; };   // cmds: several commands joined with " ;; "
static std::vector<Block> seq;
static bool seqActive = false, seqLoop = false;
static bool internalExec = false;             // true while the firmware runs commands itself
static size_t seqIdx = 0;
static uint32_t seqNextAt = 0;

// ---------------- BLE (NimBLE) ----------------
struct CmdMsg { char s[256]; };
static QueueHandle_t cmdQueue;

class RxCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    std::string v = c->getValue();
    CmdMsg m;
    size_t n = v.size() < sizeof(m.s) - 1 ? v.size() : sizeof(m.s) - 1;
    memcpy(m.s, v.data(), n);
    m.s[n] = 0;
    xQueueSend(cmdQueue, &m, 0);
  }
};

class SrvCb : public NimBLEServerCallbacks {
  void onDisconnect(NimBLEServer*) override { NimBLEDevice::startAdvertising(); }
};

static void bleBegin() {
  cmdQueue = xQueueCreate(8, sizeof(CmdMsg));
  NimBLEDevice::init(BLE_NAME);
  NimBLEDevice::setMTU(247);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new SrvCb());
  NimBLEService* svc = server->createService(BLE_SERVICE);
  NimBLECharacteristic* ch = svc->createCharacteristic(
      BLE_CHAR, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  ch->setCallbacks(new RxCb());
  ch->setValue(fx::currentText().c_str());
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SERVICE);
  adv->setScanResponse(true);
  adv->start();
  Serial.print("BLE address: ");
  Serial.println(NimBLEDevice::getAddress().toString().c_str());
}

// ---------------- command parsing ----------------
static String takeToken(String& s) {
  int len = s.length(), i = 0;
  while (i < len && s[i] == ' ') i++;
  int j = i;
  while (j < len && s[j] != ' ') j++;
  String t = s.substring(i, j);
  s = (j < len) ? s.substring(j + 1) : String("");
  return t;
}

static bool applyOpt(const String& tok) {
  int eq = tok.indexOf('=');
  if (eq <= 0) return false;
  String k = tok.substring(0, eq), v = tok.substring(eq + 1);
  k.toLowerCase();
  if (k == "speed")        fx::cfg.speed = constrain(v.toInt(), 1, 60000);
  else if (k == "hold")    fx::cfg.hold = constrain(v.toInt(), 0, 60000);
  else if (k == "dir")     fx::cfg.dir = v.length() ? tolower(v[0]) : 'l';
  else if (k == "align")   fx::cfg.align = v.length() ? tolower(v[0]) : 'l';
  else if (k == "density") fx::cfg.density = constrain(v.toInt(), 0, 100);
  else if (k == "blink")   fx::cfg.blinkMs = constrain(v.toInt(), 0, 60000);
  else if (k == "sym")     fx::cfg.sym = v.toInt() & 7;
  else return false;
  return true;
}

// Leading key=value tokens are options, the rest is the text.
static void parseOpts(String& rest) {
  for (;;) {
    String probe = rest;
    String tok = takeToken(probe);
    if (tok == "--") { rest = probe; break; }   // explicit end of options
    if (tok.length() && applyOpt(tok)) rest = probe; else break;
  }
}

static void saveBoot(const String& line) {
  if (prefs.getString("boot", "") != line) prefs.putString("boot", line);
}

// Whole state (mode, options, text) as one command line, written to flash automatically.
static void saveState() {
  if (internalExec) return;                   // sequencer steps are never written to flash
  fx::Mode m = fx::currentMode();
  const char* name = m == fx::M_SCROLL ? "scroll" : m == fx::M_VSCROLL ? "vscroll" : m == fx::M_RANDOM ? "random" : "text";
  String line = String("/") + name +
      " speed=" + String(fx::cfg.speed) + " hold=" + String(fx::cfg.hold) +
      " dir=" + String(fx::cfg.dir) + " align=" + String(fx::cfg.align) +
      " density=" + String(fx::cfg.density) + " blink=" + String(fx::cfg.blinkMs) +
      " sym=" + String(fx::cfg.sym) + " -- " + fx::currentText();
  saveBoot(line);
}

static void saveSeq() {
  String s;
  for (auto& b : seq) s += String(b.ms) + "\t" + b.cmds + "\n";
  prefs.putString("seq", s);
}

static void loadSeq() {
  seq.clear();
  String s = prefs.getString("seq", "");
  int from = 0;
  while (from < (int)s.length()) {
    int e = s.indexOf('\n', from);
    if (e < 0) e = s.length();
    String ln = s.substring(from, e);
    int t = ln.indexOf('\t');
    if (t > 0) seq.push_back({(uint32_t)ln.substring(0, t).toInt(), ln.substring(t + 1)});
    from = e + 1;
  }
}

// Every block starts from clean defaults, then runs its commands in order.
static void runBlock(const String& cmds) {
  internalExec = true;
  fx::cfg = fx::Settings();
  vfd::setSlowScan(0);
  String rest = cmds;
  for (;;) {
    int i = rest.indexOf(" ;; ");
    handleLine(i < 0 ? rest : rest.substring(0, i));
    if (i < 0) break;
    rest = rest.substring(i + 4);
  }
  internalExec = false;
}

static void restoreSaved() {
  internalExec = true;
  fx::cfg = fx::Settings();
  vfd::setSlowScan(0);
  String boot = prefs.getString("boot", "");
  if (boot.length()) handleLine(boot); else fx::setText(fx::M_TEXT, DEFAULT_TEXT);
  internalExec = false;
}

static void seqStart(bool loop) {
  seqLoop = loop;
  seqActive = true;
  seqIdx = 0;
  seqNextAt = millis();
}

static void seqStop(bool restore) {
  seqActive = false;
  if (restore) restoreSaved();
}

static void seqTick() {
  if (!seqActive || (int32_t)(millis() - seqNextAt) < 0) return;
  if (seqIdx >= seq.size()) {
    uint32_t total = 0;
    for (auto& b : seq) total += b.ms;
    if (seqLoop && total > 0) seqIdx = 0; else { seqActive = false; return; }
  }
  Block& b = seq[seqIdx++];
  runBlock(b.cmds);
  seqNextAt = millis() + b.ms;
}

static void help() {
  Serial.println(F(
    "Plain text            -> show it (saved, restored at boot)\n"
    "/text [opts] T        static text          /clear\n"
    "/scroll [opts] T      horizontal marquee   (dir=l|r)\n"
    "/vscroll [opts] T     vertical pages, split with |  (dir=u|d, hold=ms)\n"
    "/random [opts]        random segments (density=0..100)\n"
    "/blink [ms|off]       blink overlay on any mode\n"
    "/slow ms|0            scan in slow motion: one digit per half at a time (try 300), 0 = off\n"
    "/sym 0..7             17th position (minus/small/big circle bitmask)\n"
    "/align l|c|r\n"
    "/reset                forget saved state, back to defaults\n"
    "/seq add <ms> <cmd> [ ;; <cmd> ...]   append a block (any command or plain text)\n"
    "/seq play | loop | stop | list | clear   (loop restarts automatically after reboot)\n"
    "opts: speed= hold= dir= align= density= blink= sym=   (end with -- if text looks like an option)\n"
    "State is saved to flash automatically."));
}

static void handleLine(String line) {
  while (line.endsWith("\n") || line.endsWith("\r")) line.remove(line.length() - 1);
  if (line.length() == 0) return;

  if (!internalExec && seqActive && !line.startsWith("/seq") && !line.startsWith("/help"))
    seqStop(true);                            // any manual command ends the show

  if (line[0] != '/') {                       // plain text, like the old firmware
    fx::setText(fx::M_TEXT, line);
    saveState();
    return;
  }

  String rest = line.substring(1);
  String cmd = takeToken(rest);
  cmd.toLowerCase();

  if (cmd == "text")        { parseOpts(rest); fx::setText(fx::M_TEXT, rest); saveState(); }
  else if (cmd == "scroll") { fx::cfg.speed = 150; parseOpts(rest); fx::setText(fx::M_SCROLL, rest.length() ? rest : fx::currentText()); saveState(); }
  else if (cmd == "vscroll"){ fx::cfg.speed = 120; parseOpts(rest); fx::setText(fx::M_VSCROLL, rest.length() ? rest : fx::currentText()); saveState(); }
  else if (cmd == "random") { fx::cfg.speed = 70;  parseOpts(rest); fx::setMode(fx::M_RANDOM); saveState(); }
  else if (cmd == "clear")  { fx::setText(fx::M_TEXT, ""); saveState(); }
  else if (cmd == "blink")  {
    String t = takeToken(rest); t.toLowerCase();
    fx::cfg.blinkMs = (t == "off" || t == "0") ? 0 : (t.length() ? t.toInt() : 500);
    saveState();
  }
  else if (cmd == "slow")   { long v = takeToken(rest).toInt(); vfd::setSlowScan(v < 0 ? 0 : v > 2000 ? 2000 : v); }
  else if (cmd == "sym")    { fx::cfg.sym = takeToken(rest).toInt() & 7; saveState(); }
  else if (cmd == "align")  { String t = takeToken(rest); if (t.length()) fx::cfg.align = tolower(t[0]); saveState(); }
  else if (cmd == "reset")  {
    if (!internalExec) prefs.remove("boot");
    fx::cfg = fx::Settings();
    vfd::setSlowScan(0);
    fx::setText(fx::M_TEXT, DEFAULT_TEXT);
  }
  else if (cmd == "seq")    {
    String sub = takeToken(rest); sub.toLowerCase();
    if (sub == "add") {
      long ms = takeToken(rest).toInt();
      if (rest.length() == 0) Serial.println("usage: /seq add <ms> <command or text>");
      else { seq.push_back({(uint32_t)(ms < 0 ? 0 : ms), rest}); saveSeq(); }
    }
    else if (sub == "clear") { seqStop(true); seq.clear(); prefs.remove("seq"); prefs.putUChar("seqloop", 0); }
    else if (sub == "play" || sub == "loop") {
      if (seq.empty()) Serial.println("Sequence is empty");
      else { prefs.putUChar("seqloop", sub == "loop"); seqStart(sub == "loop"); }
    }
    else if (sub == "stop") { prefs.putUChar("seqloop", 0); seqStop(true); }
    else if (sub == "list") {
      for (size_t i = 0; i < seq.size(); i++) Serial.println(String(i) + ": " + seq[i].ms + " ms | " + seq[i].cmds);
    }
    else Serial.println("usage: /seq add|play|loop|stop|list|clear");
  }
  else if (cmd == "help")   { help(); }
  else { Serial.println("Unknown command, try /help"); }
}

// ---------------- Arduino entry points ----------------
void setup() {
  Serial.begin(115200);
  vfd::begin();
  prefs.begin("vfd", false);

  String boot = prefs.getString("boot", "");
  Serial.println("Boot line: " + boot);
  if (boot.length()) handleLine(boot); else fx::setText(fx::M_TEXT, DEFAULT_TEXT);

  loadSeq();
  if (prefs.getUChar("seqloop", 0) && !seq.empty()) seqStart(true);

  bleBegin();
  Serial.println("Ready. Send text, or /help");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    rxLast = millis();
    if (c == '\n' || c == '\r') { if (rxBuf.length()) { handleLine(rxBuf); rxBuf = ""; } }
    else if (rxBuf.length() < 512) rxBuf += c;
  }
  // terminals set to "no line ending": flush after a short pause
  if (rxBuf.length() && millis() - rxLast > 100) { handleLine(rxBuf); rxBuf = ""; }

  CmdMsg m;
  while (xQueueReceive(cmdQueue, &m, 0) == pdTRUE) handleLine(String(m.s));

  seqTick();
  fx::update();
  delay(2);
}
