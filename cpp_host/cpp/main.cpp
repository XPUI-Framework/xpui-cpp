// A C++ application that runs xpui screens.
//
// The whole point of this file is that everything below it has been compiled,
// linked and executed: `xpui_fui.cpp` is 882 lines that a firmware is supposed
// to add to its build, and until this example existed nothing had ever run
// them. `--selftest` and the `--expect-*` flags turn that into a pass or a
// fail — never a regex over the summary line, because ctest ignores a test's
// exit code the moment PASS_REGULAR_EXPRESSION is set.
//
//   xpui-host                                  a window, 480x800
//   xpui-host --headless --frames 30 --selftest
//   xpui-host --keys confirm,back --frames 20  a scripted run
//
// Arrows move focus, Enter opens, Backspace goes back, H is the home gesture,
// Q or Escape quits — the same keys `xpui-simulator` uses, on purpose.

#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include <xpui_app.h>
#include <xpui_fui.h>
#include <xpui_host.h>

#include <cstdio>
#include <vector>

#include "Display.h"
#include "Input.h"
#include "ScreenStack.h"
#include "internal.h"

namespace {

using xpui_host::Display;
using xpui_host::Input;
using xpui_host::ScreenStack;

struct Options {
  bool headless = false;
  bool selftest = false;
  // Check the string table's contract and exit, without opening anything.
  bool checkI18n = false;
  // Whether to install the present hook, or fall through to the weak symbol
  // this binary overrides. See `Display::attach`.
  bool useHook = true;
  // Answer "no Left/Right pair" to the framework, as a badge-style board does.
  // The keyboard has arrow keys, so without this no run can reach the mode a
  // value control opens on Confirm.
  bool noPair = false;
  int width = 480;
  int height = 800;
  int scale = 1;
  // Negative is unbounded. Headless gets a default so a run can never hang a
  // CI job waiting for a window nobody will close.
  long frames = -1;
  const char* out = nullptr;
  // What `--out` writes, when only a band of the panel is wanted. All zero is
  // the whole panel.
  Display::Rect crop{0, 0, 0, 0};
  // One tap per frame, in order. `kHomeGesture` for the gesture.
  std::vector<uint8_t> keys;

