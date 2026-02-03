#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include "types.h"
#include "display_control.h"
#include "imu_control.h"
#include "audio.h"

#include "soc/rtc_cntl_reg.h"



#define HOLD_TIME_MS 400

static FaceId currentTargetFace = FACE_UNKNOWN;

// hold detection
static FaceId stableFace = FACE_UNKNOWN;
static uint32_t stableSince = 0;

// Hold-time detection
static FaceId lastUpFace = FACE_UNKNOWN;
static uint32_t upFaceSince = 0;

// ---------------- PAUSE round state ----------------
static bool pauseActive = false;
static FaceId pauseFace = FACE_UNKNOWN;
static uint32_t pauseStartMs = 0;
static uint32_t pauseDurationMs = 0;
bool pauseWaitingForClear = false;


// --------------------------------------- BLE UUIDs (NUS) ---------------------------------------
static const char *NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char *NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static const char *NUS_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";


// --------------------------------------- BLE state ---------------------------------------
static BLECharacteristic *txChar = nullptr;
static bool bleConnected = false;
static String rxBuffer;  // <-- IMPORTANT: line buffer

// --------------------------------------- BLE send helper ---------------------------------------
static void nusSend(const String &line) {
  if (!txChar || !bleConnected) return;
  txChar->setValue(line.c_str());
  txChar->notify();
}


// --------------------------------------- Face parsing ---------------------------------------
FaceId parseFace(const String &s) {
  if (s == "TOP") return FACE_UP;
  if (s == "BOTTOM") return FACE_DOWN;
  if (s == "LEFT") return FACE_LEFT;
  if (s == "RIGHT") return FACE_RIGHT;
  if (s == "FRONT") return FACE_FRONT;
  if (s == "BACK") return FACE_BACK;
  return FACE_UNKNOWN;
}

const char *parseFace(FaceId f) {
  switch (f) {
    case FACE_UP: return "TOP";
    case FACE_DOWN: return "BOTTOM";
    case FACE_LEFT: return "LEFT";
    case FACE_RIGHT: return "RIGHT";
    case FACE_FRONT: return "FRONT";
    case FACE_BACK: return "BACK";
    default: return "UNKNOWN";
  }
}


ShapeId parseShape(const String &s) {
  if (s == "SHAPE_ARROW_UP") return SHAPE_ARROW_UP;
  if (s == "SHAPE_ARROW_DOWN") return SHAPE_ARROW_DOWN;
  if (s == "SHAPE_ARROW_LEFT") return SHAPE_ARROW_LEFT;
  if (s == "SHAPE_ARROW_RIGHT") return SHAPE_ARROW_RIGHT;
  if (s == "SHAPE_CIRCLE_6X6") return SHAPE_CIRCLE_6X6;
  return SHAPE_COUNT;
}

ColorId parseColor(const String &s) {
  if (s == "COLOR_BLACK") return COLOR_BLACK;
  if (s == "COLOR_BLUE") return COLOR_BLUE;
  if (s == "COLOR_GREEN") return COLOR_GREEN;
  if (s == "COLOR_YELLOW") return COLOR_YELLOW;
  if (s == "COLOR_RED") return COLOR_RED;
  if (s == "COLOR_PURPLE") return COLOR_PURPLE;
  if (s == "COLOR_CYAN") return COLOR_CYAN;
  if (s == "COLOR_ORANGE") return COLOR_ORANGE;
  if (s == "COLOR_WHITE") return COLOR_WHITE;
  return COLOR_COUNT;
}

// --------------------------------------- Cube adjacency (1 step) ---------------------------------------
Vec3 faceNormal(FaceId f) {
  switch (f) {
    case FACE_UP: return { 0, 0, 1 };
    case FACE_DOWN: return { 0, 0, -1 };
    case FACE_FRONT: return { 0, 1, 0 };
    case FACE_BACK: return { 0, -1, 0 };
    case FACE_RIGHT: return { 1, 0, 0 };
    case FACE_LEFT: return { -1, 0, 0 };
    default: return { 0, 0, 0 };
  }
}

