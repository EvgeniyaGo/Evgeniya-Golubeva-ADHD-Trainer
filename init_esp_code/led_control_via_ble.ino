#if 0
#include <Arduino.h>
#include <Wire.h>
#include <cstring>
#include <math.h>
#include <NimBLEDevice.h>

/* ===================== Pins ===================== */
#define LED_UP     5
#define LED_DOWN   18
#define LED_LEFT   22
#define LED_RIGHT  23
#define LED_FRONT  21
#define LED_BACK   19
#define SDA_PIN    25
#define SCL_PIN    26

/* ===================== Axis tags (uint8_t to dodge proto issues) ===================== */
static constexpr uint8_t AX_X = 0;
static constexpr uint8_t AX_Y = 1;
static constexpr uint8_t AX_Z = 2;
static constexpr uint8_t AX_NONE = 255;

static inline const char* faceForStr(uint8_t ax, bool positive) {
  if (ax == AX_Z) return positive ? "Upward"   : "Downward";
  if (ax == AX_X) return positive ? "Right"    : "Left";
  if (ax == AX_Y) return positive ? "Front"    : "Back";
  return "Unknown";
}

/* ===================== LED states ===================== */
bool ledUpState=false, ledDownState=false, ledLeftState=false, ledRightState=false, ledFrontState=false, ledBackState=false;

/* ===================== ADXL345 (minimal I2C) ===================== */
namespace ADXL345 {
  constexpr uint8_t REG_DEVID=0x00, REG_BW_RATE=0x2C, REG_POWERCTL=0x2D, REG_DATAFMT=0x31, REG_DATAX0=0x32;
  static uint8_t I2C_ADDR=0x53;
  inline void write8(uint8_t r,uint8_t v){Wire.beginTransmission(I2C_ADDR);Wire.write(r);Wire.write(v);Wire.endTransmission();}
  inline uint8_t read8(uint8_t r){Wire.beginTransmission(I2C_ADDR);Wire.write(r);Wire.endTransmission(false);Wire.requestFrom(I2C_ADDR,(uint8_t)1);return Wire.available()?Wire.read():0;}
  inline bool readXYZ_raw(int16_t &x,int16_t &y,int16_t &z){
    Wire.beginTransmission(I2C_ADDR); Wire.write(REG_DATAX0);
    if(Wire.endTransmission(false)!=0) return false;
    Wire.requestFrom(I2C_ADDR,(uint8_t)6); if(Wire.available()<6) return false;
    uint8_t b0=Wire.read(),b1=Wire.read(),b2=Wire.read(),b3=Wire.read(),b4=Wire.read(),b5=Wire.read();
    x=(int16_t)((b1<<8)|b0); y=(int16_t)((b3<<8)|b2); z=(int16_t)((b5<<8)|b4);
    return true;
  }
  inline bool probeAt(uint8_t a){I2C_ADDR=a;return read8(REG_DEVID)==0xE5;}
  inline bool begin(){ if(!probeAt(0x53)&&!probeAt(0x1D)) return false;
    write8(REG_BW_RATE,0x08); write8(REG_DATAFMT,0x0A); write8(REG_POWERCTL,0x08); delay(10); return true;}
}

/* ===================== Face detection tuning ===================== */
const float ALPHA=0.25f, MIN_G=6.5f, HYSTERESIS=0.6f;
const uint16_t PERIOD=40; // ms
const float LSB_TO_MPS2=0.0039f*9.80665f;
const int8_t SIGN_X=+1,SIGN_Y=+1,SIGN_Z=+1;

/* ===================== State ===================== */
float xf=0, yf=0, zf=9.81f;
uint8_t lastAxis=AX_NONE;
String lastOut="";

/* ===================== App control ===================== */
volatile bool appPermission=false;
volatile bool permissionChanged=false;
unsigned long loopCounter=0;

/* ===================== BLE (NUS) ===================== */
static const char* SVC_UUID="6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char* RX_UUID ="6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static const char* TX_UUID ="6e400003-b5a3-f393-e0a9-e50e24dcca9e";
static NimBLECharacteristic* txChar=nullptr;
static volatile bool deviceConnected=false;

