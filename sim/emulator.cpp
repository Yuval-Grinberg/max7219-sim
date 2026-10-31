// emulator.cpp — MAX7219 daisy-chain emulation, pins, clock and Serial plumbing.
#include "sim.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <thread>

namespace sim {

namespace {
// All emulator state lives in one function-local static: the sketch's global objects
// (e.g. `Max72xxPanel matrix(...)`) talk to the hardware from their constructors, which can
// run before this file's globals are constructed (Windows runs constructors in reverse link order).
struct State {
  std::mutex mu;  // guards everything below except the serial queue
  std::vector<Chain> chains;
  std::vector<std::vector<uint16_t>> shift;  // per chain: 16-bit shift register of each chip
  uint64_t frames = 0;
  std::vector<uint8_t> bus;  // bytes clocked out since CS went low
  int activeCs = -1;
  bool warnedOdd = false;
  uint8_t pinMode[256] = {0};
  uint8_t pinLevel[256] = {0};
  std::set<std::string> keysDown;
  std::mutex serialMu;
  std::deque<char> serialIn;
  std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  std::minstd_rand noise{42};
};
State &st() {
  static State s;
  return s;
}

#define g_mu st().mu
#define g_chains st().chains
#define g_shift st().shift
#define g_frames st().frames
#define g_bus st().bus
#define g_activeCs st().activeCs
#define g_warnedOdd st().warnedOdd
#define g_pinMode st().pinMode
#define g_pinLevel st().pinLevel
#define g_keysDown st().keysDown
#define g_serialMu st().serialMu
#define g_serialIn st().serialIn
#define g_start st().start
#define g_noise st().noise

const uint8_t kCodeB[16] = {0x7E, 0x30, 0x6D, 0x79, 0x33, 0x5B, 0x5F, 0x70,
                            0x7F, 0x7B, 0x01, 0x4F, 0x37, 0x0E, 0x67, 0x00};

void execute(Chip &c, uint16_t word) {
  uint8_t op = (word >> 8) & 0x0F, d = word & 0xFF;
  switch (op) {
    case 0: break;  // no-op
    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: {
      int digit = op - 1;
      c.digit[digit] = (c.decodeMode & (1 << digit)) ? (uint8_t)(kCodeB[d & 0x0F] | (d & 0x80)) : d;
      break;
    }
    case 9: c.decodeMode = d; break;
    case 10: c.intensity = d & 0x0F; break;
    case 11: c.scanLimit = d & 0x07; break;
    case 12: c.shutdown = !(d & 1); break;
    case 15: c.displayTest = d & 1; break;
    default: break;
  }
}

Chain &chainFor(uint8_t cs) {
  for (size_t i = 0; i < g_chains.size(); i++)
    if (g_chains[i].csPin == cs) return g_chains[i];
  g_chains.push_back(Chain{});
  g_chains.back().csPin = cs;
  g_shift.emplace_back();
  return g_chains.back();
}

void latch(uint8_t cs) {
  if (g_bus.size() & 1) {
    if (!g_warnedOdd) {
      fprintf(stderr, "[sim] warning: odd number of bytes (%zu) sent while CS pin %d was low; "
                      "a MAX7219 expects 2 bytes per chip.\n", g_bus.size(), cs);
      g_warnedOdd = true;
    }
    g_bus.pop_back();
  }
  size_t words = g_bus.size() / 2;
  if (!words) return;
  Chain &ch = chainFor(cs);
  size_t idx = &ch - &g_chains[0];
  std::vector<uint16_t> &sr = g_shift[idx];

  // Chain length: fixed by --modules if given, otherwise the longest transfer seen so far.
  size_t want = ch.chips.size();
  const Options &o = options();
  if (o.modulesX > 0) want = (size_t)o.modulesX * (size_t)(o.modulesY > 0 ? o.modulesY : 1);
  else if (words > want) want = words;
  if (ch.chips.size() < want) { ch.chips.resize(want); sr.resize(want, 0); }

  // Shift every word through the chain exactly like the real shift registers do:
  // the first word sent ends up in the chip farthest from the ESP32.
  for (size_t w = 0; w < words; w++) {
    uint16_t v = (uint16_t)((g_bus[2 * w] << 8) | g_bus[2 * w + 1]);
    for (size_t i = sr.size(); i-- > 1;) sr[i] = sr[i - 1];
    if (!sr.empty()) sr[0] = v;
  }
  for (size_t i = 0; i < ch.chips.size(); i++) execute(ch.chips[i], sr[i]);
  ch.latches++;
  g_frames++;
}
}  // namespace

Options &options() {
  static Options o;
  return o;
}

void busByte(uint8_t b) {
  std::lock_guard<std::mutex> l(g_mu);
  g_bus.push_back(b);
  if (g_bus.size() > 1 << 16) g_bus.clear();
}

void pinMode(uint8_t pin, uint8_t mode) {
  std::lock_guard<std::mutex> l(g_mu);
  g_pinMode[pin] = mode;
  if (mode == 0x05) g_pinLevel[pin] = 1;  // INPUT_PULLUP idles high
}

void pinWrite(uint8_t pin, uint8_t val) {
  std::lock_guard<std::mutex> l(g_mu);
  uint8_t old = g_pinLevel[pin];
  g_pinLevel[pin] = val;
  if (old && !val) {          // falling edge: a transfer starts
    g_bus.clear();
    g_activeCs = pin;
  } else if (!old && val) {   // rising edge: the MAX7219s latch what was shifted in
    if (!g_bus.empty() && (g_activeCs < 0 || g_activeCs == pin)) latch(pin);
    if (g_activeCs == pin || g_activeCs < 0) g_bus.clear();
    g_activeCs = -1;
  }
}

static bool keyDown(const std::string &k) { return g_keysDown.count(k) > 0; }

int pinRead(uint8_t pin) {
  std::lock_guard<std::mutex> l(g_mu);
  uint8_t mode = g_pinMode[pin];
  for (auto &b : options().buttons) {
    if (b.pin != pin) continue;
    bool pressed = keyDown(b.key);
    bool activeLow = mode == 0x05 || mode == 0x01 || mode == 0;  // pull-up wiring: pressed pulls to GND
    if (mode == 0x09) activeLow = false;                           // INPUT_PULLDOWN: pressed pulls high
    return pressed ? !activeLow : activeLow;
  }
  if (mode == 0x05) return 1;
  if (mode == 0x09) return 0;
  return g_pinLevel[pin];
}

int analogRead(uint8_t pin) {
  std::lock_guard<std::mutex> l(g_mu);
  for (auto &a : options().axes) {
    if (a.pin != pin) continue;
    bool lo = keyDown(a.lowKey), hi = keyDown(a.highKey);
    if (lo && !hi) return 0;
    if (hi && !lo) return 4095;
    return 2048;
  }
  for (auto &b : options().buttons)
    if (b.pin == pin) return keyDown(b.key) ? 0 : 4095;
  return 2040 + (int)(g_noise() % 17);  // an unconnected pin: mid-scale with a little noise
}

void setKeyDown(const std::string &key, bool down) {
  std::lock_guard<std::mutex> l(g_mu);
  if (down) g_keysDown.insert(key);
  else g_keysDown.erase(key);
}

Snapshot snapshot() {
  std::lock_guard<std::mutex> l(g_mu);
  Snapshot s;
  s.chains = g_chains;
  s.frameCounter = g_frames;
  return s;
}

// ---------------- time ----------------
uint64_t nowMicros() {
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - g_start).count();
  return (uint64_t)((double)us * options().speed);
}

