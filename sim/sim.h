// sim.h — the boundary between the fake Arduino core and the simulator.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>

namespace sim {

// ---- called by the Arduino core (sketch thread) ----
void busByte(uint8_t b);                 // one byte clocked out on DIN (SPI.transfer / shiftOut)
void pinMode(uint8_t pin, uint8_t mode);
void pinWrite(uint8_t pin, uint8_t val); // a LOW->HIGH edge on a CS pin latches the MAX7219 chain
int pinRead(uint8_t pin);
int analogRead(uint8_t pin);
uint64_t nowMicros();
void sleepMicros(uint64_t us);
void serialWrite(int port, const uint8_t *data, size_t n);
int serialAvailable();
int serialRead();
int serialPeek();
[[noreturn]] void restart();

// ---- emulated hardware state ----
struct Chip {                 // one MAX7219 / MAX7221
  uint8_t digit[8] = {0};     // digit registers 0..7 (on an 8x8 module: one row or column each)
  uint8_t intensity = 0;      // 0..15
  uint8_t scanLimit = 7;      // digits 0..scanLimit are shown
  uint8_t decodeMode = 0;
  bool shutdown = true;       // the real chip powers up in shutdown mode
  bool displayTest = false;
};

struct Chain {                // chips daisy-chained on one CS (LOAD) pin
  uint8_t csPin = 0;
  std::vector<Chip> chips;    // chips[0] is the one wired to the ESP32
  uint64_t latches = 0;
};

struct Snapshot {
  std::vector<Chain> chains;
  uint64_t frameCounter = 0;  // increases on every latch
};
Snapshot snapshot();

// ---- options (command line) ----
struct Options {
  int modulesX = 0, modulesY = 0;  // physical grid; 0 = auto (one row)
  bool rightToLeft = false;        // chip 0 sits at the right end of the row
  bool serpentine = false;         // rows alternate direction
  int moduleRotation = 0;          // 0..3, how each module is physically turned
  double speed = 1.0;              // time scale
  int ledPitch = 22;
  uint32_t onColor = 0xFF2A1A;     // classic red; --color green|blue|white|yellow|RRGGBB
  bool headless = false;
  bool console = false;
  long runMs = -1;                 // stop after this long (sketch time)
  std::string screenshot;          // write a .bmp when stopping
  struct Key { int pin; std::string key; };
  std::vector<Key> buttons;        // --button PIN=KEY : key held => pin reads "pressed"
  struct Axis { int pin; std::string lowKey, highKey; };
  std::vector<Axis> axes;          // --axis PIN=KEYLOW,KEYHIGH : analogRead gives 0 / 2048 / 4095
};
Options &options();

// ---- input from the front-end ----
void setKeyDown(const std::string &key, bool down);
void feedSerial(const char *data, size_t n);

// ---- rendering ----
struct Layout {               // where each chip's pixel ends up on screen, in LED units
  int widthLeds = 0, heightLeds = 0;
  struct Px { int chain, chip, digit, bit; };
  std::vector<Px> map;        // widthLeds*heightLeds entries, chain = -1 for gaps
  struct Module { int x, y, chain, chip; };  // top-left LED of each 8x8 module
  std::vector<Module> modules;
};
Layout computeLayout(const Snapshot &s);
// Renders the snapshot into an ARGB image of w x h. Returns the LED grid position under (mx,my) if any.
void render(const Snapshot &s, const Layout &l, uint32_t *pixels, int w, int h);
bool ledAt(const Layout &l, int w, int h, int mx, int my, int &col, int &row);
void preferredSize(const Layout &l, int &w, int &h);
bool writeBMP(const std::string &path, const uint32_t *pixels, int w, int h);
std::string consoleFrame(const Snapshot &s, const Layout &l, bool color);

} // namespace sim
