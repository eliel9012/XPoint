#pragma once

#include "TestStorage.h"

#include <cstring>
#include <mutex>
#include <string>

class PersistableStoreBase {
 protected:
  mutable std::mutex storeMutex;

  static bool readDocFromFile(const char*, JsonDocument& doc) {
    if (!fake::sdExists) return false;
    doc = fake::sdDocument;
    return true;
  }

  static std::string extractPassword(JsonVariantConst doc, bool&, size_t maxLength, bool& valid) {
    const char* value = doc["password_obf"].is<const char*>() ? doc["password_obf"].as<const char*>()
                                                          : doc["password"] | "";
    valid = std::strlen(value) <= maxLength;
    return valid ? std::string(value) : std::string();
  }
};

template <typename T>
class PersistableStore : public PersistableStoreBase {
 public:
  static T& getInstance() {
    static T instance;
    return instance;
  }
};
