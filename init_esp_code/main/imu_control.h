#ifndef IMU_CONTROL_H
#define IMU_CONTROL_H

#include <Arduino.h>
#include "types.h"

// I2C pins for ADXL345
#define SDA_PIN 13
#define SCL_PIN 22

// Tilt thresholds
#define VALID_TILT_THRESHOLD_PERCENT 30.0f
#define LOCK_TILT_THRESHOLD_PERCENT   3.5f

bool initImu();
void updateImu();
ImuState getImuState();
bool isValidUpFace();
bool isFaceLocked();

#endif
