// SPI.h — every byte sent with SPI.transfer() goes onto the simulated MAX7219 bus.
#pragma once
#include "Arduino.h"

#define SPI_MODE0 0x00
#define SPI_MODE1 0x01
#define SPI_MODE2 0x02
#define SPI_MODE3 0x03
#define SPI_CLOCK_DIV2 2
#define SPI_CLOCK_DIV4 4
#define SPI_CLOCK_DIV8 8
#define SPI_CLOCK_DIV16 16
#define SPI_CLOCK_DIV32 32
#define SPI_CLOCK_DIV64 64
#define SPI_CLOCK_DIV128 128
#define HSPI 2
#define VSPI 3
#define FSPI 1

class SPISettings {
public:
  SPISettings() {}
  SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) { (void)clock; (void)bitOrder; (void)dataMode; }
};

class SPIClass {
public:
  explicit SPIClass(uint8_t bus = VSPI) { (void)bus; }
  void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1) { (void)sck; (void)miso; (void)mosi; (void)ss; }
  void end() {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}
  void setBitOrder(uint8_t) {}
  void setDataMode(uint8_t) {}
  void setFrequency(uint32_t) {}
  void setClockDivider(uint32_t) {}
  void setHwCs(bool) {}
  uint8_t transfer(uint8_t data);
  uint16_t transfer16(uint16_t data);
  uint32_t transfer32(uint32_t data);
  void transfer(void *buf, uint32_t count);
  void transferBytes(const uint8_t *data, uint8_t *out, uint32_t size);
  void writeBytes(const uint8_t *data, uint32_t size);
  void write(uint8_t data) { transfer(data); }
  void write16(uint16_t data) { transfer16(data); }
  void write32(uint32_t data) { transfer32(data); }
};

extern SPIClass SPI;