static void bleSend(const String& line){
  if(!deviceConnected || !txChar) return;
  txChar->setValue((uint8_t*)line.c_str(), line.length());
  txChar->notify();
}

class ServerCB: public NimBLEServerCallbacks{
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { deviceConnected=true;  Serial.println("[BLE] Connected"); }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override { deviceConnected=false; Serial.println("[BLE] Disconnected, adv restart"); NimBLEDevice::startAdvertising(); }
};

class RxCB: public NimBLECharacteristicCallbacks{
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override{
    std::string d=c->getValue(); if(d.empty()) return;
    String s=String(d.c_str()); s.trim();
    Serial.print("[BLE RX] "); Serial.println(s);
    if(s.equalsIgnoreCase("START"))   { appPermission=true;  permissionChanged=true; bleSend("ACK START app=1\n"); }
    else if(s.equalsIgnoreCase("STOP")){ appPermission=false; permissionChanged=true; bleSend("ACK STOP app=0\n"); }
    else if(s.equalsIgnoreCase("TOGGLE")){ appPermission=!appPermission; permissionChanged=true; bleSend(String("ACK TOGGLE app=")+(appPermission?"1\n":"0\n")); }
    else if(s.equalsIgnoreCase("STATE?")){
      String st = String("STATE ")+
        "up="    +(ledUpState    ?"1 ":"0 ")+
        "down="  +(ledDownState  ?"1 ":"0 ")+
        "left="  +(ledLeftState  ?"1 ":"0 ")+
        "right=" +(ledRightState ?"1 ":"0 ")+
        "front=" +(ledFrontState ?"1 ":"0 ")+
        "back="  +(ledBackState  ?"1 ":"0 ")+
        "app="   +(appPermission ?"1\n":"0\n");
      bleSend(st);
    } else {
      bleSend(String("ERR unknown_cmd: ")+s+"\n");
    }
  }
};

/* ===================== LED helpers ===================== */
void allOff(){
  digitalWrite(LED_UP,LOW);   digitalWrite(LED_DOWN,LOW);
  digitalWrite(LED_LEFT,LOW); digitalWrite(LED_RIGHT,LOW);
  digitalWrite(LED_FRONT,LOW);digitalWrite(LED_BACK,LOW);
  ledUpState=ledDownState=ledLeftState=ledRightState=ledFrontState=ledBackState=false;
}
void applyLedBools(){
  digitalWrite(LED_UP,ledUpState?HIGH:LOW);
  digitalWrite(LED_DOWN,ledDownState?HIGH:LOW);
  digitalWrite(LED_LEFT,ledLeftState?HIGH:LOW);
  digitalWrite(LED_RIGHT,ledRightState?HIGH:LOW);
  digitalWrite(LED_FRONT,ledFrontState?HIGH:LOW);
  digitalWrite(LED_BACK,ledBackState?HIGH:LOW);
}
void setOnlyFace(const char* n){
  ledUpState=ledDownState=ledLeftState=ledRightState=ledFrontState=ledBackState=false;
  if     (!strcmp(n,"Upward"))   ledUpState=true;
  else if(!strcmp(n,"Downward")) ledDownState=true;
  else if(!strcmp(n,"Left"))     ledLeftState=true;
  else if(!strcmp(n,"Right"))    ledRightState=true;
  else if(!strcmp(n,"Front"))    ledFrontState=true;
  else if(!strcmp(n,"Back"))     ledBackState=true;
  applyLedBools();
}

