// Where a Rust panic goes on a device.
//
// Nothing can be recovered: the framework builds with `panic = "abort"`, so
// this is the abort. The most useful thing it can do is put the message
// somewhere a person will find it — a screen that vanished with nothing in the
// log is the worst way to learn a firmware crashed.

#include <esp_log.h>

#include <cstdlib>

#include "xpui_host.h"

extern "C" void xpui_host_panic(const uint8_t* message) {
  ESP_LOGE("xpui", "rust panic: %s", message ? reinterpret_cast<const char*>(message) : "?");
  abort();
}
