// WString.h — Arduino String class, backed by std::string.
#pragma once
#include <string>
#include <stdint.h>

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
#define FPSTR(s) (reinterpret_cast<const __FlashStringHelper *>(s))

class String {
public:
  String() {}
  String(const char *s) : s_(s ? s : "") {}
  String(const __FlashStringHelper *s) : s_(reinterpret_cast<const char *>(s)) {}
  String(const std::string &s) : s_(s) {}
  String(const String &o) = default;
  String(String &&o) = default;
  explicit String(char c) : s_(1, c) {}
  explicit String(unsigned char v, unsigned char base = 10);
  explicit String(int v, unsigned char base = 10);
  explicit String(unsigned int v, unsigned char base = 10);
  explicit String(long v, unsigned char base = 10);
  explicit String(unsigned long v, unsigned char base = 10);
  explicit String(long long v, unsigned char base = 10);
  explicit String(unsigned long long v, unsigned char base = 10);
  explicit String(float v, unsigned int decimals = 2);
  explicit String(double v, unsigned int decimals = 2);

  String &operator=(const String &o) = default;
  String &operator=(String &&o) = default;
  String &operator=(const char *s) { s_ = s ? s : ""; return *this; }

  unsigned int length() const { return (unsigned int)s_.size(); }
  bool isEmpty() const { return s_.empty(); }
  bool reserve(unsigned int n) { s_.reserve(n); return true; }
  const char *c_str() const { return s_.c_str(); }
  const std::string &str() const { return s_; }

  bool concat(const String &o) { s_ += o.s_; return true; }
  bool concat(const char *o) { if (o) s_ += o; return true; }
  bool concat(char c) { s_ += c; return true; }
  bool concat(unsigned char v) { return concat(String(v)); }
  bool concat(int v) { return concat(String(v)); }
  bool concat(unsigned int v) { return concat(String(v)); }
  bool concat(long v) { return concat(String(v)); }
  bool concat(unsigned long v) { return concat(String(v)); }
  bool concat(long long v) { return concat(String(v)); }
  bool concat(unsigned long long v) { return concat(String(v)); }
  bool concat(float v) { return concat(String(v)); }
  bool concat(double v) { return concat(String(v)); }
  bool concat(const __FlashStringHelper *s) { return concat(reinterpret_cast<const char *>(s)); }

  template <class T> String &operator+=(const T &v) { concat(v); return *this; }

  char charAt(unsigned int i) const { return i < s_.size() ? s_[i] : 0; }
  void setCharAt(unsigned int i, char c) { if (i < s_.size()) s_[i] = c; }
  char operator[](unsigned int i) const { return charAt(i); }
  char &operator[](unsigned int i) { static char dummy; return i < s_.size() ? s_[i] : (dummy = 0); }

  int compareTo(const String &o) const { return s_.compare(o.s_); }
  bool equals(const String &o) const { return s_ == o.s_; }
  bool equals(const char *o) const { return s_ == (o ? o : ""); }
  bool equalsIgnoreCase(const String &o) const;
  bool startsWith(const String &p) const { return s_.compare(0, p.s_.size(), p.s_) == 0 && s_.size() >= p.s_.size(); }
  bool startsWith(const String &p, unsigned int offset) const;
  bool endsWith(const String &p) const;

  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator==(const char *o) const { return equals(o); }
  bool operator!=(const String &o) const { return s_ != o.s_; }
  bool operator!=(const char *o) const { return !equals(o); }
  bool operator<(const String &o) const { return s_ < o.s_; }
  bool operator>(const String &o) const { return s_ > o.s_; }
  bool operator<=(const String &o) const { return s_ <= o.s_; }
  bool operator>=(const String &o) const { return s_ >= o.s_; }

  int indexOf(char c, unsigned int from = 0) const;
  int indexOf(const String &s, unsigned int from = 0) const;
  int lastIndexOf(char c) const;
  int lastIndexOf(char c, unsigned int from) const;
  int lastIndexOf(const String &s) const;
  String substring(unsigned int from) const;
  String substring(unsigned int from, unsigned int to) const;

  void replace(char a, char b);
  void replace(const String &a, const String &b);
  void remove(unsigned int index);
  void remove(unsigned int index, unsigned int count);
  void toLowerCase();
  void toUpperCase();
  void trim();

  long toInt() const;
  float toFloat() const;
  double toDouble() const;

  void getBytes(unsigned char *buf, unsigned int size, unsigned int index = 0) const;
  void toCharArray(char *buf, unsigned int size, unsigned int index = 0) const {
    getBytes((unsigned char *)buf, size, index);
  }

private:
  std::string s_;
};

inline String operator+(const String &a, const String &b) { String r(a); r.concat(b); return r; }
inline String operator+(const String &a, const char *b) { String r(a); r.concat(b); return r; }
inline String operator+(const char *a, const String &b) { String r(a); r.concat(b); return r; }
template <class T> inline String operator+(const String &a, T b) { String r(a); r.concat(b); return r; }
inline bool operator==(const char *a, const String &b) { return b.equals(a); }
