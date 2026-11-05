#include <Arduino.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include "config.h"
#include "vfd.h"
#include "effects.h"

static Preferences prefs;
static String lastModeLine;       // what /save stores
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
  else if (k == "acc")     fx::cfg.acc = v.toInt() != 0;
  else if (k == "dots")    { fx::cfg.dotMerge = v.toInt() != 0; }
  else if (k == "bright")  vfd::setBrightness(constrain(v.toInt(), 0, 255));
  else return false;
  return true;
}

// Leading key=value tokens are options, the rest is the text.
static void parseOpts(String& rest) {
  for (;;) {
    String probe = rest;
    String tok = takeToken(probe);
    if (tok.length() && applyOpt(tok)) rest = probe; else break;
  }
}

static void saveBoot(const String& line) {
  if (prefs.getString("boot", "") != line) prefs.putString("boot", line);
}

static void help() {
  Serial.println(F(
    "Plain text            -> show it (saved, restored at boot)\n"
    "/text [opts] T        static text          /clear\n"
    "/scroll [opts] T      horizontal marquee   (dir=l|r)\n"
    "/vscroll [opts] T     vertical pages, split with |  (dir=u|d, hold=ms)\n"
    "/rundot [opts] [T]    running decimal point (dir=l|r)\n"
    "/random [opts]        random segments (density=0..100)\n"
    "/sweep [opts]         lights every segment one by one (acc=1 keeps them lit)\n"
    "/blink [ms|off]       blink overlay on any mode\n"
    "/slow ms|0            slow-motion of the REAL multiplex order (0 = off)\n"
    "/sym 0..7             17th position (minus/small/big circle bitmask)\n"
    "/raw n n n | off      hold raw output lines high (wiring debug)\n"
    "/bright 0..255  /speed ms  /hold ms  /dir x  /align l|c|r  /dots 0|1\n"
    "/save                 store the last mode line as boot default\n"
    "opts: speed= hold= dir= align= density= blink= acc= dots= bright="));
}

static void handleLine(String line) {
  while (line.endsWith("\n") || line.endsWith("\r")) line.remove(line.length() - 1);
  if (line.length() == 0) return;

  if (line[0] != '/') {                       // plain text, like the old firmware
    fx::setText(fx::M_TEXT, line);
    lastModeLine = line;
    saveBoot(line);
    return;
  }

  String rest = line.substring(1);
  String cmd = takeToken(rest);
  cmd.toLowerCase();

  if (cmd == "text")        { parseOpts(rest); fx::setText(fx::M_TEXT, rest); lastModeLine = line; }
  else if (cmd == "scroll") { fx::cfg.speed = 150; parseOpts(rest); fx::setText(fx::M_SCROLL, rest.length() ? rest : fx::currentText()); lastModeLine = line; }
  else if (cmd == "vscroll"){ fx::cfg.speed = 120; parseOpts(rest); fx::setText(fx::M_VSCROLL, rest.length() ? rest : fx::currentText()); lastModeLine = line; }
  else if (cmd == "rundot") { fx::cfg.speed = 80;  parseOpts(rest); fx::setText(fx::M_RUNDOT, rest.length() ? rest : fx::currentText()); lastModeLine = line; }
  else if (cmd == "random") { fx::cfg.speed = 70;  parseOpts(rest); fx::setMode(fx::M_RANDOM); lastModeLine = line; }
  else if (cmd == "sweep")  { fx::cfg.speed = 250; parseOpts(rest); fx::setMode(fx::M_SWEEP); lastModeLine = line; }
  else if (cmd == "clear")  { fx::setText(fx::M_TEXT, ""); }
  else if (cmd == "blink")  {
    String t = takeToken(rest); t.toLowerCase();
    fx::cfg.blinkMs = (t == "off" || t == "0") ? 0 : (t.length() ? t.toInt() : 500);
  }
  else if (cmd == "slow")   { vfd::setSlowScan(constrain(takeToken(rest).toInt(), 0, 5000)); }
  else if (cmd == "sym")    { fx::cfg.sym = takeToken(rest).toInt() & 7; }
  else if (cmd == "raw")    {
    uint64_t mask = 0;
    String t = takeToken(rest);
    if (t == "off" || t.length() == 0) { vfd::setRaw(0, false); }
    else {
      while (t.length()) { int n = t.toInt(); if (n >= 0 && n < 40) mask |= 1ULL << n; t = takeToken(rest); }
      vfd::setRaw(mask, true);
    }
  }
  else if (cmd == "bright") { uint8_t b = constrain(takeToken(rest).toInt(), 0, 255); vfd::setBrightness(b); prefs.putUChar("bright", b); }
  else if (cmd == "speed")  { fx::cfg.speed = constrain(takeToken(rest).toInt(), 1, 60000); }
  else if (cmd == "hold")   { fx::cfg.hold = constrain(takeToken(rest).toInt(), 0, 60000); }
  else if (cmd == "dir")    { String t = takeToken(rest); if (t.length()) fx::cfg.dir = tolower(t[0]); }
  else if (cmd == "align")  { String t = takeToken(rest); if (t.length()) fx::cfg.align = tolower(t[0]); }
  else if (cmd == "dots")   { fx::cfg.dotMerge = takeToken(rest).toInt() != 0; fx::refresh(); }
  else if (cmd == "save")   { saveBoot(lastModeLine); Serial.println("Saved: " + lastModeLine); }
  else if (cmd == "help")   { help(); }
  else { Serial.println("Unknown command, try /help"); }
}

// ---------------- Arduino entry points ----------------
void setup() {
  Serial.begin(115200);
  vfd::begin();
  prefs.begin("vfd", false);
  vfd::setBrightness(prefs.getUChar("bright", 255));

  String boot = prefs.getString("boot", " - June youtube - ");
  Serial.println("Boot line: " + boot);
  handleLine(boot);

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
