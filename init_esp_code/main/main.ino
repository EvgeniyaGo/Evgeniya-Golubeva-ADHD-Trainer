#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include <math.h>
#include <string.h>
#include "types.h"
#include "display_control.h"
#include "imu_control.h"
#include "audio.h"

#include "soc/rtc_cntl_reg.h"

#define HOLD_TIME_MS 400

enum GameMode {
  MODE_IDLE,
  MODE_SIMON,
  MODE_SNAKE
};

static GameMode gameMode = MODE_IDLE;

enum SnakeDir {
  SNAKE_UP,
  SNAKE_DOWN,
  SNAKE_LEFT,
  SNAKE_RIGHT
};

struct SnakeHead {
  FaceId face;
  int8_t x;
  int8_t y;
};

struct SnakeApple {
  FaceId face;
  int8_t x;
  int8_t y;
  bool active;
};


static FaceId lastSnakeInputFace = FACE_UNKNOWN;
static const uint8_t MAX_SNAKE_LENGTH = 80;
static uint8_t snakeLength = 1;

static SnakeHead snakeBody[MAX_SNAKE_LENGTH] = {
  { FACE_UP, 5, 5 },
  { FACE_UP, 4, 5 },
  { FACE_UP, 3, 5 },
  { FACE_UP, 2, 5 }
};

static SnakeDir snakeDir = SNAKE_RIGHT;
static uint32_t lastSnakeMoveMs = 0;
static bool snakeGameOver = false;
static bool snakeGrowPending = false;

static const uint8_t MAX_SNAKE_APPLES = 3;
static SnakeApple snakeApples[MAX_SNAKE_APPLES];
static uint16_t snakeScore = 0;

static const uint32_t SNAKE_MOVE_INTERVAL_MS = 400;


// ADXL orientation state
static FaceId currentTargetFace = FACE_UNKNOWN;
ImuState imu;

// Hold-time detection
static FaceId lastUpFace = FACE_UNKNOWN;
static uint32_t upFaceSince = 0;

// Session metrics
static uint32_t simonSessionStartMs = 0;
static uint16_t simonTotalRounds = 0;
static uint16_t simonSuccessRounds = 0;
static uint16_t simonFailedRounds = 0;
static uint16_t simonOmissionErrors = 0;
static uint16_t simonCommissionErrors = 0;
static uint16_t simonCurrentFocusStreak = 0;
static uint16_t simonLongestFocusStreak = 0;
static uint32_t simonRoundReactionStartMs = 0;
static uint32_t simonReactionSumMs = 0;
static uint64_t simonReactionSumSqMs = 0;
static uint16_t simonReactionCount = 0;
static bool simonSessionEnded = true;

static uint32_t snakeSessionStartMs = 0;
static uint32_t snakeLastAppleMs = 0;
static uint32_t snakeAppleIntervalSumMs = 0;
static uint16_t snakeApplesCollected = 0;
static bool snakeSessionEnded = true;

// PAUSE round state
static bool pauseActive = false;
static FaceId pauseFace = FACE_UNKNOWN;
static uint32_t pauseStartMs = 0;
static uint32_t pauseDurationMs = 0;
bool pauseWaitingForClear = false;

// BLE UUIDs (NUS)
static const char *NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char *NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static const char *NUS_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

// BLE state
static BLECharacteristic *txChar = nullptr;
static bool bleConnected = false;
static String rxBuffer;  // <-- IMPORTANT: line buffer

// Game / Round state
static bool inGame = false;
static bool inRound = false;
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
FaceId startFace = FACE_UNKNOWN;
bool hasLeftStartFace = false;
struct StaticShape {
  bool active = false;
  ShapeId shape;
  ColorId color;
};
StaticShape staticShape[6];

// Countdown / timer state
static uint32_t lastRestart = 0;
static FaceId countdownOwnerFace = FACE_UNKNOWN;
bool displayClearedThisRound = false;
struct PendingBleRoundStart {
  bool active = false;
  FaceId face = FACE_UNKNOWN;
};
static PendingBleRoundStart pendingRoundStart;
struct PendingBleTx {
  bool active = false;
  String msg;
};
static PendingBleTx bleTx;

// DISPLAY INTENT QUEUE
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



// BLE send helper
static void nusSend(const String &line) {
  if (!txChar || !bleConnected) return;
  txChar->setValue(line.c_str());
  txChar->notify();
}


// Face parsing
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

