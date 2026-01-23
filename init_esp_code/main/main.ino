#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#include "types.h"
#include "display_control.h"
#include "imu_control.h"

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

  // Expected:
  // DRAW ARROW ON TOP TOWARDS LEFT
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
  nusSend("OK DRAW\n");
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
  // Event-driven tester → nothing here
}
