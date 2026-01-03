#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include <Wire.h>
#include "driver/i2s.h"
#include <math.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#include "Types.h"
//#include "ImuGame.h"

static ImuState classifyFromAccel(float ax, float ay, float az);
static bool isValidUp(float tiltPercent);
static uint8_t tiltToAccuracyLevel(float tiltPercent);

static void drawOutline(Adafruit_NeoPixel &s, uint8_t side, uint32_t c);
static void fillCenter4x4(Adafruit_NeoPixel &s, uint32_t c);
static void renderLevelPattern(Adafruit_NeoPixel &s, uint8_t level, uint32_t cFill, uint32_t cOutline);

static uint32_t colorGray(Adafruit_NeoPixel &s);
static uint32_t colorBlue(Adafruit_NeoPixel &s);
static uint32_t colorBlueGreen(Adafruit_NeoPixel &s);

static uint8_t faceToPanelIndex(FaceId f);
static void renderTargetFacePattern(const ImuState &imu);
static void pickNewFace();

// -----------------------Game states
static bool   gameOn = false;
static FaceId targetFace = FACE_UP_ID;
static uint8_t targetPanel = 0;
static FaceId   lastUpFace = FACE_UNKNOWN_ID;
static uint8_t  lastAccLevel = 255;
static uint8_t  lastTargetPanel = 255;
static bool     lastTargetIsUp = false;


// ───────────────────────── Pins ─────────────────────────
// I2S (your working wiring may require swap)
#define I2S_BCLK 18
#define I2S_LRCK 19
#define I2S_DOUT 23
static constexpr bool I2S_SWAP_BCLK_LRCK = false;

// Amp control pins
#define AMP_SD_PIN    21     // enabled HIGH
#define AMP_GAIN_PIN  25     // output-capable, not strap, not LED pin

// IMU I2C pins
#define SDA_PIN 13
#define SCL_PIN 22

// ───────────────────────── I2S init (install once) ─────────────────────────
static bool i2sInited = false;

static void i2sInitOnce() {
  if (i2sInited) return;
  i2sInited = true;

  // enable amp
  pinMode(AMP_SD_PIN, OUTPUT);
  digitalWrite(AMP_SD_PIN, HIGH);

  pinMode(AMP_GAIN_PIN, OUTPUT);
  digitalWrite(AMP_GAIN_PIN, HIGH); // "louder" on many boards (board-dependent)

  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 128,
    .use_apll = true,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  const int bck = I2S_SWAP_BCLK_LRCK ? I2S_LRCK : I2S_BCLK;
  const int ws  = I2S_SWAP_BCLK_LRCK ? I2S_BCLK : I2S_LRCK;

  i2s_pin_config_t pins = {
    .bck_io_num = bck,
    .ws_io_num  = ws,
    .data_out_num = I2S_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };

  Serial.printf("i2s_driver_install=%d\n", (int)i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL));
  Serial.printf("i2s_set_pin=%d\n", (int)i2s_set_pin(I2S_NUM_0, &pins));
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// ───────────────────────── Beep (uses already-inited I2S) ──────────────────
namespace Beep {
  static void play(float freqHz, uint16_t ms, float gain01 = 0.5f) {
    if (gain01 < 0.0f) gain01 = 0.0f;
    if (gain01 > 1.0f) gain01 = 1.0f;

    i2sInitOnce();

    constexpr int SR = 44100;
    const int totalSamples = (SR * (int)ms) / 1000;
    constexpr int chunk = 256;

    static int16_t buf[chunk * 2]; // L/R interleaved
    float phase = 0.0f;
    const float step = 2.0f * PI * freqHz / (float)SR;
    const int amplitude = (int)(32767.0f * gain01);

    int remaining = totalSamples;
    while (remaining > 0) {
      int n = (remaining > chunk) ? chunk : remaining;

      for (int i = 0; i < n; i++) {
        float env = 1.0f;
        const int fade = 64;
        int idx = totalSamples - remaining + i;
        if (idx < fade) env = (float)idx / fade;
        int tail = totalSamples - 1 - idx;
        if (tail < fade) env = fminf(env, (float)tail / fade);

        int16_t s = (int16_t)(sinf(phase) * (amplitude * env));
        buf[2*i]   = s;
        buf[2*i+1] = s;

        phase += step;
        if (phase >= 2.0f * PI) phase -= 2.0f * PI;
      }

      size_t written = 0;
      i2s_write(I2S_NUM_0, buf, n * sizeof(int16_t) * 2, &written, portMAX_DELAY);
      remaining -= n;
    }
  }
}


