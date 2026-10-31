// render.cpp — turns MAX7219 register state into a picture of the LED modules.
#include "sim.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace sim {

// The same 0/90/180/270 transform Max72xxPanel::setRotation(display, r) applies.
// A module physically turned by r shows chip pixel f_r(x, y) at its upright position (x, y),
// so a sketch that calls setRotation(display, r) for it gets an upright picture again.
static void rotXY(int r, int x, int y, int &cx, int &cy) {
  if (r >= 2) x = 7 - x;
  if (r == 1 || r == 2) y = 7 - y;
  if (r & 1) std::swap(x, y);
  cx = x;
  cy = y;
}

Layout computeLayout(const Snapshot &s) {
  const Options &o = options();
  Layout l;
  struct Placed { int chain, n, w, h, y0; };
  std::vector<Placed> placed;
  int y = 0;
  for (size_t c = 0; c < s.chains.size(); c++) {
    int n = (int)s.chains[c].chips.size();
    if (!n) continue;
    int w = o.modulesX > 0 ? o.modulesX : n;
    int h = o.modulesX > 0 ? (o.modulesY > 0 ? o.modulesY : (n + w - 1) / w) : 1;
    if (!placed.empty()) y += 2;  // gap between separate chains
    placed.push_back({(int)c, n, w, h, y});
    y += h * 8;
    l.widthLeds = std::max(l.widthLeds, w * 8);
  }
  l.heightLeds = y;
  if (!l.widthLeds || !l.heightLeds) {  // nothing has been sent yet: show one dark module
    l.widthLeds = l.heightLeds = 8;
    l.map.assign(64, {-1, 0, 0, 0});
    l.modules.push_back({0, 0, -1, 0});
    return l;
  }
  l.map.assign((size_t)l.widthLeds * l.heightLeds, {-1, 0, 0, 0});
  int rot = ((o.moduleRotation % 4) + 4) % 4;
  for (auto &p : placed) {
    for (int chip = 0; chip < p.w * p.h; chip++) {
      int row = chip / p.w, col = chip % p.w;
      bool reverse = o.rightToLeft;
      if (o.serpentine && (row & 1)) reverse = !reverse;
      if (reverse) col = p.w - 1 - col;
      int mx = col * 8, my = p.y0 + row * 8;
      l.modules.push_back({mx, my, chip < p.n ? p.chain : -1, chip});
      if (chip >= p.n) continue;
      for (int yy = 0; yy < 8; yy++)
        for (int xx = 0; xx < 8; xx++) {
          int cx, cy;
          rotXY(rot, xx, yy, cx, cy);
          l.map[(size_t)(my + yy) * l.widthLeds + mx + xx] = {p.chain, chip, cx, cy};
        }
    }
  }
  return l;
}

// -1 = off, otherwise the intensity it is lit at (0..15)
static int ledLevel(const Snapshot &s, const Layout::Px &px) {
  if (px.chain < 0 || px.chain >= (int)s.chains.size()) return -1;
  const Chain &ch = s.chains[px.chain];
  if (px.chip >= (int)ch.chips.size()) return -1;
  const Chip &c = ch.chips[px.chip];
  if (c.displayTest) return 15;
  if (c.shutdown || px.digit > c.scanLimit) return -1;
  return ((c.digit[px.digit] >> px.bit) & 1) ? c.intensity : -1;
}

static const uint32_t kBackground = 0x1B1D21, kBoard = 0x0E0F12, kSeam = 0x050506;

static inline uint32_t mix(uint32_t a, uint32_t b, double t) {
  t = std::max(0.0, std::min(1.0, t));
  int r = (int)((a >> 16 & 255) * (1 - t) + (b >> 16 & 255) * t);
  int g = (int)((a >> 8 & 255) * (1 - t) + (b >> 8 & 255) * t);
  int bl = (int)((a & 255) * (1 - t) + (b & 255) * t);
  return (uint32_t)(r << 16 | g << 8 | bl);
}
static inline uint32_t scale(uint32_t c, double k) { return mix(0, c, k); }

