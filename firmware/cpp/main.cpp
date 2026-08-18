// An ESP32 firmware that runs xpui screens.
//
// The whole of the device-specific wiring, and it is short on purpose: what
// this example exists to show is the *build*, not the application. Every
// screen, the navigator, the platform and the ABI declarations come from
// `examples/cpp_host` unchanged; what is here is a framebuffer, an entry
// point, and the four `host_*.cpp` files a laptop answers differently.
//
//   pio run -e default    Xteink X3, ESP32-C3
//   pio run -e sticky     Seeed Sticky, ESP32-S3
//
// **There is no panel driver**, for the same reason `examples/esp32` has none:
// no published Rust or C++ driver exists for these panels that this repository
// can test. `flush()` below is where one goes.

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <xpui_app.h>
#include <xpui_fui.h>
#include <xpui_host.h>
#include <xpui_screen.h>

#include <cstring>

#include "ScreenStack.h"
#include "internal.h"

namespace {

constexpr char TAG[] = "xpui";

// The panel, as the shim sees it: one bit per pixel, MSB first, a set bit is
// white.
//
// **Per board, from `platformio.ini`.** These have to be the board's real
// dimensions and not merely dimensions the buffer can hold: every screen is
// laid out against them, so a Sticky told it is 528x792 lays out 48 px wider
// than its glass and 8 px shorter — and boots, and logs a plausible ink count,
// and looks fine until somebody holds one. The numbers are `Board::X3` and
// `Board::STICKY` from `crates/boards`, which is the same source the simulator
// measures against.
#if !defined(XPUI_PANEL_WIDTH) || !defined(XPUI_PANEL_HEIGHT)
#error "platformio.ini must define XPUI_PANEL_WIDTH and XPUI_PANEL_HEIGHT for this board"
#endif

constexpr int32_t PANEL_WIDTH = XPUI_PANEL_WIDTH;
constexpr int32_t PANEL_HEIGHT = XPUI_PANEL_HEIGHT;
constexpr size_t PANEL_STRIDE = (PANEL_WIDTH + 7) / 8;
constexpr size_t FRAMEBUFFER_BYTES = PANEL_STRIDE * PANEL_HEIGHT;

// In `.bss` rather than on the heap: it is the largest single allocation in
// the firmware, and a boot that fails to allocate it should fail at link time
// with a section that does not fit, not at run time with a null pointer.
uint8_t g_framebuffer[FRAMEBUFFER_BYTES];

// Raised by the framework through `xpui_fui_request_update`, and acted on
// after the frame rather than inside it — the request arrives during input,
// before anything has been painted.
bool g_updateRequested = false;

void requestPresent() { g_updateRequested = true; }

// **Where the panel driver goes.**
//
// The X3 and the Sticky drive their glass over SPI through a command sequence,
// a waveform table and a wait on a BUSY line. None of that is in this
// repository and none of it can be tested from it, so this reports what it
// would have pushed and stops. See `examples/esp32/src/panel.rs` for the same
// seam on the pure-Rust side.
void flush() {
  size_t ink = 0;
  for (size_t index = 0; index < FRAMEBUFFER_BYTES; ++index) {
    // A set bit is white, so ink is the zeroes.
    ink += static_cast<size_t>(__builtin_popcount(static_cast<uint8_t>(~g_framebuffer[index])));
  }
  ESP_LOGI(TAG, "frame ready: %dx%d, %u ink pixels", static_cast<int>(PANEL_WIDTH), static_cast<int>(PANEL_HEIGHT),
           static_cast<unsigned>(ink));
}

}  // namespace

extern "C" void app_main(void) {
  // Paper, not ink: an unpainted frame should be blank rather than a solid
  // black rectangle.
  std::memset(g_framebuffer, 0xFF, sizeof(g_framebuffer));

  // Order matters and nothing checks it: this tells the *shim* where the panel
  // is, and `xpui_app_install` tells the *framework* where the shim is.
  xpui_fui_attach(g_framebuffer, PANEL_WIDTH, PANEL_HEIGHT);
  xpui_fui_set_present(&requestPresent);
  xpui_app_install();

  xpui_host::ScreenStack stack;
  xpui_host::g_stack = &stack;
  // `g_input` stays null — no input manager is wired on this board, and
  // `host_input.cpp` answers every query without it. `g_screen` is bound by
  // `ScreenHost` at the top of every frame, which is what makes it work when
  // the stack resumes a screen without re-entering it.

  stack.push(xpui_app_create_menu(), xpui_host_tr(reinterpret_cast<const uint8_t*>("STR_MENU_TITLE")));

  while (!stack.empty()) {
    stack.loop();
    if (g_updateRequested && stack.render()) {
      g_updateRequested = false;
      flush();
    }
    // Ten milliseconds, as on every other board here. A loop with no wait in
    // it spins the core at full clock for the life of the battery.
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  ESP_LOGI(TAG, "the root screen finished; nothing left to show");
}
