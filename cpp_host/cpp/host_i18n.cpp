// The string table, and the strings that have to outlive the screen they came
// from.
//
// A firmware generates this table from translation files. One built by hand is
// enough to prove the path — and the path is worth proving, because the screens
// carry no English of their own: every label on them arrives through here, so a
// key that is never added shows up on the panel as the key itself.

#include <string.h>

#include <deque>
#include <string>

#include "internal.h"
#include "xpui_host.h"

namespace {

struct Entry {
  const char* key;
  const char* text;
};

// English only. A second language would be a second table and a setting to
// choose between them, which is a firmware's problem rather than this
// example's.
constexpr Entry kTable[] = {
    {"STR_MENU_TITLE", "xpui on a C++ host"},
    {"STR_MENU_ABOUT", "About this host"},
    {"STR_MENU_ABOUT_SUB", "Device, version and memory"},
    {"STR_MENU_CONTROLS", "Controls"},
    {"STR_MENU_CONTROLS_SUB", "Slider, toggle and a dialog"},

    {"STR_ABOUT_TITLE", "About"},
    {"STR_ABOUT_HOST", "Host"},
    {"STR_ABOUT_DEVICE", "Device"},
    {"STR_ABOUT_VERSION", "Version"},
    {"STR_ABOUT_BATTERY", "Battery"},
    {"STR_ABOUT_MEMORY", "C++ heap"},
    {"STR_ABOUT_TOTAL", "Budget"},
    {"STR_ABOUT_USED", "Used"},
    {"STR_ABOUT_FREE", "Free"},
    {"STR_ABOUT_LARGEST", "Largest block"},
    {"STR_ABOUT_MIN_FREE", "Least ever free"},
    {"STR_ABOUT_REFRESH", "Refresh"},

    {"STR_CONTROLS_TITLE", "Controls"},
    {"STR_CONTROLS_LEVEL", "Level"},
    {"STR_CONTROLS_LIGHT", "Frontlight"},
    {"STR_CONTROLS_ON", "On"},
    {"STR_CONTROLS_OFF", "Off"},
    {"STR_CONTROLS_MODE", "Refresh mode"},
    {"STR_CONTROLS_FAST", "Fast"},
    {"STR_CONTROLS_QUALITY", "Quality"},
};

// Titles handed over from Rust, kept for the life of the program.
//
// A deque rather than a vector: growing a vector moves its elements, and every
// pointer already handed out would follow. Deque promises the opposite —
// references stay valid across insertion — which is exactly the promise
// `intern` makes.
std::deque<std::string>& internedStrings() {
  static std::deque<std::string> strings;
  return strings;
}

}  // namespace

namespace xpui_host {

const uint8_t* intern(const char* text) {
  const char* value = text ? text : "";

  // Deduplicated so a screen pushed a hundred times costs one string. Linear,
  // over a list that never reaches double figures in this example.
  for (const std::string& existing : internedStrings()) {
    if (existing == value) return reinterpret_cast<const uint8_t*>(existing.c_str());
  }

  internedStrings().emplace_back(value);
  return reinterpret_cast<const uint8_t*>(internedStrings().back().c_str());
}

}  // namespace xpui_host

extern "C" const uint8_t* xpui_host_tr(const uint8_t* key) {
  if (!key) return reinterpret_cast<const uint8_t*>("");

  const char* wanted = reinterpret_cast<const char*>(key);
  for (const Entry& entry : kTable) {
    if (strcmp(entry.key, wanted) == 0) return reinterpret_cast<const uint8_t*>(entry.text);
  }

  // The key itself, so a missing string is visible on the panel rather than a
  // blank row somebody has to go looking for.
  return key;
}
