#pragma once

#include <cstddef>
#include <cstring>

#include "TestStorage.h"

class Preferences {
 public:
  bool begin(const char*, bool) { return !fake::nvsOpenFails; }
  void end() {}
  bool isKey(const char*) { return fake::nvsPresent; }
  size_t getBytesLength(const char*) { return fake::nvsReadFails ? 0 : fake::nvsBlob.size(); }
  size_t getBytes(const char*, void* out, size_t maxLength) {
    if (fake::nvsReadFails || fake::nvsBlob.size() > maxLength) return 0;
    std::memcpy(out, fake::nvsBlob.data(), fake::nvsBlob.size());
    return fake::nvsBlob.size();
  }
  size_t putBytes(const char*, const void* data, size_t length) {
    if (fake::nvsWriteFails) return 0;
    fake::nvsBlob.assign(static_cast<const char*>(data), length);
    fake::nvsPresent = true;
    return length;
  }
};
