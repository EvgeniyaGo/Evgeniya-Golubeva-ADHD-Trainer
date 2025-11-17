#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <math.h>
#include <Types.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>   

// ───────────────────────── 1) Draw helpers (your original) ───────
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
  static void clear(Adafruit_NeoPixel& s){ s.clear(); s.show(); }
  static void square2x2(Adafruit_NeoPixel& s, uint32_t color){
    s.clear();
    uint8_t cx = W/2, cy = H/2;
    for (int dy=-1; dy<=0; ++dy)
      for (int dx=-1; dx<=0; ++dx)
        setXY(s, cx+dx, cy+dy, color);
    s.show();
  }
  static void square4x4(Adafruit_NeoPixel& s, uint32_t color){
    s.clear();
    uint8_t cx = W/2, cy = H/2;
    for (int dy=-2; dy<=1; ++dy)
      for (int dx=-2; dx<=1; ++dx)
        setXY(s, cx+dx, cy+dy, color);
    s.show();
  }
};

// ───────────────────────── 2) ADXL345 (your original) ────────────
namespace ADXL345 {
  constexpr uint8_t REG_DEVID    = 0x00;
  constexpr uint8_t REG_BW_RATE  = 0x2C;
  constexpr uint8_t REG_POWERCTL = 0x2D;
  constexpr uint8_t REG_DATAFMT  = 0x31;
  constexpr uint8_t REG_DATAX0   = 0x32;
  static uint8_t I2C_ADDR = 0x53;

  inline void write8(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg); Wire.write(val);
    Wire.endTransmission();
  }
  inline uint8_t read8(uint8_t reg) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(I2C_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
  }
  inline bool readXYZ_raw(int16_t &x, int16_t &y, int16_t &z) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(REG_DATAX0);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom(I2C_ADDR, (uint8_t)6);
    if (Wire.available() < 6) return false;
    uint8_t b0 = Wire.read(), b1 = Wire.read();
    uint8_t b2 = Wire.read(), b3 = Wire.read();
    uint8_t b4 = Wire.read(), b5 = Wire.read();
    x = (int16_t)((b1 << 8) | b0);
    y = (int16_t)((b3 << 8) | b2);
    z = (int16_t)((b5 << 8) | b4);
    return true;
  }
  inline bool probeAt(uint8_t addr) {
    I2C_ADDR = addr;
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(REG_DEVID);
    Wire.endTransmission(false);
    Wire.requestFrom(I2C_ADDR, (uint8_t)1);
    return Wire.available() && (Wire.read() == 0xE5);
  }
  inline bool begin() {
    if (!probeAt(0x53) && !probeAt(0x1D)) return false;
    write8(REG_BW_RATE,  0x08); // 25 Hz
    write8(REG_DATAFMT,  0x0A); // FULL_RES=1, ±8g
    write8(REG_POWERCTL, 0x08); // MEASURE=1
    delay(10);
    return true;
  }
}

// ───────────────────────── 3) Simple App helpers ─────────────────
namespace App {
  static inline float ema(float prev, float sample, float a) { return prev + a * (sample - prev); }

  static inline Adafruit_NeoPixel* stripForFace(Face f) {
    for (uint8_t i=0; i<NUM_PANELS; ++i) if (kPanels[i].face == f) return kPanels[i].strip;
    return nullptr;
  }
}

// ───────────────────────── 4) Boolean control per panel ──────────
// panelOn[i] == true  → full white on that face
// panelOn[i] == false → off
static bool panelOn[NUM_PANELS];

static void applyPanelStates() {
  for (uint8_t i = 0; i < NUM_PANELS; ++i) {
    Adafruit_NeoPixel* s = kPanels[i].strip;
    if (!s) continue;
    if (panelOn[i]) {
      uint32_t white = s->Color(255,255,255);
      s->fill(white, 0, s->numPixels());
    } else {
      s->clear();
    }
    s->show();
  }
}

// ───────────────────────── 5) BLE NUS config ─────────────────────
// Same UUIDs as in your React app
static const char *NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char *NUS_RX_UUID      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static const char *NUS_TX_UUID      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

static BLEServer        *pServer = nullptr;
static BLECharacteristic *pRxChar = nullptr;
static BLECharacteristic *pTxChar = nullptr;
static bool bleClientConnected = false;

// for assembling text lines
static String rxLineBuffer;

// ping counters (just for debug; app also has its own)
static uint32_t pingSeen  = 0;
static uint32_t pongSent  = 0;

// helper to send a line to app
static void nusSend(const String &line) {
  if (!pTxChar || !bleClientConnected) return;
  pTxChar->setValue(line.c_str());
  pTxChar->notify();
}