bool areFacesAdjacent(FaceId a, FaceId b) {
  if (a == FACE_UNKNOWN || b == FACE_UNKNOWN) return false;
  if (a == b) return false;

  Vec3 na = faceNormal(a);
  Vec3 nb = faceNormal(b);

  int dot = na.x * nb.x + na.y * nb.y + na.z * nb.z;
  return dot == 0;
}

void faceBasis(FaceId f, Vec3 &up, Vec3 &right) {
  switch (f) {
    case FACE_UP:
      up = { 0, -1, 0 };
      right = { 1, 0, 0 };
      break;

    case FACE_DOWN:
      up = { 0, -1, 0 };
      right = { -1, 0, 0 };
      break;

    case FACE_FRONT:
      up = { 0, 0, 1 };
      right = { 1, 0, 0 };
      break;

    case FACE_BACK:
      up = { 0, 0, 1 };
      right = { -1, 0, 0 };
      break;

    case FACE_RIGHT:
      up = { 0, 0, 1 };
      right = { 0, -1, 0 };
      break;

    case FACE_LEFT:
      up = { 0, 0, 1 };
      right = { 0, 1, 0 };
      break;
  }
}
bool arrowFromTo(FaceId from, FaceId to, ShapeId &arrowOut) {
  if (from == FACE_UNKNOWN || to == FACE_UNKNOWN) return false;

  Vec3 nFrom = faceNormal(from);
  Vec3 nTo = faceNormal(to);

  int dot = nFrom.x * nTo.x + nFrom.y * nTo.y + nFrom.z * nTo.z;
  if (dot != 0) return false;

  Vec3 up, right;
  faceBasis(from, up, right);

  // Project target normal onto source face axes
  int du = nTo.x * up.x + nTo.y * up.y + nTo.z * up.z;
  int dr = nTo.x * right.x + nTo.y * right.y + nTo.z * right.z;

  if (du == 1) {
    arrowOut = SHAPE_ARROW_UP;
    return true;
  }
  if (du == -1) {
    arrowOut = SHAPE_ARROW_DOWN;
    return true;
  }
  if (dr == 1) {
    arrowOut = SHAPE_ARROW_RIGHT;
    return true;
  }
  if (dr == -1) {
    arrowOut = SHAPE_ARROW_LEFT;
    return true;
  }

  return false;
}

// --------------------------------------- Game / Round state ---------------------------------------
static bool inGame = false;
static bool inRound = false;

static void resetGameState() {
  inGame = false;
  inRound = false;
  currentTargetFace = FACE_UNKNOWN;  // wait END ROUND"
  clearAllFaces();
}

struct RoundConfig {
  uint32_t durationMs = 0;
  bool wantLocked = false;
  bool allowSideChange = false;
};
static uint32_t roundStartMs = 0;
static uint32_t lastDebugPrintMs = 0;

static RoundConfig roundCfg;
static bool roundBalancing = false;  // ждём lock
static uint32_t roundBalanceStartMs = 0;
static FaceId roundLockedFace = FACE_UNKNOWN;

static String kvGet(const String &upperLine, const String &key) {
  // ищем "KEY=" в строке
  String pat = key + "=";
  int p = upperLine.indexOf(pat);
  if (p < 0) return "";
  int v0 = p + pat.length();
  int v1 = upperLine.indexOf(' ', v0);
  if (v1 < 0) v1 = upperLine.length();
  return upperLine.substring(v0, v1);
}

static bool kvBool(const String &v) {
  return (v == "1" || v == "TRUE" || v == "YES" || v == "ON");
}

FaceId startFace = FACE_UNKNOWN;
bool hasLeftStartFace = false;