// One pre-shaded sprite per LED state (off + 16 intensities) for the current pitch.
struct SpriteSet {
  int pitch = 0;
  uint32_t color = 0;
  std::vector<uint32_t> px[17];
};

static const SpriteSet &sprites(int p) {
  static SpriteSet set;
  uint32_t color = options().onColor;
  if (set.pitch == p && set.color == color) return set;
  set.pitch = p;
  set.color = color;
  double half = p / 2.0, R = p * 0.38;
  uint32_t offBody = mix(0x202124, color, 0.10);
  for (int st = 0; st < 17; st++) {
    std::vector<uint32_t> &v = set.px[st];
    v.assign((size_t)p * p, kBoard);
    // Perceived brightness for MAX7219 duty cycles (1/32 .. 31/32).
    double b = st == 0 ? 0 : 0.30 + 0.70 * std::sqrt((2.0 * (st - 1) + 1) / 31.0);
    for (int y = 0; y < p; y++)
      for (int x = 0; x < p; x++) {
        double dx = x + 0.5 - half, dy = y + 0.5 - half, d = std::sqrt(dx * dx + dy * dy);
        uint32_t out = kBoard;
        if (st > 0 && d > R) {
          double t = (d - R) / (half * 1.25 - R);
          if (t < 1) out = mix(kBoard, color, 0.45 * b * (1 - t) * (1 - t));
        }
        double cover = std::max(0.0, std::min(1.0, R - d + 0.5));  // anti-aliased edge
        if (cover > 0) {
          uint32_t body;
          if (st == 0) {
            body = mix(offBody, 0x3A3B40, std::max(0.0, 0.35 - d / R) * 0.6);  // faint lens highlight
          } else {
            body = scale(color, b);
            double hot = std::max(0.0, 1 - d / (R * 0.75));
            body = mix(body, 0xFFFFFF, 0.45 * b * hot * hot);
          }
          out = mix(out, body, cover);
        }
        v[(size_t)y * p + x] = out;
      }
  }
  return set;
}

static void fitPitch(const Layout &l, int w, int h, int &p, int &ox, int &oy) {
  p = std::max(2, std::min(w / (l.widthLeds + 2), h / (l.heightLeds + 2)));
  ox = (w - p * l.widthLeds) / 2;
  oy = (h - p * l.heightLeds) / 2;
}

void preferredSize(const Layout &l, int &w, int &h) {
  int p = options().ledPitch;
  // Keep big chains on screen: shrink the pitch until it fits ~1600x900.
  while (p > 6 && ((l.widthLeds + 2) * p > 1600 || (l.heightLeds + 2) * p > 900)) p--;
  w = (l.widthLeds + 2) * p;
  h = (l.heightLeds + 2) * p;
}

void render(const Snapshot &s, const Layout &l, uint32_t *pixels, int w, int h) {
  std::fill(pixels, pixels + (size_t)w * h, kBackground);
  int p, ox, oy;
  fitPitch(l, w, h, p, ox, oy);
  const SpriteSet &sp = sprites(p);

  auto fillRect = [&](int x0, int y0, int rw, int rh, uint32_t c) {
    for (int y = std::max(0, y0); y < std::min(h, y0 + rh); y++)
      for (int x = std::max(0, x0); x < std::min(w, x0 + rw); x++) pixels[(size_t)y * w + x] = c;
  };
  // module boards with a faint drop shadow
  for (auto &m : l.modules) {
    int x = ox + m.x * p, y = oy + m.y * p;
    fillRect(x + p / 6, y + p / 6, 8 * p, 8 * p, 0x111215);
  }
  for (auto &m : l.modules) fillRect(ox + m.x * p, oy + m.y * p, 8 * p, 8 * p, kBoard);

  for (int r = 0; r < l.heightLeds; r++)
    for (int c = 0; c < l.widthLeds; c++) {
      const Layout::Px &px = l.map[(size_t)r * l.widthLeds + c];
      bool inModule = false;
      for (auto &m : l.modules)
        if (c >= m.x && c < m.x + 8 && r >= m.y && r < m.y + 8) { inModule = true; break; }
      if (!inModule) continue;
      int lvl = ledLevel(s, px);
      const std::vector<uint32_t> &spr = sp.px[lvl < 0 ? 0 : lvl + 1];
      int x0 = ox + c * p, y0 = oy + r * p;
      for (int y = 0; y < p; y++) {
        int yy = y0 + y;
        if (yy < 0 || yy >= h) continue;
        for (int x = 0; x < p; x++) {
          int xx = x0 + x;
          if (xx < 0 || xx >= w) continue;
          uint32_t v = spr[(size_t)y * p + x];
          uint32_t &dst = pixels[(size_t)yy * w + xx];
          // glows add up where neighbouring LEDs overlap their cells' edges
          if (v != kBoard || dst == kBackground) dst = v;
        }
      }
    }
  // seams between modules
  for (auto &m : l.modules) {
    int x = ox + m.x * p, y = oy + m.y * p, s8 = 8 * p;
    fillRect(x, y, s8, 1, kSeam);
    fillRect(x, y + s8 - 1, s8, 1, kSeam);
    fillRect(x, y, 1, s8, kSeam);
    fillRect(x + s8 - 1, y, 1, s8, kSeam);
  }
}

