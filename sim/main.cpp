// main.cpp — runs the sketch on its own thread and shows the LED modules.
#include "sim.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <thread>
#include <algorithm>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// From the sketch (not including Arduino.h here: it clashes with windows.h).
void setup();
void loop();
void yield();
extern const char *sim_sketch_args;  // from "// sim: ..." lines in the sketch (generated)
extern const char *sim_sketch_name;

namespace sim {
int runSdl();  // frontend_sdl.cpp (only when SDL2 is available)
bool haveSdl();
}

using namespace sim;

static void usage() {
  printf(
      "MAX7219 LED matrix simulator — runs %s\n\n"
      "Wiring (make the simulated modules match your real ones):\n"
      "  --modules WxH        physical grid of 8x8 modules (default: all chips in one row)\n"
      "  --rtl                chip 0 (the one wired to the ESP32) is at the right end\n"
      "  --serpentine         rows alternate direction (zig-zag wiring)\n"
      "  --rotate N           every module is physically turned N x 90 degrees (0-3)\n"
      "Input:\n"
      "  --button PIN=KEY     holding KEY presses a button on PIN (to GND for INPUT_PULLUP)\n"
      "  --axis PIN=LOW,HIGH  analogRead(PIN) gives 0 / 2048 / 4095 for joystick keys\n"
      "                       keys: a-z 0-9 space enter up down left right shift ctrl tab\n"
      "  Typing in this terminal and pressing Enter sends text to Serial.\n"
      "Display:\n"
      "  --color NAME|RRGGBB  red (default), green, blue, yellow, white, orange\n"
      "  --pitch N            LED spacing in pixels (default 22)\n"
      "  --speed X            run sketch time X times faster (0.25 = slow motion)\n"
      "  --console            draw the matrix in the terminal instead of a window\n"
      "  --headless           no output window (use with --run-ms / --screenshot)\n"
      "  --run-ms N           stop after N ms of sketch time\n"
      "  --screenshot FILE    save a .bmp of the display when stopping\n\n"
      "Options can also live in the sketch as comment lines:  // sim: --modules 4x1 --rtl\n",
      sim_sketch_name);
}

