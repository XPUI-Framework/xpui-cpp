// The panel: a 1-bit framebuffer, and whatever shows it.
//
// This is the piece a firmware replaces with a real driver. It owns the
// framebuffer the shim draws into, converts it to pixels, and hands those to
// an SDL window — or to nothing at all, when the host is running headless.

#pragma once

#include <stdint.h>

#include <cstddef>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace xpui_host {

class Display {
 public:
  Display(int width, int height);
  ~Display();

  Display(const Display&) = delete;
  Display& operator=(const Display&) = delete;

  // Points the shim at this framebuffer, and says how a finished frame gets
  // to the panel.
  //
  // `useHook` picks which of the two mechanisms is exercised: the pointer set
  // by `xpui_fui_set_present`, or the weak `xpui_fui_present` this file
  // overrides. Both end in the same place. The hook is the one to copy — an
  // override that loses to the weak definition fails silently, as a panel that
  // never updates — and the other exists so a test can prove the override
  // works on this linker rather than assuming it.
  void attach(bool useHook);

  // Opens a window `scale` times the panel size. False when SDL will not.
  bool openWindow(int scale, const char* title);

  // A rectangle of the panel. Any non-positive extent means the whole of it,
  // which is what a default-constructed one has.
  struct Rect {
    int x;
    int y;
    int width;
    int height;

    bool empty() const { return width <= 0 || height <= 0; }
  };

  // Converts the framebuffer to pixels, and shows them if there is a window.
  void blit();

  // Writes the last blitted pixels as a 24-bit bottom-up BMP.
  //
  // Hand-rolled rather than `SDL_SaveBMP`: headless must not need SDL to have
  // been initialised, and the format is short.
  //
  // A non-empty `crop` writes that rectangle only, clipped to the panel. What
  // it is for: comparing two runs that differ in one band. A whole frame is the
  // wrong unit for that — the hint bar changes whenever the keys change
  // meaning, so two frames always differ somewhere and a whole-frame comparison
  // passes without ever looking at the control it was asked about.
  bool writeBmp(const char* path, Rect crop = Rect{}) const;

  // Whether the framebuffer holds both ink and paper.
  //
  // The assertion that catches a build which links perfectly and draws
  // nothing — `xpui-fui/testing` reaching the archive, or a shim that was
  // never attached. Neither shows up as an error anywhere else.
  bool isMixed() const;

  // What percentage of the panel is ink.
  //
  // The signal that says an overlay was painted. A scrim is defined as ink on
  // one checkerboard parity of everything behind it, so a scrimmed panel is
  // about half ink by construction, where an ordinary screen of type and rules
  // is a small fraction of that. It is a relationship rather than a magic
  // number, which is why a test can assert on it.
  int inkPercent() const;

  int width() const { return width_; }
  int height() const { return height_; }
  uint32_t framesBlitted() const { return framesBlitted_; }

  // Set by whichever present mechanism is in use, and cleared by the caller.
  //
  // A repaint is REQUESTED from inside the input phase, before anything has
  // been painted, so presenting there would push the previous frame. The flag
  // is how the request survives to the end of the frame it belongs to.
  static bool takeUpdateRequest();
  static uint32_t presentsRequested();

 private:
  size_t inkPixels() const;

  int width_;
  int height_;
  // Bytes per framebuffer row: 1 bit per pixel, MSB first, a SET BIT IS WHITE.
  size_t stride_;
  std::vector<uint8_t> framebuffer_;
  // The same image as RGB triples, which is what both SDL and a BMP want.
  std::vector<uint8_t> pixels_;
  uint32_t framesBlitted_ = 0;

  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
};

}  // namespace xpui_host