// ───────────────────────── Animation: random diodes ───────────
struct Anim {
  static constexpr uint8_t  LIT_COUNT = 20;
  static constexpr uint32_t STEP_MS   = 250;

  static inline uint32_t randomBrightColor(Adafruit_NeoPixel &s){
    uint8_t r = (uint8_t)random(40, 256);
    uint8_t g = (uint8_t)random(40, 256);
    uint8_t b = (uint8_t)random(40, 256);
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

    if (panelOn[i]) Anim::renderTenRandomPixels(*s);
    else { s->clear(); s->show(); }
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

static void allPanelsOffNow() {
  for (uint8_t i = 0; i < NUM_PANELS; ++i) {
    panelOn[i] = false;
    nextStepAt[i] = 0;
    if (auto *s = kPanels[i].strip) {
      s->clear();
      s->show();
    }
  }
}

// ───────────────────────── ADXL345 minimal driver (I2C) ────────────────────
static constexpr uint8_t ADXL_ADDR = 0x53;

// ADXL345 regs
static constexpr uint8_t REG_DEVID      = 0x00;
static constexpr uint8_t REG_POWER_CTL  = 0x2D;
static constexpr uint8_t REG_DATA_FORMAT= 0x31;
static constexpr uint8_t REG_BW_RATE    = 0x2C;
static constexpr uint8_t REG_DATAX0     = 0x32;

static bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission() == 0);
}

static bool i2cRead(uint8_t addr, uint8_t reg, uint8_t *buf, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  size_t got = Wire.requestFrom((int)addr, (int)n);
  if (got != n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)Wire.read();
  return true;
}

static bool adxlInit() {
  uint8_t id = 0;
  if (!i2cRead(ADXL_ADDR, REG_DEVID, &id, 1)) {
    Serial.println("[ADXL] I2C read failed");
    return false;
  }
  Serial.printf("[ADXL] DEVID=0x%02X (expected 0xE5)\n", id);

  // BW_RATE: 0x0A = 100 Hz output data rate
  if (!i2cWrite8(ADXL_ADDR, REG_BW_RATE, 0x0A)) return false;

  // DATA_FORMAT: FULL_RES (bit3) + +/-2g (range=0)
  // FULL_RES makes scale ~4mg/LSB across ranges
  if (!i2cWrite8(ADXL_ADDR, REG_DATA_FORMAT, 0x08)) return false;

  // POWER_CTL: Measure mode (bit3)
  if (!i2cWrite8(ADXL_ADDR, REG_POWER_CTL, 0x08)) return false;

  Serial.println("[ADXL] init OK (100Hz, full-res, measure)");
  return true;
}

static bool adxlReadAccelG(float &ax, float &ay, float &az) {
  uint8_t b[6];
  if (!i2cRead(ADXL_ADDR, REG_DATAX0, b, 6)) return false;

  int16_t rx = (int16_t)((b[1] << 8) | b[0]);
  int16_t ry = (int16_t)((b[3] << 8) | b[2]);
  int16_t rz = (int16_t)((b[5] << 8) | b[4]);

  // In FULL_RES: ~4 mg/LSB => 0.0039 g/LSB
  constexpr float k = 0.0039f;
  ax = rx * k;
  ay = ry * k;
  az = rz * k;

  return true;
}

// Simple low-pass filter (helps stability)
static float lpf(float prev, float x, float a) { return prev + a * (x - prev); }


static void pickNewFace() {
  FaceId prev = targetFace;
  FaceId nf = prev;

  for (int i = 0; i < 10 && nf == prev; ++i) {
    nf = (FaceId)random(0, 6);
  }
  if (nf == prev) nf = (FaceId)((prev + 1) % 6);

  targetFace = nf;
  targetPanel = faceToPanelIndex(targetFace);

  if (targetPanel >= NUM_PANELS) {
    // should never happen, but fail safe
    targetFace = FACE_UNKNOWN_ID;
  }
}