// --------- // --------------------------------------- Command handler ---------------------------------------
void handleCommand(const String &raw) {
  String line = raw;
  line.trim();
  if (!line.length()) return;

  String upper = line;
  upper.toUpperCase();
  // GAME START game=SIMONSAYS
  if (upper.startsWith("GAME START")) {
    resetGameState();
    inGame = true;
    nusSend("OK GAME START\n");
    return;
  }
  // ---------------- PAUSE ROUND ----------------
  if (upper .startsWith("ROUND START")) {

    // Check if this is PAUSE
    if (upper .indexOf("TYPE=PAUSE") >= 0) {

      // Default duration
      pauseDurationMs = 5000;

      int dIdx = upper.indexOf("DURATION=");
      if (dIdx >= 0) {
        int start = dIdx + 9;
        int end = upper.indexOf(' ', start);
        if (end < 0) end = upper.length();
        pauseDurationMs = upper.substring(start, end).toInt();
      }

      pauseActive = true;
      pauseFace = FACE_UNKNOWN;
      pauseStartMs = 0;

      inRound = true;
      roundBalancing = false;
      currentTargetFace = FACE_UNKNOWN;

      clearAllFaces();
      stopCountdown();

      nusSend("OK ROUND START\n");
      return;  // CRITICAL: no fallthrough
    }
    if (upper.indexOf("TYPE=ARROW") >= 0) {

      String fromStr = kvGet(upper, "FROM");
      String toStr = kvGet(upper, "TO");
      String durStr = kvGet(upper, "DURATION");

      if (!fromStr.length() || !toStr.length() || !durStr.length()) {
        nusSend("ERR BAD ARROW PARAMS\n");
        return;
      }

      FaceId from = parseFace(fromStr);
      FaceId to = parseFace(toStr);

      if (from == FACE_UNKNOWN || to == FACE_UNKNOWN) {
        nusSend("ERR BAD FACE\n");
        return;
      }

      currentTargetFace = to;
      roundCfg.durationMs = durStr.toInt();
      inRound = true;
      roundBalancing = true;
      pauseActive = false;
      roundBalanceStartMs = millis();

      nusSend("OK ROUND START\n");
      return;
    }

    nusSend("ERR UNKNOWN ROUND TYPE\n");
    return;
  }


  // GAME END
  if (upper == "GAME END") {
    resetGameState();
    nusSend("OK GAME END\n");
    return;
  }

  // --------------------------------------- Aliases for current UI ---------------------------------------
  // Your UI sends: "GAME 1" / "GAME 0
  if (upper == "GAME 1") {
    resetGameState();
    inGame = true;
    nusSend("OK GAME START\n");
    return;
  }
  if (upper == "GAME 0") {
    resetGameState();
    nusSend("OK GAME END\n");
    return;
  }

  if (upper.startsWith("CLEAR ALL")) {
    clearAllFaces();
    nusSend("OK CLEAR ALL\n");

  if (pauseActive && pauseWaitingForClear) {
    pauseWaitingForClear = false;
    pauseStartMs = millis();
    startCountdown(pauseDurationMs);
  }

    return;
  }


  // --------------------------------------- ROUND END (optional) ---------------------------------------
  if (upper.startsWith("ROUND END")) {
    inRound = false;
    roundBalancing = false;
    currentTargetFace = FACE_UNKNOWN;
    nusSend("OK ROUND END\n");
    return;
  }


  // CLEAR FACE TOP
  if (upper.startsWith("CLEAR FACE")) {
    int idx = upper.indexOf(' ', 11);  // after "CLEAR FACE"
    if (idx < 0) {
      nusSend("ERR BAD_FORMAT\n");
      return;
    }

    String faceStr = upper.substring(11);
    faceStr.trim();

    FaceId face = parseFace(faceStr);
    if (face == FACE_UNKNOWN) {
      nusSend("ERR UNKNOWN_FACE\n");
      return;
    }

    clearFace(face);
    nusSend("OK CLEAR FACE\n");
    return;
  }

  if (upper.startsWith("BEEP ")) {
    uint16_t freq = 1000;
    uint16_t dur  = 5000;

    parseKeyValueInt(line, "freq", freq);
    parseKeyValueInt(line, "dur", dur);

    audio_playBeep(freq, dur);
    nusSend("OK BEEP\n");
    return;
  }

  // Expected:
  // DRAW SHAPE FACE_TOP SHAPE_ARROW_LEFT COLOR_BLUE
  if (upper.startsWith("DRAW SHAPE ")) {
    String tokens[5];
    uint8_t count = 0;

    int start = 0;
    for (int i = 0; i <= upper.length(); i++) {
      if (i == upper.length() || upper[i] == ' ') {
        if (count < 5) tokens[count++] = upper.substring(start, i);
        start = i + 1;
      }
    }

    if (count != 5) {
      nusSend("ERR BAD_DRAW_FORMAT\n");
      return;
    }

    FaceId face = parseFace(tokens[2]);
    ShapeId shape = parseShape(tokens[3]);
    ColorId color = parseColor(tokens[4]);

    if (face >= FACE_COUNT) {
      nusSend("ERR UNKNOWN_FACE\n");
      return;
    }
    if (shape >= SHAPE_COUNT) {
      nusSend("ERR UNKNOWN_SHAPE\n");
      return;
    }
    if (color >= COLOR_COUNT) {
      nusSend("ERR UNKNOWN_COLOR\n");
      return;
    }

    clearFace(face);
    mapToDisplay(face, shape, color, DISPLAY_STATIC);
    nusSend("OK DRAW SHAPE\n");

    if (shape == SHAPE_CIRCLE_6X6 && color == COLOR_GREEN) {
      currentTargetFace = face;
//      audio_playEvent(AUDIO_ROUND_START);
    }

    return;
  }

  // Expected:
  // DRAW ARROW ON FACE_TOP TOWARDS FACE_LEFT
  if (upper.startsWith("DRAW ")) {

    String tokens[8];
    uint8_t count = 0;

    int start = 0;
    for (int i = 0; i <= upper.length(); i++) {
      if (i == upper.length() || upper[i] == ' ') {
        if (count < 8) tokens[count++] = upper.substring(start, i);
        start = i + 1;
      }
    }

    if (count != 6 || tokens[1] != "ARROW" || tokens[2] != "ON" || tokens[4] != "TOWARDS") {
      nusSend("ERR BAD_FORMAT\n");
      return;
    }

    FaceId from = parseFace(tokens[3]);
    FaceId to = parseFace(tokens[5]);

    if (from >= FACE_COUNT || to >= FACE_COUNT) {
      nusSend("ERR UNKNOWN_FACE\n");
      return;
    }

    if (!areFacesAdjacent(from, to)) {
      nusSend("ERR UNREACHABLE\n");
      return;
    }

    ShapeId arrow;
    if (!arrowFromTo(from, to, arrow)) {
      nusSend("ERR UNREACHABLE\n");
      return;
    }

    clearAllFaces();
    mapToDisplay(from, arrow, COLOR_BLUE, DISPLAY_STATIC);
    mapToDisplay(to, SHAPE_CIRCLE_6X6, COLOR_GREEN, DISPLAY_STATIC);
    nusSend("OK DRAW ARROW\n");
    return;
  }

  nusSend("ERR UNKNOWN_CMD\n");
}