// ───────────────────────── 6) Command handling ───────────────────
static void handleCommand(const String &lineRaw) {
  String line = lineRaw;
  line.trim();
  if (!line.length()) return;

  Serial.print("[CMD] ");
  Serial.println(line);

  String upper = line;
  upper.toUpperCase();

  // PING <n>  → PONG <n>
  if (upper.startsWith("PING")) {
    pingSeen++;
    int sp = line.indexOf(' ');
    String num = "";
    if (sp >= 0 && sp + 1 < line.length()) {
      num = line.substring(sp + 1);
      num.trim();
    }
    String resp = "PONG";
    if (num.length()) {
      resp += " ";
      resp += num;
    }
    resp += "\n";
    nusSend(resp);
    pongSent++;
    return;
  }

  // RESTART PING / RESTART_PING / RESTARTPING
  if (upper == "RESTART PING" || upper == "RESTART_PING" || upper == "RESTARTPING") {
    pingSeen = 0;
    pongSent = 0;
    nusSend("OK RESTART_PING\n");
    Serial.println("[PING] counters reset");
    return;
  }

  // START / STOP (used by your app's big button)
  if (upper == "START") {
    Serial.println("[GAME] START (no game now, just ACK)");
    nusSend("OK START\n");
    return;
  }
  if (upper == "STOP") {
    Serial.println("[GAME] STOP (no game now, just ACK)");
    nusSend("OK STOP\n");
    return;
  }

  // SET <index> <0|1>  : faces controlled by index
  // Example: SET 0 1 → face 0 ON, SET 3 0 → face 3 OFF
  if (upper.startsWith("SET ")) {
    int s1 = line.indexOf(' ');
    int s2 = (s1 < 0) ? -1 : line.indexOf(' ', s1 + 1);
    if (s1 > 0 && s2 > s1) {
      int idx = line.substring(s1 + 1, s2).toInt();
      int val = line.substring(s2 + 1).toInt();
      if (idx >= 0 && idx < NUM_PANELS && (val == 0 || val == 1)) {
        panelOn[idx] = (val == 1);
        applyPanelStates();
        String ack = "OK SET ";
        ack += idx;
        ack += " ";
        ack += val;
        ack += "\n";
        nusSend(ack);
        Serial.print("[PANEL] ");
        Serial.print(ack);
      } else {
        nusSend("ERR SET args\n");
      }
    } else {
      nusSend("ERR SET syntax\n");
    }
    return;
  }

  // optional: STATS → report ping counters
  if (upper == "STATS") {
    String s = "STATS pingSeen=";
    s += pingSeen;
    s += " pongSent=";
    s += pongSent;
    s += "\n";
    nusSend(s);
    return;
  }

  // Unknown command
  nusSend("ERR UNKNOWN\n");
}

// ───────────────────────── 7) BLE callbacks ──────────────────────
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override {
    bleClientConnected = true;
    Serial.println("[BLE] Client connected");
    nusSend("HELLO\n");
  }
  void onDisconnect(BLEServer *pServer) override {
    bleClientConnected = false;
    Serial.println("[BLE] Client disconnected");
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    if (pAdvertising) pAdvertising->start();
  }
};

class MyRxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String value = pChar->getValue();   // <-- Arduino String
    if (!value.length()) return;

    // accumulate into rxLineBuffer, split on \n / \r
    for (size_t i = 0; i < value.length(); ++i) {
      char c = value[i];
      if (c == '\n' || c == '\r') {
        if (rxLineBuffer.length() > 0) {
          String line = rxLineBuffer;
          handleCommand(line);          // your function from before
          rxLineBuffer = "";            // clear
        }
      } else {
        rxLineBuffer += c;
        // safety
        if (rxLineBuffer.length() > 256) {
          rxLineBuffer = "";
        }
      }
    }
  }
};

// ───────────────────────── 8) Setup & loop ───────────────────────
void setup(){
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[ESP32] BLE ping test + 6 faces booleans");

  // I2C + ADXL (you still have the sensor, even if we don't use faces now)
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(5);
  if (!ADXL345::begin()) {
    Serial.println("Failed to find ADXL345!");
    while (true) { delay(1000); }
  }

  // Init panels from kPanels (Types.h)
  for (uint8_t i = 0; i < NUM_PANELS; ++i) {
    panelOn[i] = false;
    Adafruit_NeoPixel* s = kPanels[i].strip;
    if (!s) continue;
    s->begin();
    s->setBrightness(BRIGHTNESS);
    s->clear();
    s->show();
  }
  applyPanelStates();

  // BLE init
  BLEDevice::init("ADHD Cube");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(NUS_SERVICE_UUID);

  // RX
  pRxChar = pService->createCharacteristic(
    NUS_RX_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pRxChar->setCallbacks(new MyRxCallbacks());

  // TX
  pTxChar = pService->createCharacteristic(
    NUS_TX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxChar->addDescriptor(new BLE2902());

  pService->start();

  // Advertise
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(NUS_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("[BLE] Advertising started");
}

void loop(){
  // We don't need heavy logic here; BLE is interrupt/CB based.
  // ADXL could be read here if you want orientation logs, but for Faraday-cage tests
  // we mainly care about BLE + panels.
  delay(20);
}
