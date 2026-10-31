// frontend_sdl.cpp — the simulator window.
#include "sim.h"
#include <cstdio>
#include <string>

[[noreturn]] void quitSim(int code);
bool simTimeUp();

#ifdef SIM_HAVE_SDL
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <algorithm>
#include <cctype>

namespace sim {

bool haveSdl() { return true; }

static std::string keyName(SDL_Keycode k) {
  std::string n = SDL_GetKeyName(k);
  for (auto &c : n) c = (char)tolower((unsigned char)c);
  if (n == "return" || n == "keypad enter") return "enter";
  if (n == "left shift" || n == "right shift") return "shift";
  if (n == "left ctrl" || n == "right ctrl") return "ctrl";
  if (n == "left alt" || n == "right alt") return "alt";
  return n;
}

int runSdl() {
  SDL_SetMainReady();
  SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "[sim] SDL_Init failed: %s — try --console\n", SDL_GetError());
    return 1;
  }
  Snapshot snap = snapshot();
  Layout layout = computeLayout(snap);
  int ww, wh;
  preferredSize(layout, ww, wh);
  SDL_Window *win = SDL_CreateWindow("MAX7219 simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!win) {
    fprintf(stderr, "[sim] could not open a window: %s — try --console\n", SDL_GetError());
    return 1;
  }
  SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  SDL_Texture *tex = nullptr;
  int texW = 0, texH = 0;
  std::vector<uint32_t> pixels;

  int layoutW = layout.widthLeds, layoutH = layout.heightLeds;
  bool userResized = false;
  uint64_t drawnFrame = ~0ULL, fpsBase = 0;
  Uint32 fpsTick = SDL_GetTicks();
  double fps = 0;
  int hoverCol = -1, hoverRow = -1;
  std::string lastTitle;
  bool dirty = true;

  for (;;) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT: quitSim(0);
        case SDL_KEYDOWN:
          if (e.key.keysym.sym == SDLK_ESCAPE) quitSim(0);
          if (!e.key.repeat) setKeyDown(keyName(e.key.keysym.sym), true);
          break;
        case SDL_KEYUP: setKeyDown(keyName(e.key.keysym.sym), false); break;
        case SDL_WINDOWEVENT:
          if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) { dirty = true; }
          if (e.window.event == SDL_WINDOWEVENT_RESIZED) userResized = true;
          if (e.window.event == SDL_WINDOWEVENT_LEAVE) hoverCol = hoverRow = -1;
          if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
            for (auto k : {"space", "up", "down", "left", "right", "enter", "shift", "ctrl"}) setKeyDown(k, false);
          break;
        case SDL_MOUSEMOTION: {
          int ow, oh, lw, lh;
          SDL_GetRendererOutputSize(ren, &ow, &oh);
          SDL_GetWindowSize(win, &lw, &lh);
          int mx = e.motion.x * ow / std::max(1, lw), my = e.motion.y * oh / std::max(1, lh);
          int c, r;
          if (ledAt(layout, ow, oh, mx, my, c, r)) { hoverCol = c; hoverRow = r; }
          else hoverCol = hoverRow = -1;
          break;
        }
      }
    }
    if (simTimeUp()) quitSim(0);

    snap = snapshot();
    if (snap.frameCounter != drawnFrame) dirty = true;
    layout = computeLayout(snap);
    if (layout.widthLeds != layoutW || layout.heightLeds != layoutH) {
      layoutW = layout.widthLeds;
      layoutH = layout.heightLeds;
      if (!userResized) {
        preferredSize(layout, ww, wh);
        SDL_SetWindowSize(win, ww, wh);
        SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
      }
      dirty = true;
    }

    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    if (ow != texW || oh != texH) {
      if (tex) SDL_DestroyTexture(tex);
      tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ow, oh);
      texW = ow;
      texH = oh;
      pixels.assign((size_t)ow * oh, 0);
      dirty = true;
    }
    if (dirty && tex) {
      render(snap, layout, pixels.data(), texW, texH);
      SDL_UpdateTexture(tex, nullptr, pixels.data(), texW * 4);
      drawnFrame = snap.frameCounter;
      dirty = false;
    }
    SDL_RenderClear(ren);
    if (tex) SDL_RenderCopy(ren, tex, nullptr, nullptr);
    SDL_RenderPresent(ren);

    // Title bar: status and what's under the mouse.
    Uint32 now = SDL_GetTicks();
    if (now - fpsTick >= 1000) {
      fps = (double)(snap.frameCounter - fpsBase) * 1000.0 / (now - fpsTick);
      fpsBase = snap.frameCounter;
      fpsTick = now;
    }
    char title[256];
    int chips = 0;
    for (auto &c : snap.chains) chips += (int)c.chips.size();
    if (!chips) {
      snprintf(title, sizeof title, "MAX7219 simulator — waiting for the sketch to send data…");
    } else {
      const Chip &c0 = snap.chains[0].chips[0];
      int n = snprintf(title, sizeof title, "MAX7219 sim — %d module%s · intensity %d%s · %.0f updates/s", chips,
                       chips == 1 ? "" : "s", c0.intensity, c0.shutdown ? " · SHUTDOWN" : (c0.displayTest ? " · TEST" : ""), fps);
      if (options().speed != 1.0) n += snprintf(title + n, sizeof title - n, " · speed ×%g", options().speed);
      if (hoverCol >= 0 && hoverRow < layout.heightLeds) {
        const Layout::Px &px = layout.map[(size_t)hoverRow * layout.widthLeds + hoverCol];
        snprintf(title + n, sizeof title - n, "   |   x=%d y=%d · chip %d digit %d bit %d", hoverCol, hoverRow, px.chip,
                 px.digit, px.bit);
      }
    }
    if (lastTitle != title) {
      SDL_SetWindowTitle(win, title);
      lastTitle = title;
    }
    SDL_Delay(8);
  }
}

}  // namespace sim

#else  // no SDL2: the terminal renderer is used instead

namespace sim {
bool haveSdl() { return false; }
int runSdl() { return 1; }
}  // namespace sim

#endif
