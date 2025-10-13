#include <NimBLEDevice.h>

// NUS UUIDs (case-insensitive)
static const char* SVC_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char* RX_UUID  = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // Write / WriteNR
static const char* TX_UUID  = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // Notify

static NimBLECharacteristic* txChar = nullptr;
static volatile bool deviceConnected = false;

class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
    deviceConnected = true;
    Serial.println("[ESP] Central connected");
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
    deviceConnected = false;
    Serial.printf("[ESP] Central disconnected (reason=%d). Restarting adv…\n", reason);
    NimBLEDevice::startAdvertising();
  }
};

class RxCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
    std::string d = c->getValue();
    if (d.empty()) return;
    Serial.print("[ESP] RX <- ");
    Serial.write((const uint8_t*)d.data(), d.size());
    Serial.println();
    // echo back
    txChar->setValue((const uint8_t*)d.data(), d.size());
    txChar->notify();
  }
};

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[ESP] Booting…");

  NimBLEDevice::init("Jeni-ESP32");
  NimBLEDevice::setSecurityAuth(false, false, false);
  NimBLEDevice::setMTU(185); // optional

  auto server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCB());

  auto svc = server->createService(SVC_UUID);

  txChar = svc->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);

  auto rx = svc->createCharacteristic(RX_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(new RxCB());

  svc->start();

  // Build proper advertising & scan response payloads
  auto adv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advData, scanData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.setName("Jeni-ESP32");
  advData.addServiceUUID(NimBLEUUID(SVC_UUID));
  scanData.setName("Jeni-ESP32");
  scanData.addServiceUUID(NimBLEUUID(SVC_UUID));

  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanData);
  adv->start();

  Serial.println("[ESP] Advertising NUS as 'Jeni-ESP32'…");
}

void loop() {
  delay(50);
}
