#pragma once

#include <ArduinoJson.h>

#include <string>

namespace fake {
inline bool sdExists = false;
inline bool sdRemoveFails = false;
inline JsonDocument sdDocument;
inline bool nvsOpenFails = false;
inline bool nvsWriteFails = false;
inline bool nvsReadFails = false;
inline bool nvsPresent = false;
inline std::string nvsBlob;

inline void reset() {
  sdExists = false;
  sdRemoveFails = false;
  sdDocument.clear();
  nvsOpenFails = false;
  nvsWriteFails = false;
  nvsReadFails = false;
  nvsPresent = false;
  nvsBlob.clear();
}
}  // namespace fake