bool ledAt(const Layout &l, int w, int h, int mx, int my, int &col, int &row) {
  int p, ox, oy;
  fitPitch(l, w, h, p, ox, oy);
  if (mx < ox || my < oy) return false;
  col = (mx - ox) / p;
  row = (my - oy) / p;
  return col < l.widthLeds && row < l.heightLeds && l.map[(size_t)row * l.widthLeds + col].chain >= 0;
}

bool writeBMP(const std::string &path, const uint32_t *pixels, int w, int h) {
  FILE *f = fopen(path.c_str(), "wb");
  if (!f) return false;
  int rowBytes = (w * 3 + 3) & ~3;
  uint32_t dataSize = (uint32_t)rowBytes * h, fileSize = 54 + dataSize;
  unsigned char hdr[54] = {'B', 'M'};
  auto put32 = [&](int off, uint32_t v) { for (int i = 0; i < 4; i++) hdr[off + i] = (unsigned char)(v >> (8 * i)); };
  put32(2, fileSize);
  put32(10, 54);
  put32(14, 40);
  put32(18, (uint32_t)w);
  put32(22, (uint32_t)h);
  hdr[26] = 1;
  hdr[28] = 24;
  put32(34, dataSize);
  fwrite(hdr, 1, 54, f);
  std::vector<unsigned char> row((size_t)rowBytes, 0);
  for (int y = h - 1; y >= 0; y--) {
    for (int x = 0; x < w; x++) {
      uint32_t c = pixels[(size_t)y * w + x];
      row[x * 3] = (unsigned char)(c & 255);
      row[x * 3 + 1] = (unsigned char)(c >> 8 & 255);
      row[x * 3 + 2] = (unsigned char)(c >> 16 & 255);
    }
    fwrite(row.data(), 1, row.size(), f);
  }
  fclose(f);
  return true;
}

std::string consoleFrame(const Snapshot &s, const Layout &l, bool color) {
  std::string out;
  uint32_t on = options().onColor;
  char esc[48];
  for (int r = 0; r < l.heightLeds; r++) {
    for (int c = 0; c < l.widthLeds; c++) {
      const Layout::Px &px = l.map[(size_t)r * l.widthLeds + c];
      if (px.chain < 0) { out += "  "; continue; }
      int lvl = ledLevel(s, px);
      if (color) {
        uint32_t k = lvl < 0 ? 0x3A2A2A : scale(on, 0.45 + 0.55 * lvl / 15.0);
        snprintf(esc, sizeof esc, "\x1b[38;2;%u;%u;%um", k >> 16 & 255, k >> 8 & 255, k & 255);
        out += esc;
      }
      out += lvl < 0 ? "\xC2\xB7 " : "\xE2\x97\x8F ";  // "· " or "● "
    }
    if (color) out += "\x1b[0m";
    out += "\x1b[K\n";
  }
  return out;
}

}  // namespace sim
