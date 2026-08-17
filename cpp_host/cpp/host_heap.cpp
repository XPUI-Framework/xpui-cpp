// What this host can say about its own memory.
//
// A firmware asks its RTOS: Rust allocates through the same heap the C++ does,
// so four calls cover both languages, and they are the only visibility into
// what Rust costs at run time — a build-time size report measures static
// sections, where Rust contributes almost nothing.
//
// A desktop has neither an RTOS nor a heap ceiling, so this counts what it can
// actually count. Two consequences, both visible on the About screen rather
// than hidden here:
//
//   * The figures cover the C++ side only. Rust's allocator on a desktop is
//     the system one and does not come through `operator new`. Joining the two
//     is a `#[global_allocator]` routing to the host's malloc, which is what a
//     firmware does and where that belongs.
//
//   * Fragmentation cannot be measured, so `largest_block` answers -1 rather
//     than repeating `free` — the same "I cannot tell you" the battery uses.
//     Inventing a number here would make the one figure that matters on a
//     device with no MMU into decoration.
//
// One consequence worth knowing before pointing a tool at this: the binary
// aborts under AddressSanitizer, in trackedFree, on a block ASan's own
// interposition routed here. Instrumenting the run without ASan shows zero
// foreign frees, so the replacement itself is consistent — but ASan is exactly
// the tool you would reach for to check that, and it cannot be used.

#include <stdint.h>
#include <stdlib.h>

#include <cstddef>
#include <new>

#include "xpui_host.h"

namespace {

// The frame the figures are shown against. A desktop has no ceiling; a device
// does, and a "used" figure means nothing without one.
//
// Well above what a microcontroller would have, because this host is not one:
// it keeps a full RGB copy of the panel to blit from, which is over a megabyte
// on its own and which a device does not do.
constexpr int32_t kBudget = 8 * 1024 * 1024;

// Zero-initialised before any dynamic initialisation runs, so an allocation
// made from another translation unit's constructor still counts.
size_t g_live = 0;
size_t g_peak = 0;

// The block's own size, kept in front of it so `delete` knows what to subtract.
// 16 bytes rather than `sizeof(size_t)`: malloc returns memory aligned for any
// type, and the payload has to stay that way.
constexpr size_t kHeader = 16;

void* trackedAlloc(const size_t size) {
  // The header must not make the request wrap, or a huge allocation would
  // succeed with a tiny block behind it.
  if (size > SIZE_MAX - kHeader) throw std::bad_alloc();

  // The standard requires the new-handler loop, not a bare throw: a program
  // that installed a handler is entitled to have it called and to retry.
  void* block = std::malloc(size + kHeader);
  while (!block) {
    std::new_handler handler = std::get_new_handler();
    if (!handler) throw std::bad_alloc();
    handler();
    block = std::malloc(size + kHeader);
  }

  *static_cast<size_t*>(block) = size;
  g_live += size;
  if (g_live > g_peak) g_peak = g_live;
  return static_cast<char*>(block) + kHeader;
}

void trackedFree(void* pointer) noexcept {
  if (!pointer) return;
  void* block = static_cast<char*>(pointer) - kHeader;
  const size_t size = *static_cast<size_t*>(block);
  // Clamped rather than wrapped. Every block in this program came from
  // `trackedAlloc`, so this cannot happen — but if it ever did, a wrapped
  // counter would report a heap of sixteen exabytes and look like a bug
  // somewhere else entirely.
  g_live = size > g_live ? 0 : g_live - size;
  std::free(block);
}

}  // namespace

// Replacing the global allocation functions is program-wide, so every C++
// allocation in this binary is counted — including the ones inside SDL's C++,
// if it has any. The aligned (`std::align_val_t`) overloads are deliberately
// NOT replaced: an over-aligned type then uses the default pair, which frees
// with the default delete, and the two never cross.
void* operator new(const size_t size) { return trackedAlloc(size); }
void* operator new[](const size_t size) { return trackedAlloc(size); }
void operator delete(void* pointer) noexcept { trackedFree(pointer); }
void operator delete[](void* pointer) noexcept { trackedFree(pointer); }
void operator delete(void* pointer, size_t) noexcept { trackedFree(pointer); }
void operator delete[](void* pointer, size_t) noexcept { trackedFree(pointer); }

namespace {

// The budget less what is out, clamped rather than cast.
//
// A live figure past `INT32_MAX` would otherwise overflow a signed subtraction
// — reachable with a large enough `--size` — and the ABI's own convention
// gives the honest answer for free: negative means "cannot tell you", and a
// heap that has outgrown the frame it is reported against cannot be.
int32_t remaining(const size_t used) {
  if (used > static_cast<size_t>(kBudget)) return 0;
  return kBudget - static_cast<int32_t>(used);
}

}  // namespace

extern "C" {

int32_t xpui_host_heap_total(void) { return kBudget; }

int32_t xpui_host_heap_free(void) { return remaining(g_live); }

// See the note at the top: not measurable here, and saying so beats guessing.
int32_t xpui_host_heap_largest_block(void) { return -1; }

int32_t xpui_host_heap_min_free(void) { return remaining(g_peak); }
}