// Cube adjacency (1 step)
Vec3 faceNormal(FaceId f) {
  switch (f) {
    case FACE_UP: return { 0, 0, 1 };
    case FACE_DOWN: return { 0, 0, -1 };
    case FACE_FRONT: return { 1, 0, 0 };
    case FACE_BACK: return { -1, 0, 0 };
    case FACE_LEFT: return { 0, 1, 0 };
    case FACE_RIGHT: return { 0, -1, 0 };
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
      up = { 0, 1, 0 };     // +Y = LEFT
      right = { 1, 0, 0 };  // +X = FRONT
      break;

    case FACE_DOWN:
      up = { 0, -1, 0 };
      right = { -1, 0, 0 };
      break;

    case FACE_FRONT:
      up = { 0, 0, 1 };      // +Z = UP
      right = { 0, -1, 0 };  // -Y = RIGHT
      break;

    case FACE_BACK:
      up = { 0, 0, 1 };
      right = { 0, 1, 0 };
      break;

    case FACE_LEFT:
      up = { 0, 0, 1 };
      right = { 1, 0, 0 };
      break;

    case FACE_RIGHT:
      up = { 0, 0, 1 };
      right = { -1, 0, 0 };
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

static void resetGameState() {
  inGame = false;
  inRound = false;
  currentTargetFace = FACE_UNKNOWN;
  clearAllFaces();
}

static void resetSimonSessionStats() {
  simonSessionStartMs = millis();
  simonTotalRounds = 0;
  simonSuccessRounds = 0;
  simonFailedRounds = 0;
  simonOmissionErrors = 0;
  simonCommissionErrors = 0;
  simonCurrentFocusStreak = 0;
  simonLongestFocusStreak = 0;
  simonRoundReactionStartMs = 0;
  simonReactionSumMs = 0;
  simonReactionSumSqMs = 0;
  simonReactionCount = 0;
  simonSessionEnded = false;
}

static void resetSnakeSessionStats() {
  snakeSessionStartMs = millis();
  snakeLastAppleMs = snakeSessionStartMs;
  snakeAppleIntervalSumMs = 0;
  snakeApplesCollected = 0;
  snakeSessionEnded = false;
}

static void recordSimonRoundEnd(bool success, const char *reason) {
  simonTotalRounds++;

  if (success) {
    simonSuccessRounds++;
    simonCurrentFocusStreak++;
    if (simonCurrentFocusStreak > simonLongestFocusStreak) {
      simonLongestFocusStreak = simonCurrentFocusStreak;
    }
    return;
  }

  simonFailedRounds++;
  simonCurrentFocusStreak = 0;

  if (strcmp(reason, "TIMEOUT") == 0 || strcmp(reason, "NO_LOCK") == 0) {
    simonOmissionErrors++;
  } else if (strcmp(reason, "WRONG_FACE") == 0 || strcmp(reason, "PAUSE_MOVE") == 0) {
    simonCommissionErrors++;
  }
}

static void recordSimonReaction(uint32_t acceptedAtMs) {
  if (simonRoundReactionStartMs == 0 || acceptedAtMs < simonRoundReactionStartMs) return;

  uint32_t reactionMs = acceptedAtMs - simonRoundReactionStartMs;
  simonReactionSumMs += reactionMs;
  simonReactionSumSqMs += (uint64_t)reactionMs * reactionMs;
  simonReactionCount++;
  simonRoundReactionStartMs = 0;
}

static void sendSimonSessionEnd() {
  if (simonSessionEnded) return;

  simonSessionEnded = true;

  uint32_t durationMs = millis() - simonSessionStartMs;
  uint16_t accuracyPct =
    simonTotalRounds == 0 ? 0 : (uint16_t)((simonSuccessRounds * 100UL) / simonTotalRounds);
  uint32_t meanReactionMs = 0;
  uint32_t reactionStdMs = 0;

  if (simonReactionCount > 0) {
    double mean = (double)simonReactionSumMs / simonReactionCount;
    double meanSq = (double)simonReactionSumSqMs / simonReactionCount;
    double variance = meanSq - (mean * mean);
    if (variance < 0) variance = 0;

    meanReactionMs = (uint32_t)(mean + 0.5);
    reactionStdMs = (uint32_t)(sqrt(variance) + 0.5);
  }

  // TODO: difficulty needs a true app/firmware setting.
  String msg =
    String("SESSION END type=SIMON durationMs=") + durationMs
    + " difficulty=0"
    + " omissionErrors=" + simonOmissionErrors
    + " commissionErrors=" + simonCommissionErrors
    + " meanReactionMs=" + meanReactionMs
    + " reactionStdMs=" + reactionStdMs
    + " accuracyPct=" + accuracyPct
    + " longestFocusStreak=" + simonLongestFocusStreak
    + " rounds=" + simonTotalRounds
    + "\n";

  nusSend(msg);
}

static void sendSnakeSessionEnd(const char *deathType) {
  if (snakeSessionEnded) return;

  snakeSessionEnded = true;
  gameMode = MODE_IDLE;

  uint32_t durationMs = millis() - snakeSessionStartMs;
  uint32_t avgAppleMs =
    snakeApplesCollected <= 1 ? 0 : snakeAppleIntervalSumMs / (snakeApplesCollected - 1);

  String msg =
    String("SESSION END type=SNAKE durationMs=") + durationMs
    + " speedMs=" + SNAKE_MOVE_INTERVAL_MS
    + " finalScore=" + snakeScore
    + " apples=" + snakeApplesCollected
    + " avgAppleMs=" + avgAppleMs
    + " deathType=" + deathType
    + "\n";

  nusSend(msg);
  clearAllFaces();
}

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

// Command handler
void handleCommand(const String &raw) {
  String line = raw;
  line.trim();
  if (!line.length()) return;

  String upper = line;
  upper.toUpperCase();

  // GAME START
  if (upper.startsWith("GAME START")) {
    if (upper.indexOf("TYPE=SNAKE") >= 0) {
      gameMode = MODE_SNAKE;
      inGame = true;
      inRound = false;
      roundBalancing = false;
      currentTargetFace = FACE_UNKNOWN;
      resetSnakeGame();
      resetSnakeSessionStats();

      bleTx.active = true;
      bleTx.msg = "OK GAME START type=SNAKE\n";
      return;
    }

    gameMode = MODE_SIMON;
    inGame = true;
    inRound = false;
    roundBalancing = false;
    currentTargetFace = FACE_UNKNOWN;
    resetSimonSessionStats();

    bleTx.active = true;
    bleTx.msg =
      String("OK GAME START face=")
      + parseFace(imu.upFace)
      + "\n";
    return;
  }

  // ROUND START
  if (upper.startsWith("ROUND START")) {

    // PAUSE
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
      displayClearedThisRound = false;

      bleTx.active = true;
      bleTx.msg = "OK ROUND START\n";
      return;
    }


    // ARROW / SIMON
    if (upper.indexOf("TYPE=ARROW") >= 0 || upper.indexOf("TYPE=SIMON") >= 0) {

      String fromStr = kvGet(upper, "FROM");
      String toStr = kvGet(upper, "TO");
      String durStr = kvGet(upper, "DURATION");

      if (!durStr.length()) {
        bleTx.active = true;
        bleTx.msg = "ERR MISSING DURATION\n";
        return;
      }

      roundCfg.durationMs = durStr.toInt();

      FaceId from = parseFace(fromStr);
      FaceId to = parseFace(toStr);

      if ((fromStr.length() && from == FACE_UNKNOWN) || (toStr.length() && to == FACE_UNKNOWN)) {
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
      simonRoundReactionStartMs = 0;

      pendingRoundStart.active = true;
      pendingRoundStart.face = FACE_UNKNOWN;
      pendingCountdown.action = CD_START;
      pendingDisplay.action = DISP_CLEAR_ALL;
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

    if (!parseKeyValueInt(upper, "FREQ", freq) || !parseKeyValueInt(upper, "DUR", dur)) {

      bleTx.active = true;
      bleTx.msg = "ERR BAD_BEEP_ARGS\n";
      return;
    }

    audio_playBeep(freq, dur);

    bleTx.active = true;
    bleTx.msg = "OK BEEP\n";
    return;
  }

  // GAME END
  if (upper == "GAME END" || upper == "GAME 0") {
    if (gameMode == MODE_SIMON) {
      sendSimonSessionEnd();
    } else if (gameMode == MODE_SNAKE) {
      sendSnakeSessionEnd("manualStop");
    }

    gameMode = MODE_IDLE;
    resetGameState();

    pendingCountdown.action = CD_STOP;
    pendingDisplay.action = DISP_CLEAR_ALL;

    bleTx.active = true;
    bleTx.msg = "OK GAME END\n";
    return;
  }

  if (upper == "GAME 1") {
    gameMode = MODE_SIMON;
    resetGameState();
    inGame = true;
    resetSimonSessionStats();

    bleTx.active = true;
    bleTx.msg = "OK GAME START\n";
    return;
  }

  // CLEAR ALL
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
    Serial.println("[CLEAR ALL]");

    return;
  }

  // CLEAR FACE
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

  // DRAW SHAPE
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

    FaceId face = parseFace(tokens[2]);
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
    staticShape[face].shape = shape;
    staticShape[face].color = color;

    displayClearedThisRound = false;

    bleTx.active = true;
    bleTx.msg = "OK DRAW SHAPE\n";
    return;
  }

  // DRAW ARROW
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
    FaceId to = parseFace(tokens[5]);

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

  // PING
  if (upper.startsWith("PING ")) {
    String sequence = line.substring(5);
    sequence.trim();

    bleTx.active = true;
    bleTx.msg = String("PONG ") + sequence + "\n";
    return;
  }

  // RESTART PING
  if (upper == "RESTART PING") {
    bleTx.active = true;
    bleTx.msg = "OK RESTART PING\n";
    return;
  }

  // UNKNOWN
  bleTx.active = true;
  bleTx.msg = "ERR UNKNOWN_CMD\n";
}

// BLE callbacks
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
    v.replace("\r", "\n");  // Normalize line endings
    rxBuffer += v;          // Accumulate
    while (true) {          // Process complete lines only
      int nl = rxBuffer.indexOf('\n');
      if (nl < 0) break;

      String line = rxBuffer.substring(0, nl);
      rxBuffer.remove(0, nl + 1);
      line.trim();

      if (line.length()) {
        handleCommand(line);
      }
    }
  }
};



