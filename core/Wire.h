// Wire.h — I2C is not simulated; this stub only lets sketches that include it compile.
#pragma once
#include "Arduino.h"
class TwoWire : public Stream {
public:
  bool begin(int sda = -1, int scl = -1, uint32_t freq = 0) { (void)sda; (void)scl; (void)freq; return true; }
  void setClock(uint32_t) {}
  void beginTransmission(uint8_t) {}
  uint8_t endTransmission(bool = true) { return 2; }  // 2 = NACK: no device present
  uint8_t requestFrom(uint8_t, uint8_t, bool = true) { return 0; }
  size_t write(uint8_t) override { return 1; }
  using Print::write;
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
};
extern TwoWire Wire;
