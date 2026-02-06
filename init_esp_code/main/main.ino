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
ImuState imu;

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
    case FACE_UP:    return { 0, 0, 1 };
    case FACE_DOWN:  return { 0, 0,-1 };
    case FACE_FRONT: return { 1, 0, 0 };
    case FACE_BACK:  return {-1, 0, 0 };
    case FACE_LEFT:  return { 0, 1, 0 };
    case FACE_RIGHT: return { 0,-1, 0 };
    default:         return { 0, 0, 0 };
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
      up    = { 0, 1, 0 };   // +Y = LEFT
      right = { 1, 0, 0 };   // +X = FRONT
      break;

    case FACE_DOWN:
      up    = { 0, -1, 0 };
      right = {-1, 0, 0 };
      break;

    case FACE_FRONT:
      up    = { 0, 0, 1 };   // +Z = UP
      right = { 0,-1, 0 };   // -Y = RIGHT
      break;

    case FACE_BACK:
      up    = { 0, 0, 1 };
      right = { 0, 1, 0 };
      break;

    case FACE_LEFT:
      up    = { 0, 0, 1 };
      right = { 1, 0, 0 };
      break;

    case FACE_RIGHT:
      up    = { 0, 0, 1 };
      right = {-1, 0, 0 };
      break;

    default:
      up = right = { 0, 0, 0 };
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
struct StaticShape {
  bool active = false;
  ShapeId shape;
  ColorId color;
};

StaticShape staticShape[6];



static uint32_t lastTick = 0;
static int32_t countdownMs = 10000;   // start at 10s
static int8_t dir = -1;               // -1 = down, +1 = up
static bool goingDown = true;
static uint32_t lastRestart = 0;
static FaceId countdownOwnerFace = FACE_UNKNOWN;

struct PendingBleRoundStart {
  bool active = false;
  FaceId face = FACE_UNKNOWN;
};

static PendingBleRoundStart pendingRoundStart;

struct PendingBleTx {
  bool active = false;
  String msg;
};

// ================= DISPLAY INTENT QUEUE =================

enum PendingDisplayAction {
  DISP_NONE,
  DISP_CLEAR_ALL,
  DISP_CLEAR_FACE,
  DISP_DRAW_SHAPE,
  DISP_DRAW_ARROW
};

struct PendingDisplay {
  PendingDisplayAction action = DISP_NONE;
  FaceId face = FACE_UNKNOWN;
  ShapeId shape = SHAPE_COUNT;
  ColorId color = COLOR_COUNT;
  FaceId from = FACE_UNKNOWN;
  FaceId to = FACE_UNKNOWN;
};

static PendingDisplay pendingDisplay;

static PendingBleTx bleTx;

enum PendingCountdownAction {
  CD_NONE,
  CD_START,
  CD_STOP
};

struct PendingCountdown {
  PendingCountdownAction action = CD_NONE;
  uint32_t durationMs = 0;
};

static PendingCountdown pendingCountdown;