static bool parseColor(const std::string &v, uint32_t &out) {
  struct { const char *n; uint32_t c; } names[] = {
      {"red", 0xFF2A1A}, {"green", 0x2BFF4A}, {"blue", 0x3A7BFF}, {"yellow", 0xFFD21F},
      {"white", 0xF4F7FF}, {"orange", 0xFF7A12}, {"pink", 0xFF3FA4}, {"cyan", 0x2BE8FF}};
  for (auto &n : names)
    if (v == n.n) { out = n.c; return true; }
  std::string h = v[0] == '#' ? v.substr(1) : v;
  if (h.size() != 6 || h.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return false;
  out = (uint32_t)strtoul(h.c_str(), nullptr, 16);
  return true;
}

static std::string lower(std::string s) {
  for (auto &c : s) c = (char)tolower((unsigned char)c);
  return s;
}

static bool parseArgs(const std::vector<std::string> &args) {
  Options &o = options();
  for (size_t i = 0; i < args.size(); i++) {
    const std::string &a = args[i];
    auto next = [&](std::string &v) {
      if (i + 1 >= args.size()) { fprintf(stderr, "[sim] %s needs a value\n", a.c_str()); return false; }
      v = args[++i];
      return true;
    };
    std::string v;
    if (a == "--help" || a == "-h") { usage(); exit(0); }
    else if (a == "--modules") {
      if (!next(v)) return false;
      if (sscanf(v.c_str(), "%dx%d", &o.modulesX, &o.modulesY) < 1 || o.modulesX < 1) {
        fprintf(stderr, "[sim] --modules wants WxH, e.g. 4x1\n");
        return false;
      }
    } else if (a == "--rtl") o.rightToLeft = true;
    else if (a == "--ltr") o.rightToLeft = false;
    else if (a == "--serpentine") o.serpentine = true;
    else if (a == "--rotate") { if (!next(v)) return false; o.moduleRotation = atoi(v.c_str()) & 3; }
    else if (a == "--speed") { if (!next(v)) return false; o.speed = atof(v.c_str()); if (o.speed <= 0) o.speed = 1; }
    else if (a == "--pitch") { if (!next(v)) return false; o.ledPitch = std::max(4, atoi(v.c_str())); }
    else if (a == "--color") {
      if (!next(v)) return false;
      if (!parseColor(lower(v), o.onColor)) { fprintf(stderr, "[sim] unknown color %s\n", v.c_str()); return false; }
    } else if (a == "--button") {
      if (!next(v)) return false;
      size_t eq = v.find('=');
      if (eq == std::string::npos) { fprintf(stderr, "[sim] --button wants PIN=KEY\n"); return false; }
      o.buttons.push_back({atoi(v.substr(0, eq).c_str()), lower(v.substr(eq + 1))});
    } else if (a == "--axis") {
      if (!next(v)) return false;
      size_t eq = v.find('='), comma = v.find(',');
      if (eq == std::string::npos || comma == std::string::npos) { fprintf(stderr, "[sim] --axis wants PIN=LOW,HIGH\n"); return false; }
      o.axes.push_back({atoi(v.substr(0, eq).c_str()), lower(v.substr(eq + 1, comma - eq - 1)), lower(v.substr(comma + 1))});
    } else if (a == "--console") o.console = true;
    else if (a == "--headless") o.headless = true;
    else if (a == "--run-ms") { if (!next(v)) return false; o.runMs = atol(v.c_str()); }
    else if (a == "--screenshot") { if (!next(v)) return false; o.screenshot = v; }
    else { fprintf(stderr, "[sim] unknown option %s (try --help)\n", a.c_str()); return false; }
  }
  return true;
}

static std::vector<std::string> splitArgs(const std::string &s) {
  std::vector<std::string> out;
  std::istringstream in(s);
  std::string w;
  while (in >> w) out.push_back(w);
  return out;
}

void saveScreenshot() {
  const Options &o = options();
  if (o.screenshot.empty()) return;
  Snapshot s = snapshot();
  Layout l = computeLayout(s);
  int w, h;
  preferredSize(l, w, h);
  std::vector<uint32_t> px((size_t)w * h);
  render(s, l, px.data(), w, h);
  if (writeBMP(o.screenshot, px.data(), w, h)) fprintf(stderr, "[sim] saved %s\n", o.screenshot.c_str());
  else fprintf(stderr, "[sim] could not write %s\n", o.screenshot.c_str());
}

static bool timeUp() { return options().runMs >= 0 && nowMicros() / 1000 >= (uint64_t)options().runMs; }

[[noreturn]] void quitSim(int code) {
  saveScreenshot();
  fflush(stdout);
  fflush(stderr);
  std::_Exit(code);  // the sketch thread is still inside loop(); don't wait for it
}
bool simTimeUp() { return timeUp(); }

static int runHeadless() {
  while (!timeUp()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
  quitSim(0);
}

static int runConsole() {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (GetConsoleMode(hOut, &mode)) SetConsoleMode(hOut, mode | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */);
#endif
  uint64_t lastFrame = ~0ULL;
  int lastHeight = -1;
  for (;;) {
    if (timeUp()) quitSim(0);
    Snapshot s = snapshot();
    if (s.frameCounter != lastFrame) {
      lastFrame = s.frameCounter;
      Layout l = computeLayout(s);
      if (l.heightLeds != lastHeight) {
        // Matrix on top, Serial output scrolls in the region below it.
        lastHeight = l.heightLeds;
        printf("\x1b[2J\x1b[%d;r\x1b[%d;1H", lastHeight + 2, lastHeight + 2);
      }
      std::string f = "\x1b[s\x1b[H" + consoleFrame(s, l, true) + "\x1b[u";
      fwrite(f.data(), 1, f.size(), stdout);
      fflush(stdout);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }
}

int main(int argc, char **argv) {
  std::vector<std::string> args = splitArgs(sim_sketch_args ? sim_sketch_args : "");
  for (int i = 1; i < argc; i++) args.push_back(argv[i]);
  if (!parseArgs(args)) return 2;
  Options &o = options();
  if (!o.headless && !o.console && !haveSdl()) {
    fprintf(stderr, "[sim] built without SDL2 — drawing in the terminal instead.\n");
    o.console = true;
  }

  // Serial input: lines typed into the terminal.
  if (!o.headless) {
    std::thread([] {
      char buf[512];
      while (fgets(buf, sizeof buf, stdin)) sim::feedSerial(buf, strlen(buf));
    }).detach();
  }

  std::thread([] {
    setup();
    for (;;) {
      loop();
      yield();
    }
  }).detach();

  if (o.headless) return runHeadless();
  if (o.console) return runConsole();
  return runSdl();
}
