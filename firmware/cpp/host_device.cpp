// Device identity and battery, on a device.
//
// The mirror of `examples/cpp_host/cpp/host_device.cpp`, and the clearest
// example of what this whole boundary is for: same three symbols, same header,
// completely different answers. A laptop says "no battery"; this reads the
// board.

#include <esp_app_desc.h>

#include "xpui_host.h"

#ifndef XPUI_HOST_DEVICE_NAME
#define XPUI_HOST_DEVICE_NAME CONFIG_IDF_TARGET
#endif

extern "C" {

const uint8_t* xpui_host_device_name(void) { return reinterpret_cast<const uint8_t*>(XPUI_HOST_DEVICE_NAME); }

// The version the build stamped into the image, rather than a constant that
// can disagree with what was flashed.
const uint8_t* xpui_host_firmware_version(void) {
  const esp_app_desc_t* description = esp_app_get_description();
  return reinterpret_cast<const uint8_t*>(description ? description->version : "?");
}

// Negative for "this board cannot tell you", the ABI's convention. A board
// with a fuel gauge reads it here; the dev kits do not have one.
int32_t xpui_host_battery_percent(void) { return -1; }
}
