#if 0
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#include "Types.h"

// ───────────────────────── Draw helpers (uses Types.h flags) ──────
struct Draw {
  static inline uint16_t xyToIndex(uint8_t x, uint8_t y){
    uint8_t tx = x, ty = y;
    if (SWAP_XY){ uint8_t t = tx; tx = ty; ty = t; }
    if (FLIP_X) tx = (W-1) - tx;
    if (FLIP_Y) ty = (H-1) - ty;
    if (!SERPENTINE) return uint16_t(ty) * W + tx;
    bool odd = (ty & 1);
    return odd ? uint16_t(ty)*W + (W-1 - tx) : uint16_t(ty)*W + tx;
  }

  static inline void setXY(Adafruit_NeoPixel& s, uint8_t x, uint8_t y, uint32_t c){
    if (x >= W || y >= H) return;
    s.setPixelColor(xyToIndex(x,y), c);
  }
};

// ───────────────────────── Animation: 10 random diodes ───────────
// Put everything in a struct so Arduino won't auto-prototype it
struct Anim {
  static constexpr uint8_t  LIT_COUNT = 20;
  static constexpr uint32_t STEP_MS   = 250;

  static inline uint32_t randomBrightColor(Adafruit_NeoPixel &s){
    uint8_t r = (uint8_t)random(40, 256);
    uint8_t g = (uint8_t)random(40, 256);
    uint8_t b = (uint8_t)random(40, 256);

    // Sometimes make it more "punchy"
    if (random(0, 4) == 0) {
      uint8_t pick = (uint8_t)random(0, 3);
      r = (pick == 0) ? (uint8_t)random(180, 256) : (uint8_t)random(0, 60);
      g = (pick == 1) ? (uint8_t)random(180, 256) : (uint8_t)random(0, 60);
      b = (pick == 2) ? (uint8_t)random(180, 256) : (uint8_t)random(0, 60);
    }
    return s.Color(r, g, b);
  }

  static void renderTenRandomPixels(Adafruit_NeoPixel &s){
    s.clear();

    const uint16_t N = (uint16_t)(W * H);
    const uint8_t count = (LIT_COUNT > N) ? (uint8_t)N : LIT_COUNT;

    const uint32_t col = randomBrightColor(s);

    // choose unique indices (fast for 10 out of 100)
    uint16_t chosen[LIT_COUNT];
    uint8_t chosenN = 0;

    while (chosenN < count) {
      uint16_t idx = (uint16_t)random(0, (long)N);

      bool dup = false;
      for (uint8_t i = 0; i < chosenN; ++i) {
        if (chosen[i] == idx) { dup = true; break; }
      }
      if (dup) continue;

      chosen[chosenN++] = idx;
      s.setPixelColor(idx, col);
    }

    s.show();
  }
};

// ───────────────────────── Panel state ───────────────────────────
static bool     panelOn[NUM_PANELS];
static uint32_t nextStepAt[NUM_PANELS];

static void renderAllPanelsNow(){
  for (uint8_t i = 0; i < NUM_PANELS; ++i) {
    Adafruit_NeoPixel* s = kPanels[i].strip;
    if (!s) continue;

    if (panelOn[i]) {
      Anim::renderTenRandomPixels(*s);
    } else {
      s->clear();
      s->show();
    }
  }
}

static void animatePanels(){
  const uint32_t now = millis();
  for (uint8_t i = 0; i < NUM_PANELS; ++i) {
    if (!panelOn[i]) continue;

    if ((int32_t)(now - nextStepAt[i]) >= 0) {
      Adafruit_NeoPixel* s = kPanels[i].strip;
      if (s) Anim::renderTenRandomPixels(*s);
      nextStepAt[i] = now + Anim::STEP_MS;
    }
  }
}

// ───────────────────────── BLE NUS config ────────────────────────
static const char *NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char *NUS_RX_UUID      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static const char *NUS_TX_UUID      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

static BLEServer         *pServer = nullptr;
static BLECharacteristic *pRxChar = nullptr;
static BLECharacteristic *pTxChar = nullptr;
static bool bleClientConnected = false;

static String rxLineBuffer;

static uint32_t pingSeen = 0;
static uint32_t pongSent = 0;

