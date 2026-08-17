// Device identity and battery.
//
// A firmware answers these from its board configuration and its power manager.
// A laptop is not a device, and saying so is more useful than inventing a
// plausible answer: the battery reports "no battery" rather than a number that
// would look real on a screenshot.

#include "xpui_host.h"

#ifndef XPUI_HOST_DEVICE_NAME
#define XPUI_HOST_DEVICE_NAME "Desktop (CMake)"
#endif

#ifndef XPUI_HOST_VERSION
#define XPUI_HOST_VERSION "0.0.0-dev"
#endif

extern "C" {

const uint8_t* xpui_host_device_name(void) { return reinterpret_cast<const uint8_t*>(XPUI_HOST_DEVICE_NAME); }

const uint8_t* xpui_host_firmware_version(void) { return reinterpret_cast<const uint8_t*>(XPUI_HOST_VERSION); }

// Negative for "there is no battery here", which is not the same answer as a
// flat one and must not be reported as 0.
int32_t xpui_host_battery_percent(void) { return -1; }
}