/* ===================== Init ===================== */
void initLEDs(){ pinMode(LED_UP,OUTPUT); pinMode(LED_DOWN,OUTPUT); pinMode(LED_LEFT,OUTPUT); pinMode(LED_RIGHT,OUTPUT); pinMode(LED_FRONT,OUTPUT); pinMode(LED_BACK,OUTPUT); allOff(); }
void initBLE(){
  NimBLEDevice::init("Jeni-ESP32");
  NimBLEDevice::setSecurityAuth(false,false,false);
  NimBLEDevice::setMTU(185);
  auto server=NimBLEDevice::createServer(); server->setCallbacks(new ServerCB());
  auto svc=server->createService(SVC_UUID);
  txChar=svc->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);
  auto rx=svc->createCharacteristic(RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(new RxCB());
  svc->start();
  auto adv=NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advData, scanData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.setName("Jeni-ESP32"); advData.addServiceUUID(NimBLEUUID(SVC_UUID));
  scanData.setName("Jeni-ESP32"); scanData.addServiceUUID(NimBLEUUID(SVC_UUID));
  adv->setAdvertisementData(advData); adv->setScanResponseData(scanData); adv->start();
  Serial.println("[BLE] Advertising NUS as 'Jeni-ESP32'…");
}

/* ===================== Arduino lifecycle ===================== */
void setup(){
  Serial.begin(115200); delay(200);
  Serial.println("\n[BOOT] Face detector + BLE NUS");
  initLEDs();
  Wire.begin(SDA_PIN,SCL_PIN); delay(5);
  if(!ADXL345::begin()){ Serial.println("Failed to find ADXL345!"); while(true) delay(1000); }
  int16_t rx,ry,rz;
  if(ADXL345::readXYZ_raw(rx,ry,rz)){
    xf=(SIGN_X*rx)*LSB_TO_MPS2; yf=(SIGN_Y*ry)*LSB_TO_MPS2; zf=(SIGN_Z*rz)*LSB_TO_MPS2;
  }
  initBLE();
  Serial.println("Ready: Up/Down/Left/Right/Front/Back; use START/STOP/TOGGLE");
}

void loop(){
  // === Read & filter ===
  int16_t rx,ry,rz;
  if(ADXL345::readXYZ_raw(rx,ry,rz)){
    float ax_mps2=(SIGN_X*rx)*LSB_TO_MPS2, ay_mps2=(SIGN_Y*ry)*LSB_TO_MPS2, az_mps2=(SIGN_Z*rz)*LSB_TO_MPS2;
    xf = xf + ALPHA*(ax_mps2 - xf);
    yf = yf + ALPHA*(ay_mps2 - yf);
    zf = zf + ALPHA*(az_mps2 - zf);

    float ax=fabsf(xf), ay=fabsf(yf), az=fabsf(zf);
    if(lastAxis==AX_X) ax+=HYSTERESIS; else if(lastAxis==AX_Y) ay+=HYSTERESIS; else if(lastAxis==AX_Z) az+=HYSTERESIS;

    uint8_t dom; bool pos; float domMag;
    if(ax>=ay && ax>=az){ dom=AX_X; pos=(xf>0); domMag=ax; }
    else if(ay>=ax && ay>=az){ dom=AX_Y; pos=(yf>0); domMag=ay; }
    else { dom=AX_Z; pos=(zf>0); domMag=az; }

    const char* out="Unknown"; if(domMag>=MIN_G) out = faceForStr(dom,pos);

    // === Apply LEDs (with immediate reaction to permission changes) ===
    if(permissionChanged){
      if(appPermission){
        if(strcmp(out,"Unknown")==0) allOff(); else setOnlyFace(out);
      } else {
        allOff();
      }
      lastOut = out;
      permissionChanged = false;
    } else if(lastOut != out){
      Serial.println(out);
      if(appPermission){
        if(strcmp(out,"Unknown")==0) allOff(); else setOnlyFace(out);
      } else {
        allOff();
      }
      lastOut = out;
    }
    lastAxis = dom;
  }

  // === Periodic state push (every %100) ===
  loopCounter++;
  if(deviceConnected && (loopCounter % 100UL) == 0UL){
    String st = String("STATE ")+
      "up="    +(ledUpState    ?"1 ":"0 ")+
      "down="  +(ledDownState  ?"1 ":"0 ")+
      "left="  +(ledLeftState  ?"1 ":"0 ")+
      "right=" +(ledRightState ?"1 ":"0 ")+
      "front=" +(ledFrontState ?"1 ":"0 ")+
      "back="  +(ledBackState  ?"1 ":"0 ")+
      "app="   +(appPermission ?"1\n":"0\n");
    bleSend(st);
  }

  delay(PERIOD);
}
#endif