static void nusSend(const String &line){
  if (!pTxChar || !bleClientConnected) return;
  pTxChar->setValue(line.c_str());
  pTxChar->notify();
}

// ───────────────────────── Command handling ──────────────────────
static void handleCommand(const String &lineRaw){
  String line = lineRaw;
  line.trim();
  if (!line.length()) return;

  String upper = line;
  upper.toUpperCase();

  if (upper.startsWith("PING")) {
    pingSeen++;
    int sp = line.indexOf(' ');
    String num = "";
    if (sp >= 0 && sp + 1 < line.length()) {
      num = line.substring(sp + 1);
      num.trim();
    }
    String resp = "PONG";
    if (num.length()) { resp += " "; resp += num; }
    resp += "\n";
    nusSend(resp);
    pongSent++;
    return;
  }

  if (upper == "RESTART PING" || upper == "RESTART_PING" || upper == "RESTARTPING") {
    pingSeen = 0;
    pongSent = 0;
    nusSend("OK RESTART_PING\n");
    return;
  }

  if (upper == "START") { nusSend("OK START\n"); return; }
  if (upper == "STOP")  { nusSend("OK STOP\n");  return; }

  // SET <index> <0|1>
  if (upper.startsWith("SET ")) {
    int s1 = line.indexOf(' ');
    int s2 = (s1 < 0) ? -1 : line.indexOf(' ', s1 + 1);

    if (s1 > 0 && s2 > s1) {
      int idx = line.substring(s1 + 1, s2).toInt();
      int val = line.substring(s2 + 1).toInt();

      if (idx >= 0 && idx < NUM_PANELS && (val == 0 || val == 1)) {
        panelOn[idx] = (val == 1);
        nextStepAt[idx] = millis();      // update immediately
        renderAllPanelsNow();

        String ack = "OK SET ";
        ack += idx; ack += " "; ack += val; ack += "\n";
        nusSend(ack);
      } else {
        nusSend("ERR SET args\n");
      }
    } else {
      nusSend("ERR SET syntax\n");
    }
    return;
  }

  if (upper == "STATS") {
    String s = "STATS pingSeen=";
    s += pingSeen;
    s += " pongSent=";
    s += pongSent;
    s += "\n";
    nusSend(s);
    return;
  }

  nusSend("ERR UNKNOWN\n");
}

// ───────────────────────── BLE callbacks ─────────────────────────
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    bleClientConnected = true;
    nusSend("HELLO\n");
  }
  void onDisconnect(BLEServer*) override {
    bleClientConnected = false;
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    if (pAdvertising) pAdvertising->start();
  }
};

class MyRxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String value = pChar->getValue();
    if (!value.length()) return;

    for (size_t i = 0; i < value.length(); ++i) {
      char c = value[i];
      if (c == '\n' || c == '\r') {
        if (rxLineBuffer.length() > 0) {
          String oneLine = rxLineBuffer;
          rxLineBuffer = "";
          handleCommand(oneLine);
        }
      } else {
        rxLineBuffer += c;
        if (rxLineBuffer.length() > 256) rxLineBuffer = "";
      }
    }
  }
};

// ───────────────────────── Setup & loop ──────────────────────────
void setup(){
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[ESP32] BLE NUS + 10 random diodes per active face");

  randomSeed((uint32_t)esp_random() ^ (uint32_t)micros() ^ (uint32_t)millis());

  // Init panels from Types.h
  for (uint8_t i = 0; i < NUM_PANELS; ++i) {
    panelOn[i] = false;
    nextStepAt[i] = 0;

    Adafruit_NeoPixel* s = kPanels[i].strip;
    if (!s) continue;

    s->begin();
    s->setBrightness(BRIGHTNESS);
    s->clear();
    s->show();
  }

  // BLE init
  BLEDevice::init("ADHD Cube");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(NUS_SERVICE_UUID);

  pRxChar = pService->createCharacteristic(
    NUS_RX_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pRxChar->setCallbacks(new MyRxCallbacks());

  pTxChar = pService->createCharacteristic(
    NUS_TX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxChar->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(NUS_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("[BLE] Advertising started");
}

void loop(){
  animatePanels();
  delay(10);
}
#endif