bool isOppositeFace(FaceId a, FaceId b) {
  return
    (a == FACE_UP && b == FACE_DOWN) ||
    (a == FACE_DOWN && b == FACE_UP) ||
    (a == FACE_LEFT && b == FACE_RIGHT) ||
    (a == FACE_RIGHT && b == FACE_LEFT) ||
    (a == FACE_FRONT && b == FACE_BACK) ||
    (a == FACE_BACK && b == FACE_FRONT);
}


Vec3 vecNeg(Vec3 v) {
  return { -v.x, -v.y, -v.z };
}

Vec3 vecScale(Vec3 v, float s) {
  return { v.x * s, v.y * s, v.z * s };
}

Vec3 vecAdd(Vec3 a, Vec3 b) {
  return { a.x + b.x, a.y + b.y, a.z + b.z };
}

float vecDot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

FaceId faceFromNormalVector(Vec3 n) {
  if (round(n.x) == 0 && round(n.y) == 0 && round(n.z) == 1) return FACE_UP;
  if (round(n.x) == 0 && round(n.y) == 0 && round(n.z) == -1) return FACE_DOWN;
  if (round(n.x) == 0 && round(n.y) == 1 && round(n.z) == 0) return FACE_LEFT;
  if (round(n.x) == 0 && round(n.y) == -1 && round(n.z) == 0) return FACE_RIGHT;
  if (round(n.x) == 1 && round(n.y) == 0 && round(n.z) == 0) return FACE_FRONT;
  if (round(n.x) == -1 && round(n.y) == 0 && round(n.z) == 0) return FACE_BACK;

  return FACE_UNKNOWN;
}

