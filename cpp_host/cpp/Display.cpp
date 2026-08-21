#include "Display.h"

#include <SDL.h>
#include <string.h>
#include <xpui_fui.h>

#include <algorithm>
#include <cstdio>

namespace {

// The frame the framework asked for, and how many times it has asked.
//
// File scope rather than members because both present mechanisms are plain C
// functions with no argument to carry a `this` in. One display per process is
// the whole design here; a firmware has one panel too.
bool g_updateRequested = false;
uint32_t g_presentsRequested = 0;

void notePresentRequest() {
  ++g_presentsRequested;
  g_updateRequested = true;
}

// The hook, installed by `xpui_fui_set_present`. Preferred, and what a
// firmware should copy.
void presentHook() { notePresentRequest(); }

// A BMP row is padded to a multiple of four bytes.
size_t bmpStride(const int width) { return (static_cast<size_t>(width) * 3 + 3) & ~static_cast<size_t>(3); }

void put32(uint8_t* out, const uint32_t value) {
  out[0] = static_cast<uint8_t>(value & 0xFF);
  out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  out[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  out[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

}  // namespace

// The strong definition of the shim's weak symbol.
//
// `xpui_fui.cpp` declares this `__attribute__((weak))` and defines it as a
// no-op; a definition here is meant to win. Whether it does is the linker's
// business and differs between object formats, which is why the hook above
// exists and why `--weak-present` runs the host through this path on purpose.
//
// It must NOT blit: see `Display::takeUpdateRequest`.
extern "C" void xpui_fui_present(void) { notePresentRequest(); }

namespace xpui_host {

Display::Display(const int width, const int height)
    : width_(width),
      height_(height),
      stride_(static_cast<size_t>((width + 7) / 8)),
      // 0xFF is white on this panel, so an unpainted frame is blank paper
      // rather than a solid black rectangle.
      framebuffer_(stride_ * static_cast<size_t>(height), 0xFF),
      pixels_(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, 0) {}

Display::~Display() {
  if (texture_) SDL_DestroyTexture(texture_);
  if (renderer_) SDL_DestroyRenderer(renderer_);
  if (window_) SDL_DestroyWindow(window_);
}

void Display::attach(const bool useHook) {
  xpui_fui_attach(framebuffer_.data(), width_, height_);
  // Passing null restores the weak default, which this file overrides. Set
  // explicitly either way so the choice is visible rather than implied.
  xpui_fui_set_present(useHook ? &presentHook : nullptr);
}

bool Display::openWindow(const int scale, const char* title) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return false;
  }

  window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width_ * scale, height_ * scale,
                             SDL_WINDOW_SHOWN);
  if (!window_) {
    std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
    return false;
  }

  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer_) {
    std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
    return false;
  }

  texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, width_, height_);
  if (!texture_) {
    std::fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
    return false;
  }
  return true;
}

void Display::blit() {
  for (int y = 0; y < height_; ++y) {
    const uint8_t* row = &framebuffer_[static_cast<size_t>(y) * stride_];
    uint8_t* out = &pixels_[static_cast<size_t>(y) * static_cast<size_t>(width_) * 3];
    for (int x = 0; x < width_; ++x) {
      // MSB first, and a SET bit is white.
      const bool white = (row[x / 8] >> (7 - (x % 8))) & 1;
      const uint8_t value = white ? 0xFF : 0x00;
      out[x * 3 + 0] = value;
      out[x * 3 + 1] = value;
      out[x * 3 + 2] = value;
    }
  }
  ++framesBlitted_;

  if (!texture_ || !renderer_) return;
  SDL_UpdateTexture(texture_, nullptr, pixels_.data(), width_ * 3);
  SDL_RenderClear(renderer_);
  SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
  SDL_RenderPresent(renderer_);
}

bool Display::writeBmp(const char* path, const Rect crop) const {
  // Clipped rather than rejected: a caller asking for a band that runs off the
  // panel gets what is there, and a caller asking for nothing gets everything.
  const int left = crop.empty() ? 0 : std::max(0, std::min(crop.x, width_));
  const int top = crop.empty() ? 0 : std::max(0, std::min(crop.y, height_));
  const int width = crop.empty() ? width_ : std::min(crop.width, width_ - left);
  const int height = crop.empty() ? height_ : std::min(crop.height, height_ - top);
  if (width <= 0 || height <= 0) return false;

  const size_t rowBytes = bmpStride(width);
  const uint32_t imageBytes = static_cast<uint32_t>(rowBytes * static_cast<size_t>(height));

  uint8_t header[54] = {};
  header[0] = 'B';
  header[1] = 'M';
  put32(&header[2], 54 + imageBytes);
  put32(&header[10], 54);  // pixels start here
  put32(&header[14], 40);  // DIB header size
  put32(&header[18], static_cast<uint32_t>(width));
  put32(&header[22], static_cast<uint32_t>(height));
  header[26] = 1;   // planes
  header[28] = 24;  // bits per pixel
  put32(&header[34], imageBytes);

  std::FILE* file = std::fopen(path, "wb");
  if (!file) return false;

  bool ok = std::fwrite(header, sizeof(header), 1, file) == 1;

  // Bottom-up, and BGR rather than RGB: both are the format's, not ours.
  std::vector<uint8_t> row(rowBytes, 0);
  for (int y = top + height - 1; ok && y >= top; --y) {
    const uint8_t* in = &pixels_[static_cast<size_t>(y) * static_cast<size_t>(width_) * 3];
    for (int x = 0; x < width; ++x) {
      const uint8_t* pixel = &in[(left + x) * 3];
      row[x * 3 + 0] = pixel[2];
      row[x * 3 + 1] = pixel[1];
      row[x * 3 + 2] = pixel[0];
    }
    ok = std::fwrite(row.data(), row.size(), 1, file) == 1;
  }

  return std::fclose(file) == 0 && ok;
}

// Ink pixels inside the panel.
//
// Bounded by `x < width_` rather than by the byte, so the padding bits at the
// end of a row whose width is not a multiple of eight are never counted. They
// are initialised to paper and nothing draws them, so they would only ever
// dilute the figure — but the bound is what makes that true rather than
// incidental.
size_t Display::inkPixels() const {
  size_t ink = 0;
  for (int y = 0; y < height_; ++y) {
    const uint8_t* row = &framebuffer_[static_cast<size_t>(y) * stride_];
    for (int x = 0; x < width_; ++x) {
      const bool white = (row[x / 8] >> (7 - (x % 8))) & 1;
      if (!white) ++ink;
    }
  }
  return ink;
}

bool Display::isMixed() const {
  const size_t ink = inkPixels();
  return ink > 0 && ink < static_cast<size_t>(width_) * static_cast<size_t>(height_);
}

int Display::inkPercent() const {
  const size_t pixels = static_cast<size_t>(width_) * static_cast<size_t>(height_);
  if (pixels == 0) return 0;
  return static_cast<int>(inkPixels() * 100 / pixels);
}

bool Display::takeUpdateRequest() {
  const bool requested = g_updateRequested;
  g_updateRequested = false;
  return requested;
}

uint32_t Display::presentsRequested() { return g_presentsRequested; }

}  // namespace xpui_host