// Render only the target face (others off) while game runs
static void renderTargetFacePattern(const ImuState &imu) {
  if (targetPanel >= NUM_PANELS) {
  // Invalid target → everything off
  return;
}
  const bool validUp = isValidUp(imu.tiltPercent);
  const bool targetIsUp = (imu.upFace == targetFace) && validUp;
  const uint8_t acc = targetIsUp ? tiltToAccuracyLevel(imu.tiltPercent) : 3;

  // If nothing relevant changed, don't touch LEDs
  if (imu.upFace == lastUpFace &&
      acc == lastAccLevel &&
      targetPanel == lastTargetPanel &&
      targetIsUp == lastTargetIsUp) {
    return;
  }

  lastUpFace = imu.upFace;
  lastAccLevel = acc;
  lastTargetPanel = targetPanel;
  lastTargetIsUp = targetIsUp;

  // Turn off all panels, but do it without spamming show() on all of them each time.
  // We'll only show() on panels we actually changed.
  for (uint8_t i = 0; i < NUM_PANELS; ++i) {
    if (auto *sp = kPanels[i].strip) {
      sp->clear();
      if (i != targetPanel) sp->show();  // show off state for non-target
    }
  }

  auto *t = kPanels[targetPanel].strip;
  if (!t) return;

  if (!targetIsUp) {
    uint32_t g = colorGray(*t);
    renderLevelPattern(*t, 3, g, g);
    return;
  }

  if (acc == 0) {
    uint32_t c = colorBlueGreen(*t);
    renderLevelPattern(*t, 0, c, c);
  } else {
    uint32_t b = colorBlue(*t);
    renderLevelPattern(*t, acc, b, b);
  }
}

// ───────────────────────── BLE NUS config ────────────────────────
static const char *NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char *NUS_RX_UUID      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static const char *NUS_TX_UUID      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

static BLEServer         *pServer = nullptr;
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

  if (upper == "BEEP1") { Beep::play(880.0f, 180, 0.5f); nusSend("OK BEEP1\n"); return; }
  if (upper == "BEEP2") { Beep::play(1760.0f, 160, 0.5f); nusSend("OK BEEP2\n"); return; }

  if (upper == "RESTART PING" || upper == "RESTART_PING" || upper == "RESTARTPING") {
    pingSeen = 0; pongSent = 0;
    nusSend("OK RESTART_PING\n");
    return;
  }

  // ─── GAME commands ──────────────────────────────────────────────
  // GAME 1  -> enable game + pick face
  // GAME 0  -> disable game + restore normal panel state
  if (upper.startsWith("GAME ")) {
    int v = line.substring(5).toInt();
    if (v == 0 || v == 1) {
      gameOn = (v == 1);
      if (gameOn) {
        pickNewFace();
        nusSend("OK GAME 1\n");
      } else {
        nusSend("OK GAME 0\n");
        renderAllPanelsNow(); // back to manual/animation view
      }
    } else {
      nusSend("ERR GAME args\n");
    }
    return;
  }

  if (upper == "NEW_FACE") {
    if (!gameOn) { nusSend("ERR NEW_FACE game_off\n"); return; }
    pickNewFace();
    String s = "OK NEW_FACE ";
    s += (int)targetFace;
    s += "\n";
    nusSend(s);
    return;
  }

  if (upper.startsWith("FACE ")) {
    if (!gameOn) { nusSend("ERR FACE game_off\n"); return; }
    int n = line.substring(5).toInt();
    if (n >= 0 && n <= 5) {
      targetFace = (FaceId)n;
      targetPanel = faceToPanelIndex(targetFace);
      String s = "OK FACE ";
      s += n;
      s += "\n";
      nusSend(s);
    } else {
      nusSend("ERR FACE args\n");
    }
    return;
  }

  // Existing controls (still work when game is OFF)
  if (upper == "START") { nusSend("OK START\n"); return; }
  if (upper == "STOP")  {
    gameOn = false;              // stop game too
    allPanelsOffNow();           // hard off
    nusSend("OK STOP\n");
    return;
  }

  if (upper.startsWith("SET ")) {
    if (gameOn) { nusSend("ERR SET game_on\n"); return; } // avoid fighting visuals

    int s1 = line.indexOf(' ');
    int s2 = (s1 < 0) ? -1 : line.indexOf(' ', s1 + 1);

    if (s1 > 0 && s2 > s1) {
      int idx = line.substring(s1 + 1, s2).toInt();
      int val = line.substring(s2 + 1).toInt();

      if (idx >= 0 && idx < NUM_PANELS && (val == 0 || val == 1)) {
        panelOn[idx] = (val == 1);
        nextStepAt[idx] = millis();
        renderAllPanelsNow();

        String ack = "OK SET ";
        ack += idx; ack += " "; ack += val; ack += "\n";
        nusSend(ack);
      } else nusSend("ERR SET args\n");
    } else nusSend("ERR SET syntax\n");
    return;
  }

  if (upper == "STATS") {
    String s = "STATS pingSeen=";
    s += pingSeen;
    s += " pongSent=";
    s += pongSent;
    s += " gameOn=";
    s += (gameOn ? 1 : 0);
    s += " targetFace=";
    s += (int)targetFace;
    s += "\n";
    nusSend(s);
    return;
  }

  nusSend("ERR UNKNOWN\n");
}