Vec3 snakeDirToWorldVector(FaceId face, SnakeDir dir) {
  Vec3 up, right;
  faceBasis(face, up, right);

  switch (dir) {
    case SNAKE_UP:
      return up;

    case SNAKE_DOWN:
      return vecNeg(up);

    case SNAKE_RIGHT:
      return right;

    case SNAKE_LEFT:
      return vecNeg(right);

    default:
      return { 0, 0, 0 };
  }
}

Vec3 snakePixelToWorldPoint(FaceId face, int8_t x, int8_t y) {
  Vec3 normal = faceNormal(face);

  Vec3 up, right;
  faceBasis(face, up, right);

  const float bound = MATRIX_WIDTH - 1;

  float localRight = 2.0f * x - bound;
  float localUp = bound - 2.0f * y;

  Vec3 p = vecScale(normal, bound);
  p = vecAdd(p, vecScale(right, localRight));
  p = vecAdd(p, vecScale(up, localUp));

  return p;
}

void worldPointToSnakePixel(FaceId face, Vec3 p, int8_t &xOut, int8_t &yOut) {
  Vec3 up, right;
  faceBasis(face, up, right);

  const float bound = MATRIX_WIDTH - 1;

  float localRight = vecDot(p, right);
  float localUp = vecDot(p, up);

  int x = round((localRight + bound) / 2.0f);
  int y = round((bound - localUp) / 2.0f);

  if (x < 0) x = 0;
  if (x >= MATRIX_WIDTH) x = MATRIX_WIDTH - 1;

  if (y < 0) y = 0;
  if (y >= MATRIX_HEIGHT) y = MATRIX_HEIGHT - 1;

  xOut = x;
  yOut = y;
}

bool sameSnakePosition(SnakeHead a, SnakeHead b) {
  return a.face == b.face && a.x == b.x && a.y == b.y;
}

bool SnakeHeadHitsBody() {
  for (uint8_t i = 1; i < snakeLength; i++) {
    if (sameSnakePosition(snakeBody[0], snakeBody[i])) {
      return true;
    }
  }

  return false;
}

void growSnakeNextMove() {
  if (snakeLength < MAX_SNAKE_LENGTH) {
    snakeGrowPending = true;
  }
}

bool worldVectorToSnakeDir(FaceId face, Vec3 worldDir, SnakeDir &outDir) {
  Vec3 up, right;
  faceBasis(face, up, right);

  int du = round(worldDir.x * up.x + worldDir.y * up.y + worldDir.z * up.z);
  int dr = round(worldDir.x * right.x + worldDir.y * right.y + worldDir.z * right.z);

  if (du == 1) {
    outDir = SNAKE_UP;
    return true;
  }

  if (du == -1) {
    outDir = SNAKE_DOWN;
    return true;
  }

  if (dr == 1) {
    outDir = SNAKE_RIGHT;
    return true;
  }

  if (dr == -1) {
    outDir = SNAKE_LEFT;
    return true;
  }

  return false;
}

void updateSnakeDirectionFromActiveFace(FaceId activeFace) {
  if (activeFace == FACE_UNKNOWN) return;

  // Do not continuously re-apply the same physical cube orientation.
  // A direction command happens only when the player changes the active face.
  if (activeFace == lastSnakeInputFace) {
    return;
  }

  lastSnakeInputFace = activeFace;

  if (activeFace == snakeBody[0].face || isOppositeFace(activeFace, snakeBody[0].face)) {
    return;
  }

  if (!areFacesAdjacent(activeFace, snakeBody[0].face)) {
    return;
  }

  Vec3 commandWorldDir = faceNormal(activeFace);

  SnakeDir newDir;
if (worldVectorToSnakeDir(snakeBody[0].face, commandWorldDir, newDir)) {
  if (!isOppositeSnakeDir(newDir, snakeDir)) {
    snakeDir = newDir;
  }
}
}

void snakeDieAndReset(const char *deathType) {
  sendSnakeSessionEnd(deathType);
  audio_playEvent(AUDIO_SNAKE_GAME_OVER);
  delay(350);
}