// --------------------------------------- Command handler ---------------------------------------
void handleCommand(const String &raw) {
  String line = raw;
  line.trim();
  if (!line.length()) return;

  String upper = line;
  upper.toUpperCase();

  // ================= GAME START =================
  if (upper.startsWith("GAME START")) {
    inGame = true;
    inRound = false;
    roundBalancing = false;
    currentTargetFace = FACE_UNKNOWN;

    bleTx.active = true;
    bleTx.msg =
      String("OK GAME START face=")
      + parseFace(imu.upFace)
      + "\n";
    return;
  }

  // ================= ROUND START =================
  if (upper.startsWith("ROUND START")) {

    // -------- PAUSE --------
    if (upper.indexOf("TYPE=PAUSE") >= 0) {

      pauseDurationMs = 5000;
      String durStr = kvGet(upper, "DURATION");
      if (durStr.length()) {
        pauseDurationMs = durStr.toInt();
      }

      pauseActive = true;
      pauseFace = FACE_UNKNOWN;
      pauseStartMs = 0;
      pauseWaitingForClear = false;

      inRound = true;
      roundBalancing = false;
      currentTargetFace = FACE_UNKNOWN;

      pendingCountdown.action = CD_STOP;
      pendingDisplay.action = DISP_CLEAR_ALL;

      bleTx.active = true;
      bleTx.msg = "OK ROUND START\n";
      return;
    }


    // -------- ARROW / SIMON --------
    if (upper.indexOf("TYPE=ARROW") >= 0 ||
        upper.indexOf("TYPE=SIMON") >= 0) {

      String fromStr = kvGet(upper, "FROM");
      String toStr   = kvGet(upper, "TO");
      String durStr  = kvGet(upper, "DURATION");

      if (!durStr.length()) {
        bleTx.active = true;
        bleTx.msg = "ERR MISSING DURATION\n";
        return;
      }

      roundCfg.durationMs = durStr.toInt();

      FaceId from = parseFace(fromStr);
      FaceId to   = parseFace(toStr);

      if ((fromStr.length() && from == FACE_UNKNOWN) ||
          (toStr.length()   && to   == FACE_UNKNOWN)) {
        bleTx.active = true;
        bleTx.msg = "ERR BAD FACE\n";
        return;
      }

      String expectedStr = kvGet(upper, "EXPECTED");
      if (expectedStr.length()) {
        currentTargetFace = parseFace(expectedStr);
      } else if (to != FACE_UNKNOWN) {
        currentTargetFace = to;
      } else {
        currentTargetFace = FACE_UNKNOWN;
      }

      inRound = true;
      roundBalancing = true;
      pauseActive = false;
      roundBalanceStartMs = millis();

      pendingRoundStart.active = true;
      pendingRoundStart.face =
        (from != FACE_UNKNOWN) ? from : imu.upFace;

      pendingCountdown.action = CD_START;
      pendingCountdown.durationMs = roundCfg.durationMs;

      bleTx.active = true;
      bleTx.msg = "OK ROUND START\n";
      return;
    }

    bleTx.active = true;
    bleTx.msg = "ERR UNKNOWN ROUND TYPE\n";
    return;
  }

  if (upper.startsWith("BEEP")) {

    uint16_t freq, dur;

    if (!parseKeyValueInt(upper, "FREQ", freq) ||
        !parseKeyValueInt(upper, "DUR", dur)) {

      bleTx.active = true;
      bleTx.msg = "ERR BAD_BEEP_ARGS\n";
      return;
    }

    audio_playBeep(freq, dur);

    bleTx.active = true;
    bleTx.msg = "OK BEEP\n";
    return;
  }

  // ================= GAME END =================
  if (upper == "GAME END" || upper == "GAME 0") {
    resetGameState();

    pendingCountdown.action = CD_STOP;
    pendingDisplay.action = DISP_CLEAR_ALL;

    bleTx.active = true;
    bleTx.msg = "OK GAME END\n";
    return;
  }

  if (upper == "GAME 1") {
    resetGameState();
    inGame = true;

    bleTx.active = true;
    bleTx.msg = "OK GAME START\n";
    return;
  }

  // ================= CLEAR ALL =================
  if (upper.startsWith("CLEAR ALL")) {
    pendingDisplay.action = DISP_CLEAR_ALL;

    if (pauseActive && pauseWaitingForClear) {
      pauseWaitingForClear = false;
      pauseStartMs = millis();

      pendingCountdown.action = CD_START;
      pendingCountdown.durationMs = pauseDurationMs;
    }

    bleTx.active = true;
    bleTx.msg = "OK CLEAR ALL\n";
    return;
  }

  // ================= CLEAR FACE =================
  if (upper.startsWith("CLEAR FACE")) {
    String faceStr = upper.substring(10);
    faceStr.trim();

    FaceId face = parseFace(faceStr);
    if (face == FACE_UNKNOWN) {
      bleTx.active = true;
      bleTx.msg = "ERR UNKNOWN_FACE\n";
      return;
    }

    pendingDisplay.action = DISP_CLEAR_FACE;
    pendingDisplay.face = face;

    bleTx.active = true;
    bleTx.msg = "OK CLEAR FACE\n";
    return;
  }

  // ================= DRAW SHAPE =================
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
      bleTx.active = true;
      bleTx.msg = "ERR BAD_DRAW_FORMAT\n";
      return;
    }

    FaceId face   = parseFace(tokens[2]);
    ShapeId shape = parseShape(tokens[3]);
    ColorId color = parseColor(tokens[4]);

    if (face >= FACE_COUNT || shape >= SHAPE_COUNT || color >= COLOR_COUNT) {
      bleTx.active = true;
      bleTx.msg = "ERR BAD_DRAW_ARGS\n";
      return;
    }

    pendingDisplay.action = DISP_DRAW_SHAPE;
    pendingDisplay.face = face;
    pendingDisplay.shape = shape;
    pendingDisplay.color = color;
    staticShape[face].active = true;
    staticShape[face].shape  = shape;
    staticShape[face].color  = color;

    bleTx.active = true;
    bleTx.msg = "OK DRAW SHAPE\n";
    return;
  }

  // ================= DRAW ARROW =================
  if (upper.startsWith("DRAW ")) {

    String tokens[6];
    uint8_t count = 0;

    int start = 0;
    for (int i = 0; i <= upper.length(); i++) {
      if (i == upper.length() || upper[i] == ' ') {
        if (count < 6) tokens[count++] = upper.substring(start, i);
        start = i + 1;
      }
    }

    if (count != 6 || tokens[1] != "ARROW" || tokens[2] != "ON" || tokens[4] != "TOWARDS") {
      bleTx.active = true;
      bleTx.msg = "ERR BAD_FORMAT\n";
      return;
    }

    FaceId from = parseFace(tokens[3]);
    FaceId to   = parseFace(tokens[5]);

    if (!areFacesAdjacent(from, to)) {
      bleTx.active = true;
      bleTx.msg = "ERR UNREACHABLE\n";
      return;
    }

    pendingDisplay.action = DISP_DRAW_ARROW;
    pendingDisplay.from = from;
    pendingDisplay.to = to;

    bleTx.active = true;
    bleTx.msg = "OK DRAW ARROW\n";
    return;
  }

  // ================= UNKNOWN =================
  bleTx.active = true;
  bleTx.msg = "ERR UNKNOWN_CMD\n";
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
  setFaceRotation(FACE_UP, 1);
  setFaceRotation(FACE_DOWN, -1);
  setFaceRotation(FACE_LEFT, 0);
  setFaceRotation(FACE_RIGHT, -3);
  setFaceRotation(FACE_FRONT, 0);
  setFaceRotation(FACE_BACK, 2);


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

