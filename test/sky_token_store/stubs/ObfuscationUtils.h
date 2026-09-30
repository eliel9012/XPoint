#pragma once

#include <cstring>
#include <string>

namespace obfuscation {
// Test encoding models valid decode, malformed data, and decoded-size limits.
inline std::string deobfuscateFromBase64(const char* encoded, size_t maxLength, bool* ok, bool* tooLong) {
  *ok = false;
  *tooLong = false;
  if (encoded == nullptr || std::strncmp(encoded, "enc:", 4) != 0) return {};
  const size_t length = std::strlen(encoded + 4);
  if (length > maxLength) {
    *tooLong = true;
    return {};
  }
  *ok = true;
  return std::string(encoded + 4, length);
}
}  // namespace obfuscation
