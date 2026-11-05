#include <Arduino.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include "config.h"
#include "vfd.h"
#include "effects.h"

static Preferences prefs;
static const char* DEFAULT_TEXT = " - june youtube - ";
static String rxBuf;
static uint32_t rxLast = 0;

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
  fx::Mode m = fx::currentMode();
  const char* name = m == fx::M_SCROLL ? "scroll" : m == fx::M_VSCROLL ? "vscroll" : m == fx::M_RANDOM ? "random" : "text";
  String line = String("/") + name +
      " speed=" + String(fx::cfg.speed) + " hold=" + String(fx::cfg.hold) +
      " dir=" + String(fx::cfg.dir) + " align=" + String(fx::cfg.align) +
      " density=" + String(fx::cfg.density) + " blink=" + String(fx::cfg.blinkMs) +
      " sym=" + String(fx::cfg.sym) + " -- " + fx::currentText();
  saveBoot(line);
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
    "opts: speed= hold= dir= align= density= blink= sym=   (end with -- if text looks like an option)\n"
    "State is saved to flash automatically."));
}

static void handleLine(String line) {
  while (line.endsWith("\n") || line.endsWith("\r")) line.remove(line.length() - 1);
  if (line.length() == 0) return;

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
    prefs.remove("boot");
    fx::cfg = fx::Settings();
    vfd::setSlowScan(0);
    fx::setText(fx::M_TEXT, DEFAULT_TEXT);
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

  fx::update();
  delay(2);
}