//    mapToDisplay(FACE_UP, SHAPE_CIRCLE_6X6, COLOR_BLUE, DISPLAY_STATIC);
//    mapToDisplay(FACE_DOWN, SHAPE_CIRCLE_6X6, COLOR_BLUE, DISPLAY_STATIC);
//    mapToDisplay(FACE_LEFT, SHAPE_CIRCLE_6X6, COLOR_BLUE, DISPLAY_STATIC);
//    mapToDisplay(FACE_RIGHT, SHAPE_CIRCLE_6X6, COLOR_BLUE, DISPLAY_STATIC);
//    mapToDisplay(FACE_FRONT, SHAPE_CIRCLE_6X6, COLOR_BLUE, DISPLAY_STATIC);
//    mapToDisplay(FACE_BACK, SHAPE_CIRCLE_6X6, COLOR_BLUE, DISPLAY_STATIC);
//
FaceId testFace = FACE_FRONT;   // pick ONE face
startCountdown(10000);
    lastRestart = millis();

}



bool countdownMayFollowUpFace() {
  if (pauseActive) return false;
  if (inRound && !roundBalancing) return false; // locked play phase
  return true;
}
void updateCountdownOwner(const ImuState& imu) {
  if (!isCountdownActive()) return;

  if (countdownMayFollowUpFace()) {
    if (isValidUpFace()) {
      //  change owner only here
      countdownOwnerFace = imu.upFace;
    }
  }
}

// Called from BLE RX when frontend says "ROUND START"
void onBleRoundStart(FaceId requestedFace) {
  pendingRoundStart.active = true;
  pendingRoundStart.face = requestedFace;
}

