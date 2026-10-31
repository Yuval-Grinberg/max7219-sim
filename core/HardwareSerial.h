// HardwareSerial.h — Serial goes to the terminal; text typed in the terminal comes back as Serial input.
#pragma once
#include "Print.h"

class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  void setTimeout(unsigned long ms) { timeout_ = ms; }
  unsigned long getTimeout() const { return timeout_; }
  size_t readBytes(char *buffer, size_t length);
  size_t readBytes(uint8_t *buffer, size_t length) { return readBytes((char *)buffer, length); }
  size_t readBytesUntil(char terminator, char *buffer, size_t length);
  String readString();
  String readStringUntil(char terminator);
  long parseInt();
  float parseFloat();
  bool find(const char *target);

protected:
  int timedRead();
  int timedPeek();
  unsigned long timeout_ = 1000;
};

class HardwareSerial : public Stream {
public:
  explicit HardwareSerial(int port) : port_(port) {}
  void begin(unsigned long baud, uint32_t config = 0, int8_t rxPin = -1, int8_t txPin = -1, bool invert = false,
             unsigned long timeoutMs = 20000UL) {
    (void)baud; (void)config; (void)rxPin; (void)txPin; (void)invert; (void)timeoutMs;
  }
  void end() {}
  int available() override;
  int read() override;
  int peek() override;
  void flush() override;
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buffer, size_t size) override;
  using Print::write;
  int availableForWrite() override { return 128; }
  operator bool() const { return true; }

private:
  int port_;
};

extern HardwareSerial Serial;
extern HardwareSerial Serial1;
extern HardwareSerial Serial2;