void moveSnakeHeadOneStep() {
  if (snakeGameOver) return;

  SnakeHead oldHead = snakeBody[0];
  SnakeHead newHead = oldHead;

  int8_t nextX = oldHead.x;
  int8_t nextY = oldHead.y;

  switch (snakeDir) {
    case SNAKE_UP:
      nextY--;
      break;

    case SNAKE_DOWN:
      nextY++;
      break;

    case SNAKE_LEFT:
      nextX--;
      break;

    case SNAKE_RIGHT:
      nextX++;
      break;
  }

  if (
    nextX >= 0 && nextX < MATRIX_WIDTH &&
    nextY >= 0 && nextY < MATRIX_HEIGHT
  ) {
    newHead.x = nextX;
    newHead.y = nextY;
  } else {
    FaceId oldFace = oldHead.face;
    Vec3 oldNormal = faceNormal(oldFace);

    Vec3 movementWorldDir = snakeDirToWorldVector(oldFace, snakeDir);

    FaceId newFace = faceFromNormalVector(movementWorldDir);
if (newFace == FACE_UNKNOWN) {
audio_playEvent(AUDIO_SNAKE_GAME_OVER);
snakeDieAndReset("deadZoneCollision");
return;
}

    Vec3 edgePoint = snakePixelToWorldPoint(oldFace, oldHead.x, oldHead.y);

    int8_t wrappedX = 0;
    int8_t wrappedY = 0;
    worldPointToSnakePixel(newFace, edgePoint, wrappedX, wrappedY);

    newHead.face = newFace;
    newHead.x = wrappedX;
    newHead.y = wrappedY;

    Vec3 continuedWorldDir = vecNeg(oldNormal);

    SnakeDir continuedDir;
    if (worldVectorToSnakeDir(newFace, continuedWorldDir, continuedDir)) {
      snakeDir = continuedDir;
    }
  }

  uint8_t newLength = snakeLength;

  if (snakeGrowPending && snakeLength < MAX_SNAKE_LENGTH) {
    newLength++;
    snakeGrowPending = false;
  }

  for (int i = newLength - 1; i > 0; i--) {
    snakeBody[i] = snakeBody[i - 1];
  }

  snakeBody[0] = newHead;
  snakeLength = newLength;

if (SnakeHeadHitsBody()) {
snakeDieAndReset("selfCollision");
return;
}}

bool sameSnakeCell(FaceId faceA, int8_t xA, int8_t yA, FaceId faceB, int8_t xB, int8_t yB) {
  return faceA == faceB && xA == xB && yA == yB;
}

bool positionIsOnSnake(FaceId face, int8_t x, int8_t y) {
  for (uint8_t i = 0; i < snakeLength; i++) {
    if (
      snakeBody[i].face == face &&
      snakeBody[i].x == x &&
      snakeBody[i].y == y
    ) {
      return true;
    }
  }

  return false;
}

bool positionIsOnAppleExcept(FaceId face, int8_t x, int8_t y, uint8_t ignoreIndex) {
  for (uint8_t i = 0; i < MAX_SNAKE_APPLES; i++) {
    if (i == ignoreIndex) continue;
    if (!snakeApples[i].active) continue;

    if (
      snakeApples[i].face == face &&
      snakeApples[i].x == x &&
      snakeApples[i].y == y
    ) {
      return true;
    }
  }

  return false;
}

bool positionIsFreeForApple(FaceId face, int8_t x, int8_t y, uint8_t ignoreIndex) {
  if (positionIsOnSnake(face, x, y)) return false;
  if (positionIsOnAppleExcept(face, x, y, ignoreIndex)) return false;

  return true;
}

void spawnSnakeAppleInSlot(uint8_t index) {
  if (index >= MAX_SNAKE_APPLES) return;

  SnakeApple &apple = snakeApples[index];

  uint16_t attempts = 0;

  do {
    apple.face = (FaceId)random(0, FACE_COUNT);
    apple.x = random(0, MATRIX_WIDTH);
    apple.y = random(0, MATRIX_HEIGHT);
    attempts++;
  } while (
    !positionIsFreeForApple(apple.face, apple.x, apple.y, index) &&
    attempts < 300
  );

  if (attempts >= 300) {
    apple.active = false;
    return;
  }

  apple.active = true;
}

void checkSnakeAppleCollision() {
  for (uint8_t i = 0; i < MAX_SNAKE_APPLES; i++) {
    if (!snakeApples[i].active) continue;

    if (
      snakeBody[0].face == snakeApples[i].face &&
      snakeBody[0].x == snakeApples[i].x &&
      snakeBody[0].y == snakeApples[i].y
    ) {
snakeScore++;
uint32_t now = millis();
if (snakeApplesCollected > 0) {
  snakeAppleIntervalSumMs += now - snakeLastAppleMs;
}
snakeApplesCollected++;
snakeLastAppleMs = now;
growSnakeNextMove();

audio_playEvent(AUDIO_SNAKE_APPLE);

spawnSnakeAppleInSlot(i);

Serial.print("[SNAKE] score=");
Serial.print(snakeScore);
Serial.print(" length=");
Serial.println(snakeLength);
    }
  }
}


void renderSnakeTest() {
  clearAllFacesNoShow();

for (uint8_t i = 0; i < MAX_SNAKE_APPLES; i++) {
  if (!snakeApples[i].active) continue;

  drawPixelOnFace(
    snakeApples[i].face,
    snakeApples[i].x,
    snakeApples[i].y,
    COLOR_RED,
    false
  );
}
  for (uint8_t i = 1; i < snakeLength; i++) {
    drawPixelOnFace(
      snakeBody[i].face,
      snakeBody[i].x,
      snakeBody[i].y,
      COLOR_GREEN,
      false
    );
  }

  drawPixelOnFace(
    snakeBody[0].face,
    snakeBody[0].x,
    snakeBody[0].y,
    snakeGameOver ? COLOR_RED : COLOR_GREEN,
    false
  );
}
const char* snakeDirName(SnakeDir dir) {
  switch (dir) {
    case SNAKE_UP: return "UP";
    case SNAKE_DOWN: return "DOWN";
    case SNAKE_LEFT: return "LEFT";
    case SNAKE_RIGHT: return "RIGHT";
    default: return "?";
  }
}

