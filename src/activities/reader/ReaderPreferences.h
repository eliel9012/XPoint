#pragma once

#include <cstddef>
#include <cstdint>

// Reader-only, allocation-free view of the preferences that affect the page
// and its footer. It deliberately reads CrossPointSettings without owning or
// editing settings, keeping the notification center independent of the
// settings/keyboard/browser activities.
namespace ReaderPreferences {

struct Snapshot {
  char source[48] = {};
  uint8_t fontSize = 0;
  bool showsPageCount = false;
  bool showsTimeLeft = false;
  bool showsProgress = false;
  bool showsTitle = false;
  bool showsBattery = false;
  bool showsProgressBar = false;
};

Snapshot snapshot();

// Formats a short detail line for the bounded notification entry. The
// caller-owned buffer is always NUL terminated, including on truncation.
void formatNotificationDetail(char* buffer, size_t bufferSize);

}  // namespace ReaderPreferences
