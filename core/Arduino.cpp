#include "Arduino.h"
#include "Wire.h"
#include "../sim/sim.h"
#include <random>

HardwareSerial Serial(0);
HardwareSerial Serial1(1);
HardwareSerial Serial2(2);
TwoWire Wire;
EspClass ESP;

void pinMode(uint8_t pin, uint8_t mode) { sim::pinMode(pin, mode); }
void digitalWrite(uint8_t pin, uint8_t val) { sim::pinWrite(pin, val ? HIGH : LOW); }
int digitalRead(uint8_t pin) { return sim::pinRead(pin); }
int analogRead(uint8_t pin) { return sim::analogRead(pin); }
void analogWrite(uint8_t, int) {}
void analogReadResolution(uint8_t) {}

unsigned long millis() { return (unsigned long)(uint32_t)(sim::nowMicros() / 1000); }
unsigned long micros() { return (unsigned long)(uint32_t)sim::nowMicros(); }
void delay(uint32_t ms) { sim::sleepMicros((uint64_t)ms * 1000); }
void delayMicroseconds(uint32_t us) { sim::sleepMicros(us); }
void yield() { sim::sleepMicros(0); }

static std::mt19937 &rng() {
  static std::mt19937 r(12345);
  return r;
}
long random(long howbig) {
  if (howbig <= 0) return 0;
  return (long)(rng()() % (uint32_t)howbig);
}
long random(long howsmall, long howbig) {
  if (howsmall >= howbig) return howsmall;
  return random(howbig - howsmall) + howsmall;
}
void randomSeed(unsigned long seed) {
  if (seed != 0) rng().seed((uint32_t)seed);
}
uint32_t esp_random() { return rng()(); }

long map(long x, long in_min, long in_max, long out_min, long out_max) {
  if (in_max == in_min) return out_min;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val) {
  (void)dataPin; (void)clockPin;
  uint8_t b = val;
  if (bitOrder == LSBFIRST) {
    b = 0;
    for (int i = 0; i < 8; i++) if (val & (1 << i)) b |= (uint8_t)(0x80 >> i);
  }
  sim::busByte(b);  // MAX7219 always takes MSB first; a reversed byte arrives reversed, as on hardware
}
uint8_t shiftIn(uint8_t, uint8_t, uint8_t) { return 0; }
unsigned long pulseIn(uint8_t, uint8_t, unsigned long) { return 0; }

void attachInterrupt(uint8_t, voidFuncPtr, int) {}
void detachInterrupt(uint8_t) {}
void tone(uint8_t, unsigned int, unsigned long) {}
void noTone(uint8_t) {}

uint16_t makeWord(uint16_t w) { return w; }
uint16_t makeWord(uint8_t h, uint8_t l) { return (uint16_t)((h << 8) | l); }

void EspClass::restart() { sim::restart(); }

// ---------------- Serial ----------------
int HardwareSerial::available() { return port_ == 0 ? sim::serialAvailable() : 0; }
int HardwareSerial::read() { return port_ == 0 ? sim::serialRead() : -1; }
int HardwareSerial::peek() { return port_ == 0 ? sim::serialPeek() : -1; }
void HardwareSerial::flush() {}
size_t HardwareSerial::write(uint8_t c) { sim::serialWrite(port_, &c, 1); return 1; }
size_t HardwareSerial::write(const uint8_t *buffer, size_t size) { sim::serialWrite(port_, buffer, size); return size; }

int Stream::timedRead() {
  unsigned long start = millis();
  do {
    int c = read();
    if (c >= 0) return c;
    delay(1);
  } while (millis() - start < timeout_);
  return -1;
}
int Stream::timedPeek() {
  unsigned long start = millis();
  do {
    int c = peek();
    if (c >= 0) return c;
    delay(1);
  } while (millis() - start < timeout_);
  return -1;
}
size_t Stream::readBytes(char *buffer, size_t length) {
  size_t n = 0;
  while (n < length) {
    int c = timedRead();
    if (c < 0) break;
    buffer[n++] = (char)c;
  }
  return n;
}
size_t Stream::readBytesUntil(char terminator, char *buffer, size_t length) {
  size_t n = 0;
  while (n < length) {
    int c = timedRead();
    if (c < 0 || c == terminator) break;
    buffer[n++] = (char)c;
  }
  return n;
}
String Stream::readString() {
  String s;
  int c;
  while ((c = timedRead()) >= 0) s += (char)c;
  return s;
}
String Stream::readStringUntil(char terminator) {
  String s;
  int c;
  while ((c = timedRead()) >= 0 && c != terminator) s += (char)c;
  return s;
}
long Stream::parseInt() {
  int c;
  while ((c = timedPeek()) >= 0 && !(isdigit(c) || c == '-')) read();
  if (c < 0) return 0;
  bool neg = false;
  long v = 0;
  if (c == '-') { neg = true; read(); }
  while ((c = timedPeek()) >= 0 && isdigit(c)) { v = v * 10 + (c - '0'); read(); }
  return neg ? -v : v;
}
float Stream::parseFloat() {
  int c;
  while ((c = timedPeek()) >= 0 && !(isdigit(c) || c == '-' || c == '.')) read();
  if (c < 0) return 0;
  String s;
  while ((c = timedPeek()) >= 0 && (isdigit(c) || c == '-' || c == '.')) { s += (char)c; read(); }
  return s.toFloat();
}
bool Stream::find(const char *target) {
  size_t len = strlen(target), idx = 0;
  if (!len) return true;
  int c;
  while ((c = timedRead()) >= 0) {
    if (c == target[idx]) { if (++idx == len) return true; }
    else idx = (c == target[0]) ? 1 : 0;
  }
  return false;
}
