// Heap figures, from the RTOS.
//
// **This is the pay-off for routing Rust's allocator through the firmware's
// `malloc`.** On the desktop host these four cover the C++ side only, and the
// About screen says so. Here they cover both languages, because there is one
// heap — which makes them the only visibility into what Rust costs at run
// time: the build-time size report measures static sections, where Rust
// contributes almost nothing.

#include <esp_heap_caps.h>

#include "xpui_host.h"

extern "C" {

int32_t xpui_host_heap_total(void) {
  multi_heap_info_t info = {};
  heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
  return static_cast<int32_t>(info.total_free_bytes + info.total_allocated_bytes);
}

int32_t xpui_host_heap_free(void) { return static_cast<int32_t>(heap_caps_get_free_size(MALLOC_CAP_DEFAULT)); }

// Fragmentation, which a desktop cannot measure and this can. It is the figure
// that matters on a device with no MMU: plenty free and nowhere to put
// anything is a real failure, and the About screen shows the gap.
int32_t xpui_host_heap_largest_block(void) {
  return static_cast<int32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}

int32_t xpui_host_heap_min_free(void) {
  return static_cast<int32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
}
}
