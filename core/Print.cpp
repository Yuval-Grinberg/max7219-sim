#include "Arduino.h"
#include <vector>

size_t Print::printULL(unsigned long long v, int base) {
  if (base == 0) return write((uint8_t)v);
  if (base < 2) base = 10;
  char buf[72];
  int i = 71;
  buf[i] = 0;
  do {
    int d = (int)(v % base);
    buf[--i] = (char)(d < 10 ? '0' + d : 'A' + d - 10);
    v /= base;
  } while (v);
  return write(buf + i);
}

size_t Print::printLL(long long v, int base, int bits) {
  if (base == 0) return write((uint8_t)v);
  if (base == 10) {
    if (v < 0) {
      size_t n = print('-');
      return n + printULL((unsigned long long)(-(v + 1)) + 1, 10);
    }
    return printULL((unsigned long long)v, 10);
  }
  // Like Arduino: negative values in HEX/BIN/OCT print as two's complement of the type width.
  unsigned long long u = (unsigned long long)v;
  if (bits < 64) u &= (1ULL << bits) - 1;
  return printULL(u, base);
}

size_t Print::print(double v, int digits) {
  if (std::isnan(v)) return print("nan");
  if (std::isinf(v)) return print("inf");
  char buf[64];
  snprintf(buf, sizeof buf, "%.*f", digits < 0 ? 0 : digits, v);
  return write(buf);
}

size_t Print::printf(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  va_list ap2;
  va_copy(ap2, ap);
  int len = vsnprintf(nullptr, 0, format, ap);
  va_end(ap);
  if (len <= 0) { va_end(ap2); return 0; }
  std::vector<char> buf((size_t)len + 1);
  vsnprintf(buf.data(), buf.size(), format, ap2);
  va_end(ap2);
  return write((const uint8_t *)buf.data(), (size_t)len);
}