// --------------------------------------- BLE callbacks ---------------------------------------
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    bleConnected = true;
    nusSend("HELLO\n");  // optional, safe
  }

  void onDisconnect(BLEServer *) override {
    bleConnected = false;
    if (auto *adv = BLEDevice::getAdvertising())
      adv->start();
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    String v = c->getValue();
    if (!v.length()) return;

    // Normalize line endings
    v.replace("\r", "\n");

    // Accumulate
    rxBuffer += v;

    // Process complete lines only
    while (true) {
      int nl = rxBuffer.indexOf('\n');
      if (nl < 0) break;

      String line = rxBuffer.substring(0, nl);
      rxBuffer.remove(0, nl + 1);
      line.trim();

      if (line.length()) {
        handleCommand(line);  // <-- YOUR existing command logic
      }
    }
  }
};



// --------------------------------------- setup / loop ---------------------------------------
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  delay(200);
  // BLE init
  BLEDevice::init("ADHD Cube");

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(NUS_SERVICE_UUID);

  BLECharacteristic *rx = service->createCharacteristic(
    NUS_RX_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->setCallbacks(new RxCallbacks());

  txChar = service->createCharacteristic(
    NUS_TX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY);
  txChar->addDescriptor(new BLE2902());

  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

//  Serial.println("[ESP] Cube display tester");

  delay(800);
  // Init subsystems
  Serial.begin(115200);
  Serial.println("[ESP] Cube booted on battery");
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(200);
  audio_init();
  delay(200);
  initDisplay();
  delay(200);
  initImu();  // ← must match imu_control.h exactly
  delay(200);

  // Face rotation compensation (adjust later if needed)
  setFaceRotation(FACE_UP, 0);
  setFaceRotation(FACE_DOWN, 0);
  setFaceRotation(FACE_LEFT, -1);
  setFaceRotation(FACE_RIGHT, -2);
  setFaceRotation(FACE_FRONT, 0);
  setFaceRotation(FACE_BACK, 0);


  /*
 [0] FACE_UP    → PIN_LED_UP    (26)
 [1] FACE_DOWN  → PIN_LED_DOWN  (12)
 [2] FACE_LEFT  → PIN_LED_LEFT  (33)
 [3] FACE_RIGHT → PIN_LED_RIGHT (32)
 [4] FACE_BACK  → PIN_LED_BACK  (14)
 [5] FACE_FRONT → PIN_LED_FRONT (27)
*/


  clearAllFaces();

  Serial.println("[BLE] Advertising started");
}