void sleepMicros(uint64_t us) {
  if (us == 0) { std::this_thread::yield(); return; }
  // Sleep until a target time so long runs don't drift.
  uint64_t target = nowMicros() + us;
  for (;;) {
    uint64_t now = nowMicros();
    if (now >= target) break;
    double realUs = (double)(target - now) / options().speed;
    if (realUs > 2000) std::this_thread::sleep_for(std::chrono::microseconds((long long)(realUs - 1000)));
    else if (realUs > 200) std::this_thread::sleep_for(std::chrono::microseconds(100));
    else std::this_thread::yield();
  }
}

// ---------------- Serial ----------------
void serialWrite(int port, const uint8_t *data, size_t n) {
  (void)port;
  fwrite(data, 1, n, stdout);
  fflush(stdout);
}

void feedSerial(const char *data, size_t n) {
  std::lock_guard<std::mutex> l(g_serialMu);
  g_serialIn.insert(g_serialIn.end(), data, data + n);
}
int serialAvailable() {
  std::lock_guard<std::mutex> l(g_serialMu);
  return (int)g_serialIn.size();
}
int serialRead() {
  std::lock_guard<std::mutex> l(g_serialMu);
  if (g_serialIn.empty()) return -1;
  int c = (unsigned char)g_serialIn.front();
  g_serialIn.pop_front();
  return c;
}
int serialPeek() {
  std::lock_guard<std::mutex> l(g_serialMu);
  return g_serialIn.empty() ? -1 : (unsigned char)g_serialIn.front();
}

void restart() {
  fprintf(stderr, "\n[sim] ESP.restart() called — exiting the simulator.\n");
  fflush(stdout);
  fflush(stderr);
  std::_Exit(0);
}

}  // namespace sim
