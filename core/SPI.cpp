#include "SPI.h"
#include "../sim/sim.h"

SPIClass SPI;

uint8_t SPIClass::transfer(uint8_t data) { sim::busByte(data); return 0; }
uint16_t SPIClass::transfer16(uint16_t data) { transfer((uint8_t)(data >> 8)); transfer((uint8_t)data); return 0; }
uint32_t SPIClass::transfer32(uint32_t data) { transfer16((uint16_t)(data >> 16)); transfer16((uint16_t)data); return 0; }
void SPIClass::transfer(void *buf, uint32_t count) {
  uint8_t *p = (uint8_t *)buf;
  for (uint32_t i = 0; i < count; i++) { transfer(p[i]); p[i] = 0; }
}
void SPIClass::transferBytes(const uint8_t *data, uint8_t *out, uint32_t size) {
  for (uint32_t i = 0; i < size; i++) { transfer(data ? data[i] : 0xFF); if (out) out[i] = 0; }
}
void SPIClass::writeBytes(const uint8_t *data, uint32_t size) { for (uint32_t i = 0; i < size; i++) transfer(data[i]); }