void loop() {
  // ================= IMU =================
  updateImu();
  ImuState imu = getImuState();
  uint32_t now = millis();

  // ================= DEBUG =================
  if (now - lastDebugPrintMs >= 5000) {
    lastDebugPrintMs = now;

    Serial.print("[DBG] inGame=");
    Serial.print(inGame);
    Serial.print(" inRound=");
    Serial.print(inRound);
    Serial.print(" balancing=");
    Serial.print(roundBalancing);
    Serial.print(" pause=");
    Serial.println(pauseActive);

    Serial.print("[IMU] upFace=");
    Serial.print(parseFace(imu.upFace));
    Serial.print(" tilt=");
    Serial.println(imu.tiltPercent, 1);
  }

  // ======================================================
  // ========== APPLY PENDING BLE ROUND START ==============
  // ======================================================
  if (pendingRoundStart.active) {

    inRound = true;
    pauseActive = false;
    roundBalancing = true;

    roundLockedFace = pendingRoundStart.face;
    startFace = pendingRoundStart.face;

    hasLeftStartFace = false;
    roundBalanceStartMs = now;
    roundStartMs = now;

    countdownOwnerFace = roundLockedFace;

    pendingCountdown.action = CD_START;
    pendingCountdown.durationMs = roundCfg.durationMs;

    bleTx.active = true;
    bleTx.msg =
      String("ROUND BALANCE side=")
      + parseFace(roundLockedFace)
      + "\n";

    pendingRoundStart.active = false;
  }

  // ======================================================
  // ================== PAUSE LOGIC =======================
  // ======================================================
  if (pauseActive) {

    // ---- pause balancing ----
    if (pauseStartMs == 0 && !pauseWaitingForClear) {

      if (imu.upFace != FACE_UNKNOWN && isFaceLocked()) {
        pauseFace = imu.upFace;
        pauseWaitingForClear = true;

        countdownOwnerFace = pauseFace;

        bleTx.active = true;
        bleTx.msg =
          String("ROUND BALANCE side=")
          + parseFace(pauseFace)
          + "\n";
      }

      goto render_tail;
    }

    // ---- pause moved -> FAIL ----
    if (imu.upFace != FACE_UNKNOWN &&
        pauseStartMs > 0 &&
        imu.upFace != pauseFace) {

      pendingCountdown.action = CD_STOP;

      pauseActive = false;
      inRound = false;
      roundBalancing = false;
      currentTargetFace = FACE_UNKNOWN;

      bleTx.active = true;
      bleTx.msg =
        String("END ROUND result=FAIL face=")
        + parseFace(imu.upFace)
        + " reason=PAUSE_MOVE\n";

      goto render_tail;
    }

    // ---- pause success ----
    if (pauseFace != FACE_UNKNOWN &&
        pauseStartMs > 0 &&
        now - pauseStartMs >= pauseDurationMs) {

      pendingCountdown.action = CD_STOP;

      pauseActive = false;
      inRound = false;
      roundBalancing = false;
      currentTargetFace = FACE_UNKNOWN;

      bleTx.active = true;
      bleTx.msg =
        String("END ROUND result=SUCCESS face=")
        + parseFace(pauseFace)
        + "\n";

      goto render_tail;
    }
  }

  // ======================================================
  // ================= BALANCING ==========================
  // ======================================================
  if (inRound && roundBalancing) {

    // timeout waiting for lock
    if (roundBalanceStartMs > 0 &&
        now - roundBalanceStartMs > 30000 &&
        imu.upFace != FACE_UNKNOWN) {

      pendingCountdown.action = CD_STOP;

      inRound = false;
      roundBalancing = false;
      currentTargetFace = FACE_UNKNOWN;

      bleTx.active = true;
      bleTx.msg =
        String("END ROUND result=FAIL face=")
        + parseFace(imu.upFace)
        + " reason=NO_LOCK\n";

      goto render_tail;
    }

    if (isFaceLocked()) {
      roundLockedFace = imu.upFace;
      startFace = imu.upFace;
      hasLeftStartFace = false;

      roundBalancing = false;
      roundStartMs = now;

      countdownOwnerFace = roundLockedFace;

      pendingCountdown.action = CD_START;
      pendingCountdown.durationMs = roundCfg.durationMs;

      bleTx.active = true;
      bleTx.msg =
        String("ROUND BALANCE side=")
        + parseFace(roundLockedFace)
        + "\n";
    }

    goto render_tail;
  }

  // ======================================================
  // ================= PLAY PHASE =========================
  // ======================================================
  if (inRound && !roundBalancing && currentTargetFace != FACE_UNKNOWN) {

    // timeout
    if (now - roundStartMs > roundCfg.durationMs) {

      pendingCountdown.action = CD_STOP;

      inRound = false;
      currentTargetFace = FACE_UNKNOWN;

      bleTx.active = true;
      bleTx.msg =
        String("END ROUND result=FAIL face=")
        + parseFace(startFace)
        + " reason=TIMEOUT\n";

      goto render_tail;
    }
  }

  // ======================================================
  // ================= FACE TRACKING ======================
  // ======================================================
  if (!isValidUpFace()) {
    lastUpFace = FACE_UNKNOWN;
    upFaceSince = now;
    goto render_tail;
  }

  if (imu.upFace != lastUpFace) {
    lastUpFace = imu.upFace;
    upFaceSince = now;
  }

  // wrong face
  if (currentTargetFace != FACE_UNKNOWN &&
      hasLeftStartFace &&
      imu.upFace != currentTargetFace &&
      (now - upFaceSince) >= HOLD_TIME_MS) {

    pendingCountdown.action = CD_STOP;

    inRound = false;
    currentTargetFace = FACE_UNKNOWN;

    bleTx.active = true;
    bleTx.msg =
      String("END ROUND result=FAIL face=")
      + parseFace(imu.upFace)
      + " reason=WRONG_FACE\n";

    goto render_tail;
  }

  // success
  if (currentTargetFace != FACE_UNKNOWN &&
      imu.upFace == currentTargetFace &&
      (now - upFaceSince) >= HOLD_TIME_MS &&
      !pauseActive) {

    pendingCountdown.action = CD_STOP;

    inRound = false;
    currentTargetFace = FACE_UNKNOWN;

    bleTx.active = true;
    bleTx.msg =
      String("END ROUND result=SUCCESS face=")
      + parseFace(imu.upFace)
      + "\n";
  }

  // ======================================================
  // ================= COUNTDOWN LIFECYCLE =================
  // ======================================================
  if (pendingCountdown.action != CD_NONE) {
    if (pendingCountdown.action == CD_STOP) {
      stopCountdown();
    }
    if (pendingCountdown.action == CD_START) {
      startCountdown(pendingCountdown.durationMs);
    }
    pendingCountdown.action = CD_NONE;
  }

  // ======================================================
  // ================= DISPLAY LIFECYCLE ==================
  // ======================================================
  if (pendingDisplay.action != DISP_NONE) {

    switch (pendingDisplay.action) {

      case DISP_CLEAR_ALL:
        clearAllFaces();
        break;

      case DISP_CLEAR_FACE:
        clearFace(pendingDisplay.face);
        break;

      case DISP_DRAW_SHAPE:
        clearFace(pendingDisplay.face);
        mapToDisplay(
          pendingDisplay.face,
          pendingDisplay.shape,
          pendingDisplay.color,
          DISPLAY_STATIC
        );
        break;

      case DISP_DRAW_ARROW: {
        ShapeId arrow;
        if (arrowFromTo(pendingDisplay.from, pendingDisplay.to, arrow)) {
          clearAllFaces();
          mapToDisplay(
            pendingDisplay.from,
            arrow,
            COLOR_BLUE,
            DISPLAY_STATIC
          );
          mapToDisplay(
            pendingDisplay.to,
            SHAPE_CIRCLE_6X6,
            COLOR_GREEN,
            DISPLAY_STATIC
          );
        }
        break;
      }

      default:
        break;
    }

    pendingDisplay.action = DISP_NONE;
  }

  // ======================================================
  // ================= RENDER TAIL ========================
  // ======================================================
render_tail:

  updateCountdownOwner(imu);
  updateCountdown(countdownOwnerFace);

  if (countdownOwnerFace != FACE_UNKNOWN) {
    renderFace(countdownOwnerFace);
  }

  if (bleTx.active) {
    nusSend(bleTx.msg.c_str());
    bleTx.active = false;
  }

  delay(20);
}

