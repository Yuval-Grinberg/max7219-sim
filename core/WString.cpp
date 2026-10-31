#include "Arduino.h"
#include <algorithm>
#include <cctype>

static std::string intToStr(unsigned long long v, bool neg, unsigned char base) {
  if (base < 2 || base > 36) base = 10;
  char buf[72];
  int i = 71;
  buf[i] = 0;
  do {
    int d = (int)(v % base);
    buf[--i] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
    v /= base;
  } while (v);
  if (neg) buf[--i] = '-';
  return std::string(buf + i);
}

static std::string signedToStr(long long v, unsigned char base) {
  // Arduino prints negative numbers in non-decimal bases as their two's complement.
  if (base == 10 && v < 0) return intToStr((unsigned long long)(-(v + 1)) + 1, true, base);
  return intToStr((unsigned long long)v, false, base);
}

static std::string floatToStr(double v, unsigned int decimals) {
  char buf[64];
  snprintf(buf, sizeof buf, "%.*f", (int)decimals, v);
  return buf;
}

String::String(unsigned char v, unsigned char base) : s_(intToStr(v, false, base)) {}
String::String(int v, unsigned char base) : s_(base == 10 ? signedToStr(v, base) : intToStr((unsigned int)v, false, base)) {}
String::String(unsigned int v, unsigned char base) : s_(intToStr(v, false, base)) {}
String::String(long v, unsigned char base) : s_(base == 10 ? signedToStr(v, base) : intToStr((uint32_t)v, false, base)) {}
String::String(unsigned long v, unsigned char base) : s_(intToStr(v, false, base)) {}
String::String(long long v, unsigned char base) : s_(signedToStr(v, base)) {}
String::String(unsigned long long v, unsigned char base) : s_(intToStr(v, false, base)) {}
String::String(float v, unsigned int d) : s_(floatToStr(v, d)) {}
String::String(double v, unsigned int d) : s_(floatToStr(v, d)) {}

bool String::equalsIgnoreCase(const String &o) const {
  if (s_.size() != o.s_.size()) return false;
  for (size_t i = 0; i < s_.size(); i++)
    if (tolower((unsigned char)s_[i]) != tolower((unsigned char)o.s_[i])) return false;
  return true;
}

bool String::startsWith(const String &p, unsigned int offset) const {
  return offset <= s_.size() && s_.compare(offset, p.s_.size(), p.s_) == 0 && s_.size() - offset >= p.s_.size();
}

bool String::endsWith(const String &p) const {
  return s_.size() >= p.s_.size() && s_.compare(s_.size() - p.s_.size(), p.s_.size(), p.s_) == 0;
}

int String::indexOf(char c, unsigned int from) const {
  size_t r = s_.find(c, from);
  return r == std::string::npos ? -1 : (int)r;
}
int String::indexOf(const String &s, unsigned int from) const {
  size_t r = s_.find(s.s_, from);
  return r == std::string::npos ? -1 : (int)r;
}
int String::lastIndexOf(char c) const {
  size_t r = s_.rfind(c);
  return r == std::string::npos ? -1 : (int)r;
}
int String::lastIndexOf(char c, unsigned int from) const {
  size_t r = s_.rfind(c, from);
  return r == std::string::npos ? -1 : (int)r;
}
int String::lastIndexOf(const String &s) const {
  size_t r = s_.rfind(s.s_);
  return r == std::string::npos ? -1 : (int)r;
}

String String::substring(unsigned int from) const {
  return from >= s_.size() ? String() : String(s_.substr(from));
}
String String::substring(unsigned int from, unsigned int to) const {
  if (from > to) std::swap(from, to);
  if (from >= s_.size()) return String();
  if (to > s_.size()) to = (unsigned int)s_.size();
  return String(s_.substr(from, to - from));
}

void String::replace(char a, char b) { std::replace(s_.begin(), s_.end(), a, b); }
void String::replace(const String &a, const String &b) {
  if (a.s_.empty()) return;
  size_t pos = 0;
  while ((pos = s_.find(a.s_, pos)) != std::string::npos) {
    s_.replace(pos, a.s_.size(), b.s_);
    pos += b.s_.size();
  }
}
void String::remove(unsigned int index) { if (index < s_.size()) s_.erase(index); }
void String::remove(unsigned int index, unsigned int count) { if (index < s_.size()) s_.erase(index, count); }
void String::toLowerCase() { for (auto &c : s_) c = (char)tolower((unsigned char)c); }
void String::toUpperCase() { for (auto &c : s_) c = (char)toupper((unsigned char)c); }
void String::trim() {
  size_t a = 0, b = s_.size();
  while (a < b && isspace((unsigned char)s_[a])) a++;
  while (b > a && isspace((unsigned char)s_[b - 1])) b--;
  s_ = s_.substr(a, b - a);
}

long String::toInt() const { return atol(s_.c_str()); }
float String::toFloat() const { return (float)atof(s_.c_str()); }
double String::toDouble() const { return atof(s_.c_str()); }

void String::getBytes(unsigned char *buf, unsigned int size, unsigned int index) const {
  if (!buf || !size) return;
  unsigned int n = 0;
  if (index < s_.size()) {
    n = (unsigned int)std::min<size_t>(size - 1, s_.size() - index);
    memcpy(buf, s_.data() + index, n);
  }
  buf[n] = 0;
}