  // What the run has to end up with, or exit non-zero. Negative is "do not
  // check".
  //
  // These are flags rather than something a test greps out of the summary
  // line, and that is not a style choice: **ctest ignores the exit code
  // entirely when PASS_REGULAR_EXPRESSION is set**, so a test written that way
  // passes a run whose `--selftest` failed. Everything a test asserts has to
  // reach the exit code.
  long expectDepth = -1;
  long expectMaxDepth = -1;
  long expectInkAtLeast = -1;
  long expectInkAtMost = -1;
};

constexpr long kHeadlessDefaultFrames = 120;

void usage() {
  std::fprintf(stderr,
               "usage: xpui-host [options]\n"
               "  --headless          no window; --frames defaults to %ld\n"
               "  --frames N          stop after N frames\n"
               "  --keys a,b,c        one scripted press per frame\n"
               "                      (back confirm left right up down\n"
               "                       pageback pageforward home)\n"
               "  --out PATH          write the last frame as a BMP\n"
               "  --crop X,Y,W,H      write only that band of it\n"
               "  --selftest          exit non-zero unless the frame drew\n"
               "  --check-i18n        check xpui_host_tr's contract and exit\n"
               "  --expect-depth N    exit non-zero unless N screens are left\n"
               "  --expect-max-depth N   ... unless the stack ever reached N\n"
               "  --expect-ink N      ... unless at least N%% of the panel is\n"
               "                      ink, which is what a scrim makes it\n"
               "  --expect-ink-at-most N  ... unless at most N%% is\n"
               "  --size WxH          panel size, default 480x800\n"
               "  --scale N           window pixels per panel pixel\n"
               "  --weak-present      push frames through the weak symbol\n"
               "                      instead of the xpui_fui_set_present hook\n"
               "  --no-pair           answer that this device has no Left/Right keys\n",
               kHeadlessDefaultFrames);
}

bool parseSize(const char* text, Options& options) {
  const char* cross = strchr(text, 'x');
  if (!cross) return false;
  options.width = atoi(text);
  options.height = atoi(cross + 1);
  return options.width > 0 && options.height > 0;
}

bool parseCrop(const char* text, Options& options) {
  return std::sscanf(text, "%d,%d,%d,%d", &options.crop.x, &options.crop.y, &options.crop.width,
                     &options.crop.height) == 4;
}

bool parseKeys(char* text, Options& options) {
  for (char* name = strtok(text, ","); name; name = strtok(nullptr, ",")) {
    uint8_t button = 0;
    if (!xpui_host::buttonForName(name, button)) {
      std::fprintf(stderr, "unknown key: %s\n", name);
      return false;
    }
    options.keys.push_back(button);
  }
  return true;
}

// An `--expect-*` value, which may not be negative.
//
// Negative is the "not asked for" sentinel, so accepting one from the command
// line would let `--expect-depth -1` turn its own check off and exit 0 — the
// one failure mode these flags exist to remove. A non-number is already loud:
// `atol` gives 0, and zero screens is a mismatch.
bool parseExpectation(const char* text, long& out) {
  const long value = atol(text);
  if (value < 0) {
    std::fprintf(stderr, "an --expect-* value cannot be negative: %s\n", text);
    return false;
  }
  out = value;
  return true;
}

// Hand-parsed. A handful of flags in an example whose point is the C ABI does
// not justify a command-line dependency.
bool parse(const int argc, char** argv, Options& options) {
  for (int index = 1; index < argc; ++index) {
    const char* arg = argv[index];
    const bool hasValue = index + 1 < argc;

    if (strcmp(arg, "--headless") == 0) {
      options.headless = true;
    } else if (strcmp(arg, "--selftest") == 0) {
      options.selftest = true;
    } else if (strcmp(arg, "--check-i18n") == 0) {
      options.checkI18n = true;
    } else if (strcmp(arg, "--weak-present") == 0) {
      options.useHook = false;
    } else if (strcmp(arg, "--no-pair") == 0) {
      options.noPair = true;
    } else if (strcmp(arg, "--frames") == 0 && hasValue) {
      options.frames = atol(argv[++index]);
    } else if (strcmp(arg, "--expect-depth") == 0 && hasValue) {
      if (!parseExpectation(argv[++index], options.expectDepth)) return false;
    } else if (strcmp(arg, "--expect-max-depth") == 0 && hasValue) {
      if (!parseExpectation(argv[++index], options.expectMaxDepth)) return false;
    } else if (strcmp(arg, "--expect-ink") == 0 && hasValue) {
      if (!parseExpectation(argv[++index], options.expectInkAtLeast)) return false;
    } else if (strcmp(arg, "--expect-ink-at-most") == 0 && hasValue) {
      if (!parseExpectation(argv[++index], options.expectInkAtMost)) return false;
    } else if (strcmp(arg, "--scale") == 0 && hasValue) {
      options.scale = atoi(argv[++index]);
    } else if (strcmp(arg, "--out") == 0 && hasValue) {
      options.out = argv[++index];
    } else if (strcmp(arg, "--crop") == 0 && hasValue) {
      if (!parseCrop(argv[++index], options)) return false;
    } else if (strcmp(arg, "--size") == 0 && hasValue) {
      if (!parseSize(argv[++index], options)) return false;
    } else if (strcmp(arg, "--keys") == 0 && hasValue) {
      if (!parseKeys(argv[++index], options)) return false;
    } else {
      std::fprintf(stderr, "unknown argument: %s\n", arg);
      return false;
    }
  }

  if (options.headless && options.frames < 0) options.frames = kHeadlessDefaultFrames;
  if (options.scale < 1) options.scale = 1;
  return true;
}

// Everything the run has to show for itself, on one line. For a person
// reading the output; nothing asserts on it.
void report(const Options& options, const Display& display, const ScreenStack& stack, const long frames) {
  std::printf("xpui-host: frames=%ld presents=%u blits=%u depth=%zu maxdepth=%zu mixed=%d ink=%d%% present=%s\n",
              frames, Display::presentsRequested(), display.framesBlitted(), stack.depth(), stack.maxDepth(),
              display.isMixed() ? 1 : 0, display.inkPercent(), options.useHook ? "hook" : "weak");
}

// What `xpui_host_tr` promises, checked.
//
// Two halves, and the second is a **soundness** requirement rather than a
// nicety. A key that is in the table comes back as a pointer into the table.
// A key that is not comes back as *the caller's own pointer* — which is how a
// missing string shows up on the panel as the key itself instead of a blank
// row, and which is why the Rust side takes `&'static CStr` and can hand the
// result back as a `&'static str`. Return a copy, or `""`, and that bound
// stops being justified while everything still compiles and still draws.
//
// Nothing else here exercises a missing key: the one runtime caller looks up
// STR_MENU_TITLE, which is in the table.
int checkI18n() {
  int failures = 0;

  const auto* known = reinterpret_cast<const uint8_t*>("STR_MENU_TITLE");
  const uint8_t* found = xpui_host_tr(known);
  if (found == known) {
    std::fprintf(stderr, "check-i18n: a key that is in the table came back as itself\n");
    ++failures;
  } else if (strcmp(reinterpret_cast<const char*>(found), "xpui on a C++ host") != 0) {
    std::fprintf(stderr, "check-i18n: STR_MENU_TITLE resolved to \"%s\"\n", reinterpret_cast<const char*>(found));
    ++failures;
  }

  const auto* missing = reinterpret_cast<const uint8_t*>("STR_NOT_IN_THE_TABLE");
  if (xpui_host_tr(missing) != missing) {
    std::fprintf(stderr,
                 "check-i18n: an unknown key did not come back as the caller's own\n"
                 "            pointer. Rust hands that result out as a &'static str.\n");
    ++failures;
  }

  if (xpui_host_tr(nullptr) == nullptr) {
    std::fprintf(stderr, "check-i18n: a null key came back null rather than empty\n");
    ++failures;
  }

  if (failures == 0) {
    std::printf("check-i18n: the table's contract holds\n");
  }
  return failures == 0 ? 0 : 1;
}

// The three things that have to be true, or the C ABI is not working.
//
// Each fails independently and says which: a shim that never got a framebuffer
// and a build that quietly linked the test doubles look identical from
// outside, and only the third assertion tells them apart.
int selftest(const Display& display) {
  int failures = 0;

  if (Display::presentsRequested() == 0) {
    std::fprintf(stderr, "selftest: nothing ever asked for the panel to update\n");
    ++failures;
  }
  if (display.framesBlitted() == 0) {
    std::fprintf(stderr, "selftest: no frame was ever blitted\n");
    ++failures;
  }
  if (!display.isMixed()) {
    std::fprintf(stderr, "selftest: the framebuffer is one flat colour - nothing drew\n");
    ++failures;
  }

  return failures;
}

// What a scripted run said it would end up with.
int expectations(const Options& options, const Display& display, const ScreenStack& stack) {
  int failures = 0;

  if (options.expectDepth >= 0 && static_cast<long>(stack.depth()) != options.expectDepth) {
    std::fprintf(stderr, "expected %ld screen(s) left, found %zu\n", options.expectDepth, stack.depth());
    ++failures;
  }
  if (options.expectMaxDepth >= 0 && static_cast<long>(stack.maxDepth()) != options.expectMaxDepth) {
    std::fprintf(stderr, "expected the stack to reach %ld, it reached %zu\n", options.expectMaxDepth, stack.maxDepth());
    ++failures;
  }
  if (options.expectInkAtLeast >= 0 && display.inkPercent() < options.expectInkAtLeast) {
    std::fprintf(stderr, "expected at least %ld%% of the panel to be ink, found %d%%\n", options.expectInkAtLeast,
                 display.inkPercent());
    ++failures;
  }
  if (options.expectInkAtMost >= 0 && display.inkPercent() > options.expectInkAtMost) {
    std::fprintf(stderr, "expected at most %ld%% of the panel to be ink, found %d%%\n", options.expectInkAtMost,
                 display.inkPercent());
    ++failures;
  }

  return failures;
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  if (!parse(argc, argv, options)) {
    usage();
    return 2;
  }

  // Before any window, any framebuffer and any screen: it needs none of them.
  if (options.checkI18n) {
    return checkI18n();
  }

  Display display(options.width, options.height);
  if (!options.headless && !display.openWindow(options.scale, "xpui — C++ host")) {
    return 1;
  }

  // Order matters and nothing checks it: `attach` tells the shim where the
  // panel is, `xpui_app_install` tells the framework where the shim is.
  xpui_host::setHasLeftRightKeys(!options.noPair);
  display.attach(options.useHook);
  xpui_app_install();

  Input input;
  ScreenStack stack;
  xpui_host::g_input = &input;
  xpui_host::g_stack = &stack;

  stack.push(xpui_app_create_menu(), xpui_host_tr(reinterpret_cast<const uint8_t*>("STR_MENU_TITLE")));

  long frame = 0;
  size_t scripted = 0;
  bool running = true;

  while (running) {
    input.beginFrame();

    if (!options.headless) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
          running = false;
        } else if (event.type == SDL_KEYDOWN) {
          const SDL_Keycode key = event.key.keysym.sym;
          if (key == SDLK_q || key == SDLK_ESCAPE) {
            running = false;
          } else {
            input.keyDown(key);
          }
        } else if (event.type == SDL_KEYUP) {
          input.keyUp(event.key.keysym.sym);
        }
      }
    }

    // One scripted press per frame, so a run reads as what a person did.
    if (scripted < options.keys.size()) {
      const uint8_t key = options.keys[scripted++];
      if (key == xpui_host::kHomeGesture) {
        input.tapHome();
      } else {
        input.tapButton(key);
      }
    }

    // Reported to the screen through the ABI *and* acted on here. Not a double
    // action: the framework never handles the gesture itself, it only lets a
    // screen see one, so somebody above the stack has to give it its meaning.
    //
    // `xpui-simulator` splits these — its H key calls `App::home_gesture` only,
    // and just its edge-swipe path also sets `was_home_gesture`. Doing both for
    // the key here means a screen polling `Input::was_home_gesture()` sees it
    // however the gesture arrived.
    if (input.wasHomeGesture()) stack.homeGesture();

    stack.loop();

    // Requested during the input phase, acted on after it — a present that
    // blitted where it was raised would push the frame *before* the one it
    // was asking for. Only counted when a screen actually painted, or an empty
    // stack would leave a blit on the record with nothing behind it.
    if (Display::takeUpdateRequest() && stack.render()) {
      display.blit();
    }

    ++frame;
    if (stack.empty()) running = false;
    if (options.frames >= 0 && frame >= options.frames) running = false;
    // ~60 Hz. A real panel refreshes in a second or more; this is the window's
    // pace, not the panel's.
    if (!options.headless && running) SDL_Delay(16);
  }

  if (options.out && !display.writeBmp(options.out, options.crop)) {
    std::fprintf(stderr, "could not write %s\n", options.out);
    return 1;
  }

  report(options, display, stack, frame);

  // Every assertion lands in the exit code, never in the output a test greps:
  // ctest ignores the exit code entirely once PASS_REGULAR_EXPRESSION is set.
  int failures = expectations(options, display, stack);
  if (options.selftest) failures += selftest(display);
  return failures == 0 ? 0 : 1;
}