void loop() {
  // --- IMU update ---
  updateImu();  // or imu_read(), whichever you already use

  ImuState imu = getImuState();
  uint32_t now = millis();
  const uint32_t ROUND_PLAY_TIMEOUT_MS = roundCfg.durationMs;

  // --------------------------------------- Debug state print ---------------------------------------
  if (now - lastDebugPrintMs >= 5000) {  // once per second
    lastDebugPrintMs = now;

    uint32_t elapsedSec = 0;
    uint32_t totalSec = roundCfg.durationMs / 1000;

    // round is playing ⇔ inRound && !roundBalancing
    if (inRound && !roundBalancing) {
      elapsedSec = (now - roundStartMs) / 1000;
      if (elapsedSec > totalSec) elapsedSec = totalSec;
    }

    String dbg;
    dbg.reserve(80);
    dbg += elapsedSec;
    dbg += "/";
    dbg += totalSec;
    dbg += " sec round have passed; game=";
    dbg += (inGame ? "1" : "0");
    dbg += ";round=";
    dbg += (inRound ? "1" : "0");
    dbg += ";balancing=";
    dbg += (roundBalancing ? "1" : "0");
    dbg += "; \n";

    Serial.print(dbg);
    nusSend(dbg.c_str());

    ImuState imu = getImuState();
    Serial.print("ax=");
    Serial.print(imu.ax);
    Serial.print(" ay=");
    Serial.print(imu.ay);
    Serial.print(" az=");
    Serial.print(imu.az);

    Serial.print("[IMU] upFace=");
    Serial.print(parseFace(imu.upFace));

    Serial.print("  tilt=");
    Serial.print(imu.tiltPercent, 1);

    Serial.print("  valid=");
    Serial.print(isValidUpFace() ? "Y" : "N");

    Serial.print("  locked=");
    Serial.println(isFaceLocked() ? "Y" : "N");
  }
  // --------------------------------------- Countdown overlay ---------------------------------------
  // Countdown is rendered as a 1-pixel border on the current active face.
  // If up face is unknown, countdown stays on the last known face.
  if (isCountdownActive()) {
    FaceId active = isValidUpFace() ? imu.upFace : FACE_UNKNOWN;
    updateCountdown(active);
  }

  // ================== PAUSE ROUND ==================
  if (pauseActive) {

    // ---- BALANCE phase ----
    // ================== PAUSE ROUND ==================
    if (pauseActive) {

      // ---- BALANCE phase ----
      if (pauseStartMs == 0 && !pauseWaitingForClear) {

        if (imu.upFace != FACE_UNKNOWN && isFaceLocked()) {

          pauseFace = imu.upFace;
          pauseWaitingForClear = true;   

          nusSend(
            String("ROUND BALANCE side=") + parseFace(pauseFace) + "\n"
          );
        }

        return;  // block ALL other logic
      }
    }

    // ---- ACTIVE phase ----
    // any rotation = FAIL
    if (pauseFace != FACE_UNKNOWN && pauseStartMs > 0 && imu.upFace != pauseFace) {
      stopCountdown();
      pauseActive = false;
      inRound = false;
      roundBalancing = false;
      currentTargetFace = FACE_UNKNOWN;

      nusSend(
        String("END ROUND result=FAIL face=") + parseFace(pauseFace) + " reason=PAUSE_MOVE\n");
      return;
    }

    // time elapsed = SUCCESS
    if (pauseFace != FACE_UNKNOWN &&
        pauseStartMs > 0 &&
        millis() - pauseStartMs >= pauseDurationMs) {

      stopCountdown();
      pauseActive = false;
      inRound = false;
      roundBalancing = false;
      currentTargetFace = FACE_UNKNOWN;

      nusSend(
        String("END ROUND result=SUCCESS face=") + parseFace(pauseFace) + "\n");
      return;
    }
  }

  // --------------------------------------- Balancing phase ---------------------------------------

if (inRound && roundBalancing) {

  if (roundBalanceStartMs > 0 &&
      now - roundBalanceStartMs > 30000 &&
      imu.upFace != FACE_UNKNOWN) {

    nusSend(
      String("END ROUND result=FAIL face=")
      + parseFace(imu.upFace)
      + " reason=NO_LOCK\n"
    );

    inRound = false;
    roundBalancing = false;
    currentTargetFace = FACE_UNKNOWN;
    stopCountdown();
    return;
  }


    if (isFaceLocked()) {
      FaceId locked = imu.upFace;
      roundLockedFace = locked;

      roundBalancing = false;

      // Round play starts now (after we got a stable locked face)
      roundStartMs = millis();
      startFace = locked;
      hasLeftStartFace = false;

      // Start the time-bound countdown border
      startCountdown(roundCfg.durationMs);
      updateCountdown(locked);  // render immediately on the locked face


      String msg = "ROUND BALANCE side=";
      msg += parseFace(locked);
      msg += "\n";
      nusSend(msg);
    }

    return;
  }
// --------------------------------------- Play phase (ARROW timeout) ---------------------------------------

if (inRound && !roundBalancing && currentTargetFace != FACE_UNKNOWN) {

  // play-phase timeout
  if (now - roundStartMs > roundCfg.durationMs) {

    stopCountdown();
    inRound = false;
    currentTargetFace = FACE_UNKNOWN;
    nusSend(
      String("END ROUND result=FAIL face=")
      + parseFace(startFace)
      + " reason=TIMEOUT\n"
    );
    return;
  }

  // (do NOT return here — let success logic run below)
}

  // Only consider VALID faces
  if (!isValidUpFace()) {
    lastUpFace = FACE_UNKNOWN;
    upFaceSince = now;
    return;
  }

  // Track stability of UP face
  if (imu.upFace != lastUpFace) {
    lastUpFace = imu.upFace;
    upFaceSince = now;
  }

  if (currentTargetFace != FACE_UNKNOWN && hasLeftStartFace && imu.upFace != currentTargetFace && (now - upFaceSince) >= HOLD_TIME_MS) {

    uint32_t elapsed = now - roundStartMs;

    String msg = "END ROUND result=FAIL face=";
    msg += parseFace(imu.upFace);
    msg += " time=";
    msg += elapsed;
    msg += " reason=WRONG_FACE\n";

    nusSend(msg.c_str());
    stopCountdown();

    currentTargetFace = FACE_UNKNOWN;
    return;
  }

  if (currentTargetFace != FACE_UNKNOWN && imu.upFace == currentTargetFace && (now - upFaceSince) >= HOLD_TIME_MS && pauseActive != true) {

    uint32_t elapsed = now - roundStartMs;

    String msg = "END ROUND result=SUCCESS face=";
    msg += parseFace(imu.upFace);
    msg += " time=";
    msg += elapsed;
    msg += "\n";

    nusSend(msg.c_str());
    stopCountdown();

    currentTargetFace = FACE_UNKNOWN;
    return;
  }
}