// ───────────────────────── BLE callbacks ─────────────────────────
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { bleClientConnected = true; nusSend("HELLO\n"); }
  void onDisconnect(BLEServer*) override {
    bleClientConnected = false;
    if (auto *pAdvertising = BLEDevice::getAdvertising()) pAdvertising->start();
  }
};

class MyRxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String value = pChar->getValue();
    if (!value.length()) return;

    // Normalize line endings
    value.replace("\r", "\n");

    // If the packet has newlines, process each line.
    // If it has no newline at all, treat the whole packet as one command.
    if (value.indexOf('\n') < 0) {
      value.trim();
      if (value.length()) handleCommand(value);
      return;
    }

    int start = 0;
    while (start < (int)value.length()) {
      int end = value.indexOf('\n', start);
      if (end < 0) end = value.length();
      String one = value.substring(start, end);
      one.trim();
      if (one.length()) handleCommand(one);
      start = end + 1;
    }
  }
};

// ───────────────────────── Setup & loop ──────────────────────────
static uint32_t lastImuSendAt = 0;
static uint32_t nextImuAt = 0;

// filtered accel
static float fax = 0, fay = 0, faz = -1;

void setup(){
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[ESP32] BLE NUS + LED panels + Beep + ADXL345 Game");

  randomSeed((uint32_t)esp_random() ^ (uint32_t)micros() ^ (uint32_t)millis());

  // I2C init
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  // ADXL init
  bool ok = adxlInit();
  if (!ok) Serial.println("[ADXL] init FAILED (check wiring/address)");

  // Audio init once (so BEEP is instant)
  i2sInitOnce();

  // Init panels
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

  auto *pRxChar = pService->createCharacteristic(
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
  const uint32_t now = millis();

  if (!gameOn) {
    // your old animation mode
    animatePanels();
    delay(10);
    return;
  }

  // GAME mode: read IMU at ~50Hz
  if ((int32_t)(now - nextImuAt) >= 0) {
    nextImuAt = now + 40; // 50 Hz

    float ax, ay, az;
    if (adxlReadAccelG(ax, ay, az)) {
      // low-pass filter for stability
      fax = lpf(fax, ax, 0.25f);
      fay = lpf(fay, ay, 0.25f);
      faz = lpf(faz, az, 0.25f);

      float axc = fax, ayc = fay, azc = faz;

      // try ONE of these, depending on which face set is "tilted":
      rotateAroundY(axc, azc, -5.0f);   // example: -2 degrees around Y
      rotateAroundX(ayc, azc, 0.0f);
      rotateAroundZ(axc, ayc, 0.0f);

      ImuState imu = classifyFromAccel(axc, ayc, azc);


//      ImuState imu = classifyFromAccel(fax, fay, faz);

      // If >30%, treat as unknown
      if (!isValidUp(imu.tiltPercent)) imu.upFace = FACE_UNKNOWN_ID;

      renderTargetFacePattern(imu);

      // send debug every 150ms
      if ((int32_t)(now - lastImuSendAt) >= 0) {
        lastImuSendAt = now + 150;
        String msg = "IMU upFace=";
        msg += (int)imu.upFace;
        msg += " tilt=";
        msg += String(imu.tiltPercent, 1);
        msg += " target=";
        msg += (int)targetFace;
        msg += "\n";
        nusSend(msg);
      }
    } else {
      // If IMU read fails, don’t spam, but let you see it
      if ((int32_t)(now - lastImuSendAt) >= 0) {
        lastImuSendAt = now + 400;
        nusSend("ERR IMU read\n");
      }
    }
  }

  delay(2);
}