bool isOppositeSnakeDir(SnakeDir a, SnakeDir b) {
  return
    (a == SNAKE_UP && b == SNAKE_DOWN) ||
    (a == SNAKE_DOWN && b == SNAKE_UP) ||
    (a == SNAKE_LEFT && b == SNAKE_RIGHT) ||
    (a == SNAKE_RIGHT && b == SNAKE_LEFT);
}

void resetSnakeGame() {
  snakeLength = 1;

  snakeBody[0] = { FACE_UP, 5, 5 };

  snakeDir = SNAKE_RIGHT;
  lastSnakeMoveMs = millis();

  // Important: do NOT treat the current cube position as a fresh command right after reset.
  lastSnakeInputFace = imu.upFace;

  snakeGameOver = false;
  snakeGrowPending = false;
  snakeScore = 0;

  for (uint8_t i = 0; i < MAX_SNAKE_APPLES; i++) {
    snakeApples[i].active = false;
  }

  for (uint8_t i = 0; i < MAX_SNAKE_APPLES; i++) {
    spawnSnakeAppleInSlot(i);
  }

  renderSnakeTest();
  flushDisplay();

  Serial.println("[SNAKE] reset");
}



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
  initImu();
  delay(200);

  // Face rotation compensation (adjust later if needed)
  setFaceRotation(FACE_UP, 1);
  setFaceRotation(FACE_DOWN, -1);
  setFaceRotation(FACE_LEFT, 0);
  setFaceRotation(FACE_RIGHT, -3);
  setFaceRotation(FACE_FRONT, 0);
  setFaceRotation(FACE_BACK, 2);

  Serial.println("[BLE] Advertising started");
  clearAllFaces();

  lastRestart = millis();
}

// Called from BLE RX when frontend says "ROUND START"
void onBleRoundStart(FaceId requestedFace) {
  pendingRoundStart.active = true;
  pendingRoundStart.face = requestedFace;
}


