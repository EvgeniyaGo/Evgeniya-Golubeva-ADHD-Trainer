#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include "types.h"
#include "display_control.h"
#include "imu_control.h"

#define HOLD_TIME_MS 400

static FaceId currentTargetFace = FACE_UNKNOWN;

// hold detection
static FaceId stableFace = FACE_UNKNOWN;
static uint32_t stableSince = 0;

// Hold-time detection
static FaceId lastUpFace = FACE_UNKNOWN;
static uint32_t upFaceSince = 0;


// ───────────────────────── BLE UUIDs (NUS) ─────────────────────────
static const char *NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char *NUS_RX_UUID      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static const char *NUS_TX_UUID      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";


// ───────────────────────── BLE state ─────────────────────────
static BLECharacteristic *txChar = nullptr;
static bool bleConnected = false;
static String rxBuffer;   // <-- IMPORTANT: line buffer

// ───────────────────────── BLE send helper ─────────────────────────
static void nusSend(const String &line) {
  if (!txChar || !bleConnected) return;
  txChar->setValue(line.c_str());
  txChar->notify();
}


// ───────────────────────── Face parsing ─────────────────────────
FaceId parseFace(const String &s) {
  if (s == "TOP")    return FACE_UP;
  if (s == "BOTTOM") return FACE_DOWN;
  if (s == "LEFT")   return FACE_LEFT;
  if (s == "RIGHT")  return FACE_RIGHT;
  if (s == "FRONT")  return FACE_FRONT;
  if (s == "BACK")   return FACE_BACK;
  return FACE_UNKNOWN;
}

const char* faceToString(FaceId f) {
  switch (f) {
    case FACE_UP:    return "TOP";
    case FACE_DOWN: return "BOTTOM";
    case FACE_LEFT:   return "LEFT";
    case FACE_RIGHT:  return "RIGHT";
    case FACE_FRONT:  return "FRONT";
    case FACE_BACK:   return "BACK";
    default:          return "UNKNOWN";
  }
}


ShapeId parseShape(const String &s) {
  if (s == "SHAPE_ARROW_UP")    return SHAPE_ARROW_UP;
  if (s == "SHAPE_ARROW_DOWN")  return SHAPE_ARROW_DOWN;
  if (s == "SHAPE_ARROW_LEFT")  return SHAPE_ARROW_LEFT;
  if (s == "SHAPE_ARROW_RIGHT") return SHAPE_ARROW_RIGHT;
  if (s == "SHAPE_CIRCLE_6X6")  return SHAPE_CIRCLE_6X6;
  return SHAPE_COUNT;
}

ColorId parseColor(const String &s) {
  if (s == "COLOR_BLACK")  return COLOR_BLACK;
  if (s == "COLOR_BLUE")   return COLOR_BLUE;
  if (s == "COLOR_GREEN")  return COLOR_GREEN;
  if (s == "COLOR_YELLOW") return COLOR_YELLOW;
  if (s == "COLOR_RED")    return COLOR_RED;
  if (s == "COLOR_PURPLE") return COLOR_PURPLE;
  if (s == "COLOR_CYAN")   return COLOR_CYAN;
  if (s == "COLOR_ORANGE") return COLOR_ORANGE;
  if (s == "COLOR_WHITE")  return COLOR_WHITE;
  return COLOR_COUNT;
}

// ───────────────────────── Cube adjacency (1 step) ─────────────────────────
Vec3 faceNormal(FaceId f) {
  switch (f) {
    case FACE_UP:    return { 0,  0,  1};
    case FACE_DOWN:  return { 0,  0, -1};
    case FACE_FRONT: return { 0,  1,  0};
    case FACE_BACK:  return { 0, -1,  0};
    case FACE_RIGHT: return { 1,  0,  0};
    case FACE_LEFT:  return {-1,  0,  0};
    default:         return { 0,  0,  0};
  }
}
bool areFacesAdjacent(FaceId a, FaceId b) {
  if (a == FACE_UNKNOWN || b == FACE_UNKNOWN) return false;
  if (a == b) return false;

  Vec3 na = faceNormal(a);
  Vec3 nb = faceNormal(b);

  int dot = na.x*nb.x + na.y*nb.y + na.z*nb.z;
  return dot == 0;
}

void faceBasis(FaceId f, Vec3 &up, Vec3 &right) {
  switch (f) {
    case FACE_UP:
      up    = { 0,  -1,  0};
      right = { 1,  0,  0};
      break;

    case FACE_DOWN:
      up    = { 0, -1,  0};
      right = { -1,  0,  0};
      break;

    case FACE_FRONT:
      up    = { 0,  0,  1};
      right = { 1,  0,  0};
      break;

    case FACE_BACK:
      up    = { 0,  0,  1};
      right = {-1,  0,  0};
      break;

    case FACE_RIGHT:
      up    = { 0,  0,  1};
      right = { 0, -1,  0};
      break;

    case FACE_LEFT:
      up    = { 0,  0,  1};
      right = { 0,  1,  0};
      break;
  }
}
bool arrowFromTo(FaceId from, FaceId to, ShapeId &arrowOut) {
  if (from == FACE_UNKNOWN || to == FACE_UNKNOWN) return false;

  Vec3 nFrom = faceNormal(from);
  Vec3 nTo   = faceNormal(to);

  int dot = nFrom.x*nTo.x + nFrom.y*nTo.y + nFrom.z*nTo.z;
  if (dot != 0) return false;

  Vec3 up, right;
  faceBasis(from, up, right);

  // Project target normal onto source face axes
  int du = nTo.x*up.x    + nTo.y*up.y    + nTo.z*up.z;
  int dr = nTo.x*right.x + nTo.y*right.y + nTo.z*right.z;

  if (du == 1)  { arrowOut = SHAPE_ARROW_UP;    return true; }
  if (du == -1) { arrowOut = SHAPE_ARROW_DOWN;  return true; }
  if (dr == 1)  { arrowOut = SHAPE_ARROW_RIGHT; return true; }
  if (dr == -1) { arrowOut = SHAPE_ARROW_LEFT;  return true; }

  return false;
}


