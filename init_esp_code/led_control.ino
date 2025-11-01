#include <Arduino.h>

// ===== Pin assignments =====
#define LED_UP     23
#define LED_DOWN   22
#define LED_LEFT   21
#define LED_RIGHT  19
#define LED_FRONT  18
#define LED_BACK   5

// ===== LED state booleans =====
bool ledUpState    = true;
bool ledDownState  = true;
bool ledLeftState  = true;
bool ledRightState = true;
bool ledFrontState = true;
bool ledBackState  = true;

// ===== Initialization =====
void initLEDs() {
  pinMode(LED_UP, OUTPUT);
  pinMode(LED_DOWN, OUTPUT);
  pinMode(LED_LEFT, OUTPUT);
  pinMode(LED_RIGHT, OUTPUT);
  pinMode(LED_FRONT, OUTPUT);
  pinMode(LED_BACK, OUTPUT);
  allOff();
}

// ===== Turn all LEDs off =====
void allOff() {
  digitalWrite(LED_UP, LOW);
  digitalWrite(LED_DOWN, LOW);
  digitalWrite(LED_LEFT, LOW);
  digitalWrite(LED_RIGHT, LOW);
  digitalWrite(LED_FRONT, LOW);
  digitalWrite(LED_BACK, LOW);
}

// ===== Update LEDs to match boolean states =====
void updateLEDs() {
  digitalWrite(LED_UP,     ledUpState    ? HIGH : LOW);
  digitalWrite(LED_DOWN,   ledDownState  ? HIGH : LOW);
  digitalWrite(LED_LEFT,   ledLeftState  ? HIGH : LOW);
  digitalWrite(LED_RIGHT,  ledRightState ? HIGH : LOW);
  digitalWrite(LED_FRONT,  ledFrontState ? HIGH : LOW);
  digitalWrite(LED_BACK,   ledBackState  ? HIGH : LOW);
}

// ===== Arduino setup() =====
void setup() {
  Serial.begin(115200);
  initLEDs();
  Serial.println("LEDs initialized.");

  // Example: turn on a few LEDs
  // all others stay false
  updateLEDs();
}

// ===== Arduino loop() =====
void loop() {
  // Keep LEDs synced to current boolean states
  updateLEDs();

  // You can change states here or from other code parts:
  // e.g., if (someCondition) ledRightState = true;
  // Just remember: updateLEDs() reflects any changes.

  delay(50);  // light CPU load, still responsive
}