void loop() {
  updateImu();
  imu = getImuState();
  uint32_t now = millis();

  if (gameMode == MODE_SNAKE) {
    updateSnakeDirectionFromActiveFace(imu.upFace);

    if (now - lastSnakeMoveMs >= SNAKE_MOVE_INTERVAL_MS) {
      lastSnakeMoveMs = now;

      if (!snakeGameOver) {
        moveSnakeHeadOneStep();
        if (gameMode == MODE_SNAKE) {
          checkSnakeAppleCollision();
        }
      }

      if (gameMode != MODE_SNAKE) {
        return;
      }

      renderSnakeTest();

      Serial.print("[SNAKE] headFace=");
      Serial.print(parseFace(snakeBody[0].face));
      Serial.print(" activeFace=");
      Serial.print(parseFace(imu.upFace));
      Serial.print(" x=");
      Serial.print(snakeBody[0].x);
      Serial.print(" y=");
      Serial.print(snakeBody[0].y);
      Serial.print(" dir=");
      Serial.print(snakeDirName(snakeDir));
      Serial.print(" length=");
      Serial.print(snakeLength);
      Serial.print(" gameOver=");
      Serial.println(snakeGameOver ? "YES" : "NO");
    }
  }

  if (gameMode == MODE_SIMON) {
    bool simonHandled = false;

    if (pendingRoundStart.active && imu.upFace != FACE_UNKNOWN) {
      inRound = true;
      pauseActive = false;
      roundBalancing = true;

      hasLeftStartFace = false;
      roundBalanceStartMs = now;
      roundStartMs = now;

      pendingCountdown.action = CD_START;
      pendingCountdown.durationMs = roundCfg.durationMs;
      pendingRoundStart.active = false;
    }

    if (pauseActive) {
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

        simonHandled = true;
      } else if (imu.upFace != FACE_UNKNOWN && pauseStartMs > 0 && imu.upFace != pauseFace) {
        pendingCountdown.action = CD_STOP;

        pauseActive = false;
        inRound = false;
        roundBalancing = false;
        currentTargetFace = FACE_UNKNOWN;

        pendingDisplay.action = DISP_CLEAR_ALL;

        recordSimonRoundEnd(false, "PAUSE_MOVE");

        bleTx.active = true;
        bleTx.msg =
          String("END ROUND result=FAIL face=")
          + parseFace(imu.upFace)
          + " reason=PAUSE_MOVE\n";

        simonHandled = true;
      } else if (pauseFace != FACE_UNKNOWN && pauseStartMs > 0 && now - pauseStartMs >= pauseDurationMs) {
        pendingCountdown.action = CD_STOP;

        pauseActive = false;
        inRound = false;
        roundBalancing = false;
        currentTargetFace = FACE_UNKNOWN;

        pendingDisplay.action = DISP_CLEAR_ALL;

        recordSimonRoundEnd(true, "");

        bleTx.active = true;
        bleTx.msg =
          String("END ROUND result=SUCCESS face=")
          + parseFace(pauseFace)
          + "\n";

        simonHandled = true;
      }
    }

    if (!simonHandled && inRound && roundBalancing) {
      if (roundBalanceStartMs > 0 && now - roundBalanceStartMs > 30000 && imu.upFace != FACE_UNKNOWN) {
        pendingCountdown.action = CD_STOP;

        inRound = false;
        roundBalancing = false;
        currentTargetFace = FACE_UNKNOWN;

        pendingDisplay.action = DISP_CLEAR_ALL;

        recordSimonRoundEnd(false, "NO_LOCK");

        bleTx.active = true;
        bleTx.msg =
          String("END ROUND result=FAIL face=")
          + parseFace(imu.upFace)
          + " reason=NO_LOCK\n";
      } else if (roundBalancing && isFaceLocked() && imu.upFace != FACE_UNKNOWN) {
        if (pendingRoundStart.face != FACE_UNKNOWN && pendingRoundStart.face == imu.upFace) {
          startFace = pendingRoundStart.face;
        } else {
          startFace = imu.upFace;
        }

        roundLockedFace = startFace;
        roundBalancing = false;
        simonRoundReactionStartMs = now;

        bleTx.active = true;
        bleTx.msg =
          String("ROUND BALANCE side=")
          + parseFace(startFace)
          + "\n";

        pendingRoundStart.active = false;
        pendingRoundStart.face = FACE_UNKNOWN;
      }

      simonHandled = true;
    }

    if (!simonHandled && inRound && !roundBalancing && currentTargetFace != FACE_UNKNOWN) {
      if (now - roundStartMs > roundCfg.durationMs) {
        pendingCountdown.action = CD_STOP;

        inRound = false;
        currentTargetFace = FACE_UNKNOWN;

        pendingDisplay.action = DISP_CLEAR_ALL;

        recordSimonRoundEnd(false, "TIMEOUT");

        bleTx.active = true;
        bleTx.msg =
          String("END ROUND result=FAIL face=")
          + parseFace(startFace)
          + " reason=TIMEOUT\n";

        simonHandled = true;
      }
    }

    if (!simonHandled) {
      if (!isValidUpFace()) {
        lastUpFace = FACE_UNKNOWN;
        upFaceSince = now;
      } else {
        if (imu.upFace != lastUpFace) {
          lastUpFace = imu.upFace;
          upFaceSince = now;
        }

        if (currentTargetFace != FACE_UNKNOWN && hasLeftStartFace && imu.upFace != currentTargetFace && (now - upFaceSince) >= HOLD_TIME_MS) {
          pendingCountdown.action = CD_STOP;

          inRound = false;
          currentTargetFace = FACE_UNKNOWN;

          pendingDisplay.action = DISP_CLEAR_ALL;

          recordSimonRoundEnd(false, "WRONG_FACE");

          bleTx.active = true;
          bleTx.msg =
            String("END ROUND result=FAIL face=")
            + parseFace(imu.upFace)
            + " reason=WRONG_FACE\n";
        } else if (currentTargetFace != FACE_UNKNOWN && imu.upFace == currentTargetFace && (now - upFaceSince) >= HOLD_TIME_MS && !pauseActive) {
          pendingCountdown.action = CD_STOP;

          recordSimonReaction(now);

          inRound = false;
          currentTargetFace = FACE_UNKNOWN;

          pendingDisplay.action = DISP_CLEAR_ALL;

          recordSimonRoundEnd(true, "");

          bleTx.active = true;
          bleTx.msg =
            String("END ROUND result=SUCCESS face=")
            + parseFace(imu.upFace)
            + "\n";
        }
      }
    }
  }

  if (pendingCountdown.action != CD_NONE) {
    if (pendingCountdown.action == CD_STOP) {
      stopCountdown();
    }
    if (pendingCountdown.action == CD_START) {
      startCountdown(pendingCountdown.durationMs, imu.upFace);
    }
    pendingCountdown.action = CD_NONE;
  }

  if (gameMode == MODE_SIMON) {
    updateCountdown(imu.upFace);
  }

  if (pendingDisplay.action != DISP_NONE) {
    switch (pendingDisplay.action) {
      case DISP_CLEAR_ALL:
        clearAllFaces();
        pendingDisplay.action = DISP_NONE;
        displayClearedThisRound = true;
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
          DISPLAY_STATIC);
        break;

      case DISP_DRAW_ARROW:
        {
          ShapeId arrow;
          if (arrowFromTo(pendingDisplay.from, pendingDisplay.to, arrow)) {
            clearAllFaces();
            mapToDisplay(
              pendingDisplay.from,
              arrow,
              COLOR_BLUE,
              DISPLAY_STATIC);
            mapToDisplay(
              pendingDisplay.to,
              SHAPE_CIRCLE_6X6,
              COLOR_GREEN,
              DISPLAY_STATIC);
          }
          break;
        }

      default:
        break;
    }

    pendingDisplay.action = DISP_NONE;
  }

  if (bleTx.active) {
    nusSend(bleTx.msg.c_str());
    bleTx.active = false;
  }

  flushDisplay();
  delay(20);
}
/*
  // DEBUG
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

  // APPLY PENDING BLE ROUND START
  if (pendingRoundStart.active && imu.upFace != FACE_UNKNOWN) {

    inRound = true;
    pauseActive = false;
    roundBalancing = true;

    hasLeftStartFace = false;
    roundBalanceStartMs = now;
    roundStartMs = now;

    pendingCountdown.action = CD_START;
    pendingCountdown.durationMs = roundCfg.durationMs;
    pendingRoundStart.active = false;
  }

  // PAUSE LOGIC
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

    // pause moved -> FAIL
    if (imu.upFace != FACE_UNKNOWN && pauseStartMs > 0 && imu.upFace != pauseFace) {

      pendingCountdown.action = CD_STOP;

      pauseActive = false;
      inRound = false;
      roundBalancing = false;
      currentTargetFace = FACE_UNKNOWN;

      pendingDisplay.action = DISP_CLEAR_ALL;

      bleTx.active = true;
      bleTx.msg =
        String("END ROUND result=FAIL face=")
        + parseFace(imu.upFace)
        + " reason=PAUSE_MOVE\n";

      goto render_tail;
    }

    //  pause success
    if (pauseFace != FACE_UNKNOWN && pauseStartMs > 0 && now - pauseStartMs >= pauseDurationMs) {

      pendingCountdown.action = CD_STOP;

      pauseActive = false;
      inRound = false;
      roundBalancing = false;
      currentTargetFace = FACE_UNKNOWN;

      pendingDisplay.action = DISP_CLEAR_ALL;

      bleTx.active = true;
      bleTx.msg =
        String("END ROUND result=SUCCESS face=")
        + parseFace(pauseFace)
        + "\n";

      goto render_tail;
    }
  }

  // BALANCING
  if (inRound && roundBalancing) {

    // timeout waiting for lock
    if (roundBalanceStartMs > 0 && now - roundBalanceStartMs > 30000 && imu.upFace != FACE_UNKNOWN) {

      pendingCountdown.action = CD_STOP;

      inRound = false;
      roundBalancing = false;
      currentTargetFace = FACE_UNKNOWN;

      pendingDisplay.action = DISP_CLEAR_ALL;

      bleTx.active = true;
      bleTx.msg =
        String("END ROUND result=FAIL face=")
        + parseFace(imu.upFace)
        + " reason=NO_LOCK\n";

      goto render_tail;
    }

    // BALANCE LOCK
    if (roundBalancing && isFaceLocked() && imu.upFace != FACE_UNKNOWN) {

      // Decide final start face
      if (pendingRoundStart.face != FACE_UNKNOWN && pendingRoundStart.face == imu.upFace) {
        startFace = pendingRoundStart.face;
      } else {
        startFace = imu.upFace;
      }

      roundLockedFace = startFace;
      roundBalancing = false;

      bleTx.active = true;
      bleTx.msg =
        String("ROUND BALANCE side=")
        + parseFace(startFace)
        + "\n";

      // cleanup
      pendingRoundStart.active = false;
      pendingRoundStart.face = FACE_UNKNOWN;
    }

    goto render_tail;
  }

  // PLAY PHASE
  if (inRound && !roundBalancing && currentTargetFace != FACE_UNKNOWN) {
    // timeout
    if (now - roundStartMs > roundCfg.durationMs) {

      pendingCountdown.action = CD_STOP;

      inRound = false;
      currentTargetFace = FACE_UNKNOWN;

      pendingDisplay.action = DISP_CLEAR_ALL;

      bleTx.active = true;
      bleTx.msg =
        String("END ROUND result=FAIL face=")
        + parseFace(startFace)
        + " reason=TIMEOUT\n";

      goto render_tail;
    }
  }

  // FACE TRACKING
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
  if (currentTargetFace != FACE_UNKNOWN && hasLeftStartFace && imu.upFace != currentTargetFace && (now - upFaceSince) >= HOLD_TIME_MS) {

    pendingCountdown.action = CD_STOP;

    inRound = false;
    currentTargetFace = FACE_UNKNOWN;

    pendingDisplay.action = DISP_CLEAR_ALL;

    bleTx.active = true;
    bleTx.msg =
      String("END ROUND result=FAIL face=")
      + parseFace(imu.upFace)
      + " reason=WRONG_FACE\n";

    goto render_tail;
  }

  // success
  if (currentTargetFace != FACE_UNKNOWN && imu.upFace == currentTargetFace && (now - upFaceSince) >= HOLD_TIME_MS && !pauseActive) {

    pendingCountdown.action = CD_STOP;

    inRound = false;
    currentTargetFace = FACE_UNKNOWN;

    pendingDisplay.action = DISP_CLEAR_ALL;

    bleTx.active = true;
    bleTx.msg =
      String("END ROUND result=SUCCESS face=")
      + parseFace(imu.upFace)
      + "\n";
  }

  // COUNTDOWN LIFECYCLE
  if (pendingCountdown.action != CD_NONE) {
    if (pendingCountdown.action == CD_STOP) {
      stopCountdown();
    }
    if (pendingCountdown.action == CD_START) {
      startCountdown(pendingCountdown.durationMs, imu.upFace);
    }
    pendingCountdown.action = CD_NONE;
  }

  // DISPLAY LIFECYCLE
  if (pendingDisplay.action != DISP_NONE) {

    switch (pendingDisplay.action) {

      case DISP_CLEAR_ALL:
        clearAllFaces();
        pendingDisplay.action = DISP_NONE;
        displayClearedThisRound = true;
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
          DISPLAY_STATIC);
        break;

      case DISP_DRAW_ARROW:
        {
          ShapeId arrow;
          if (arrowFromTo(pendingDisplay.from, pendingDisplay.to, arrow)) {
            clearAllFaces();
            mapToDisplay(
              pendingDisplay.from,
              arrow,
              COLOR_BLUE,
              DISPLAY_STATIC);
            mapToDisplay(
              pendingDisplay.to,
              SHAPE_CIRCLE_6X6,
              COLOR_GREEN,
              DISPLAY_STATIC);
          }
          break;
        }

      default:
        break;
    }

    pendingDisplay.action = DISP_NONE;
  }

  // RENDER TAIL
render_tail:

  updateCountdown(imu.upFace);

  if (bleTx.active) {
    nusSend(bleTx.msg.c_str());
    bleTx.active = false;
  }

  flushDisplay();

  delay(20);
}
*/
