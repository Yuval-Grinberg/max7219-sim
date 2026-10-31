// Arduino.h — PC stand-in for the ESP32 Arduino core.
// Covers the parts of the Arduino API that LED-matrix sketches normally use.
// Anything that talks to real hardware (pins, SPI, Serial) is routed to the simulator.
#pragma once

#ifndef ARDUINO
#define ARDUINO 10819
#endif
#ifndef ESP32
#define ESP32 1
#endif
#ifndef ARDUINO_ARCH_ESP32
#define ARDUINO_ARCH_ESP32 1
#endif
#define MAX7219_SIMULATOR 1   // lets a sketch do #ifdef MAX7219_SIMULATOR if it ever needs to

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>

#include "binary.h"
#include "pgmspace.h"

typedef bool boolean;
typedef uint8_t byte;
typedef uint16_t word;

#define HIGH 0x1
#define LOW 0x0

#define INPUT 0x01
#define OUTPUT 0x03
#define PULLUP 0x04
#define INPUT_PULLUP 0x05
#define PULLDOWN 0x08
#define INPUT_PULLDOWN 0x09

#define LSBFIRST 0
#define MSBFIRST 1

#define CHANGE 0x03
#define FALLING 0x02
#define RISING 0x01

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define EULER 2.718281828459045235360287471352

// ESP32 DevKit pin names
#define LED_BUILTIN 2
#define A0 36
#define A3 39
#define A4 32
#define A5 33
#define A6 34
#define A7 35
#define A10 4
#define A11 0
#define A12 2
#define A13 15
#define A14 13
#define A15 12
#define A16 14
#define A17 27
#define A18 25
#define A19 26
#define SS 5
#define MOSI 23
#define MISO 19
#define SCK 18
#define SDA 21
#define SCL 22

#if defined(_MSC_VER) && !defined(__clang__)
#define __builtin_bswap16 _byteswap_ushort
#define __builtin_bswap32 _byteswap_ulong
#endif

#define IRAM_ATTR
#define DRAM_ATTR
#define RTC_DATA_ATTR

#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitToggle(value, bit) ((value) ^= (1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#define bit(b) (1UL << (b))
#define sq(x) ((x) * (x))
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)
#define interrupts()
#define noInterrupts()
#define digitalPinToInterrupt(p) (p)
#define clockCyclesPerMicrosecond() 240
#define F_CPU 240000000L

#ifdef __cplusplus

#include <algorithm>
#include <cmath>
#include <cstdlib>

// Arduino's min/max/constrain accept mixed argument types.
template <class A, class B> inline auto min(A a, B b) -> decltype(a < b ? a : b) { return b < a ? b : a; }
template <class A, class B> inline auto max(A a, B b) -> decltype(a < b ? a : b) { return a < b ? b : a; }
template <class T, class L, class H> inline T constrain(T x, L lo, H hi) {
  return x < (T)lo ? (T)lo : (x > (T)hi ? (T)hi : x);
}
using std::abs;
using std::round;

#include "WString.h"
#include "Print.h"
#include "HardwareSerial.h"

typedef void (*voidFuncPtr)(void);

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int value);
void analogReadResolution(uint8_t bits);

unsigned long millis();
unsigned long micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void yield();
inline void optimistic_yield(uint32_t) { yield(); }

long random(long howbig);
long random(long howsmall, long howbig);
void randomSeed(unsigned long seed);
long map(long x, long in_min, long in_max, long out_min, long out_max);
uint32_t esp_random();

void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val);
uint8_t shiftIn(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder);
unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout = 1000000L);

void attachInterrupt(uint8_t pin, voidFuncPtr fn, int mode);
void detachInterrupt(uint8_t pin);

void tone(uint8_t pin, unsigned int frequency, unsigned long duration = 0);
void noTone(uint8_t pin);

uint16_t makeWord(uint16_t w);
uint16_t makeWord(uint8_t h, uint8_t l);

// A few ESP32-specific bits sketches sometimes touch.
class EspClass {
public:
  void restart();
  uint32_t getFreeHeap() { return 240000; }
  uint32_t getHeapSize() { return 320000; }
  uint32_t getCpuFreqMHz() { return 240; }
  const char *getChipModel() { return "ESP32 (simulated)"; }
  uint64_t getEfuseMac() { return 0x1234567890ABULL; }
};
extern EspClass ESP;

// The sketch provides these.
void setup();
void loop();

#endif // __cplusplus