// ───────────────────────── Command handler ─────────────────────────
void handleCommand(const String &raw) {
  String line = raw;
  line.trim();
  if (!line.length()) return;

  String upper = line;
  upper.toUpperCase();

  if (upper.startsWith("CLEAR ALL")) {
    clearAllFaces();
    nusSend("OK CLEAR ALL\n");
    return;
  }

  // CLEAR FACE TOP
  if (upper.startsWith("CLEAR FACE")) {
    int idx = upper.indexOf(' ', 11); // after "CLEAR FACE"
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

    FaceId  face  = parseFace(tokens[2]);
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

    if (count != 6 ||
        tokens[1] != "ARROW" ||
        tokens[2] != "ON" ||
        tokens[4] != "TOWARDS") {
      nusSend("ERR BAD_FORMAT\n");
      return;
    }

    FaceId from = parseFace(tokens[3]);
    FaceId to   = parseFace(tokens[5]);

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



// ───────────────────────── BLE callbacks ─────────────────────────
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    bleConnected = true;
    nusSend("HELLO\n");   // optional, safe
  }

  void onDisconnect(BLEServer*) override {
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
        handleCommand(line);   // <-- YOUR existing command logic
      }
    }
  }
};



// ───────────────────────── setup / loop ─────────────────────────
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(200);
  Serial.println("[ESP] Cube display tester");

  // Init subsystems
  initDisplay();
  initImu();   // ← must match imu_control.h exactly

  // Face rotation compensation (adjust later if needed)
  setFaceRotation(FACE_UP,    0);
  setFaceRotation(FACE_DOWN,  0);
  setFaceRotation(FACE_LEFT,  -1);
  setFaceRotation(FACE_RIGHT, -2);
  setFaceRotation(FACE_FRONT, 0);
  setFaceRotation(FACE_BACK,  0);


/*
 [0] FACE_UP    → PIN_LED_UP    (26)
 [1] FACE_DOWN  → PIN_LED_DOWN  (12)
 [2] FACE_LEFT  → PIN_LED_LEFT  (33)
 [3] FACE_RIGHT → PIN_LED_RIGHT (32)
 [4] FACE_BACK  → PIN_LED_BACK  (14)
 [5] FACE_FRONT → PIN_LED_FRONT (27)
*/


  clearAllFaces();

    // BLE init
  BLEDevice::init("ADHD Cube");

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(NUS_SERVICE_UUID);

  BLECharacteristic *rx = service->createCharacteristic(
    NUS_RX_UUID,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_WRITE_NR
  );
  rx->setCallbacks(new RxCallbacks());

  txChar = service->createCharacteristic(
    NUS_TX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  txChar->addDescriptor(new BLE2902());

  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();


  Serial.println("[BLE] Advertising started");
}

void loop() {
  // --- IMU update ---
  updateImu();   // or imu_read(), whichever you already use

  ImuState imu = getImuState();
  uint32_t now = millis();
  static uint32_t lastPrint = 0;

if (now - lastPrint > 300) {  // print ~3x per second
  ImuState imu = getImuState();
  Serial.print("ax=");
  Serial.print(imu.ax);
  Serial.print(" ay=");
  Serial.print(imu.ay);
  Serial.print(" az=");
  Serial.println(imu.az);

  Serial.print("[IMU] upFace=");
  Serial.print(faceToString(imu.upFace));

  Serial.print("  tilt=");
  Serial.print(imu.tiltPercent, 1);

  Serial.print("  valid=");
  Serial.print(isValidUpFace() ? "Y" : "N");

  Serial.print("  locked=");
  Serial.println(isFaceLocked() ? "Y" : "N");

  lastPrint = now;
}

  FaceId upFace = imu.upFace;

  // Only consider VALID faces
  if (!isValidUpFace()) {
    lastUpFace = FACE_UNKNOWN;
    upFaceSince = now;
    return;
  }

  // Track stability of UP face
  if (upFace != lastUpFace) {
    lastUpFace = upFace;
    upFaceSince = now;
  }

  // Check round completion
  if (currentTargetFace != FACE_UNKNOWN &&
      upFace == currentTargetFace &&
      (now - upFaceSince) >= HOLD_TIME_MS) {

    String msg = "END ROUND ";
    msg += faceToString(upFace);  // MUST output TOP / LEFT / etc
    msg += "\n";

    nusSend(msg.c_str());

    // Disarm until next round
    currentTargetFace = FACE_UNKNOWN;
  }

  // --- rest of your loop (BLE, display, etc.) ---
}
