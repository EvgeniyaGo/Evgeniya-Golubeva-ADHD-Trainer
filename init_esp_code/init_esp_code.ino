#include <NimBLEDevice.h>

// Nordic UART Service (NUS) UUIDs
static const char* SVC_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* RX_UUID  = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"; // phone -> ESP (Write/WriteNR)
static const char* TX_UUID  = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // ESP -> phone (Notify)

NimBLECharacteristic* txChar;
volatile bool deviceConnected = false;

// ---- Server callbacks (support old and new NimBLE signatures) ----
class ServerCallbacks : public NimBLEServerCallbacks {
public:
  // Newer NimBLE
  void onConnect(NimBLEServer* s, NimBLEConnInfo& connInfo) {
    deviceConnected = true;
    // Request a power-friendly interval (36–72 * 1.25ms = 45–90ms)
    s->updateConnParams(connInfo.getConnHandle(), 36, 72, 4, 600);
    Serial.println("Central connected (new sig)");
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& /*connInfo*/) {
    deviceConnected = false;
    Serial.println("Central disconnected (new sig)");
    NimBLEDevice::startAdvertising();
  }
  // Older NimBLE (kept for compatibility)
  void onConnect(NimBLEServer* s) {
    deviceConnected = true;
    Serial.println("Central connected (old sig)");
  }
  void onDisconnect(NimBLEServer* s) {
    deviceConnected = false;
    Serial.println("Central disconnected (old sig)");
    NimBLEDevice::startAdvertising();
  }
};

// ---- Characteristic callbacks (support old and new signatures) ----
class RxCallbacks : public NimBLECharacteristicCallbacks {
public:
  // Newer NimBLE
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& /*connInfo*/) {
    handleWrite(c);
  }
  // Older NimBLE
  void onWrite(NimBLECharacteristic* c) {
    handleWrite(c);
  }
private:
  void handleWrite(NimBLECharacteristic* c) {
    std::string data = c->getValue();
    if (data.empty()) return;
    Serial.print("RX <- ");
    Serial.println(data.c_str());
    // Echo back to phone (optional)
    txChar->setValue((uint8_t*)data.data(), data.size());
    txChar->notify();
  }
};

void setup() {
  Serial.begin(115200);
  delay(200);

  NimBLEDevice::init("Jeni-ESP32");
  // Lower TX power for 2–5 m & power saving (tweak as needed)
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* svc = server->createService(SVC_UUID);

  txChar = svc->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);

  NimBLECharacteristic* rxChar = svc->createCharacteristic(
      RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(new RxCallbacks());

  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SVC_UUID);
  // Some NimBLE versions don’t have setScanResponse(true); safe to omit.
  // If you want scan response data, use adv->setScanResponseData(...).
  adv->start();

  Serial.println("BLE up. Advertising as Jeni-ESP32");
}

void loop() {
  static uint32_t last = 0;
  static uint32_t count = 0;

  if (deviceConnected && millis() - last >= 1000) {
    last = millis();
    char msg[64];
    snprintf(msg, sizeof(msg), "count=%lu, ms=%lu\n",
             (unsigned long)count++, (unsigned long)millis());
    txChar->setValue((uint8_t*)msg, strlen(msg));
    txChar->notify();
    delay(5);
  }
  delay(10);
}
