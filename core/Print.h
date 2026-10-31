// Print.h — Arduino Print base class (used by Serial and by Adafruit_GFX text output).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "WString.h"

class Print;
class Printable {
public:
  virtual ~Printable() {}
  virtual size_t printTo(Print &p) const = 0;
};

class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t *buffer, size_t size) {
    size_t n = 0;
    while (size--) n += write(*buffer++);
    return n;
  }
  size_t write(const char *str) { return str ? write((const uint8_t *)str, strlen(str)) : 0; }
  size_t write(const char *buffer, size_t size) { return write((const uint8_t *)buffer, size); }
  virtual int availableForWrite() { return 0; }
  virtual void flush() {}

  size_t print(const __FlashStringHelper *s) { return write(reinterpret_cast<const char *>(s)); }
  size_t print(const String &s) { return write(s.c_str(), s.length()); }
  size_t print(const char s[]) { return write(s); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(unsigned char v, int base = 10) { return printULL(v, base); }
  size_t print(int v, int base = 10) { return printLL(v, base, 32); }
  size_t print(unsigned int v, int base = 10) { return printULL(v, base); }
  size_t print(long v, int base = 10) { return printLL(v, base, 8 * sizeof(long)); }
  size_t print(unsigned long v, int base = 10) { return printULL(v, base); }
  size_t print(long long v, int base = 10) { return printLL(v, base, 64); }
  size_t print(unsigned long long v, int base = 10) { return printULL(v, base); }
  size_t print(double v, int digits = 2);
  size_t print(const Printable &p) { return p.printTo(*this); }

  size_t println() { return write("\r\n"); }
  template <class T> size_t println(const T &v) { size_t n = print(v); return n + println(); }
  template <class T> size_t println(const T &v, int fmt) { size_t n = print(v, fmt); return n + println(); }
  size_t println(const char s[]) { size_t n = print(s); return n + println(); }

  size_t printf(const char *format, ...)
#if defined(__GNUC__)
      __attribute__((format(printf, 2, 3)))
#endif
      ;

private:
  size_t printULL(unsigned long long v, int base);
  size_t printLL(long long v, int base, int bits);
};
