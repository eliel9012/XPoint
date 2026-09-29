#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>

// Stores the user-provided SKY bearer token. The on-disk representation uses
// the same hardware-bound obfuscation as Wi-Fi and KOReader credentials.
class SkyTokenStore final : public PersistableStore<SkyTokenStore> {
 private:
  std::string token;

  SkyTokenStore() = default;
  friend class PersistableStore<SkyTokenStore>;

 public:
  static constexpr size_t MAX_TOKEN_LENGTH = 256;
  static const char* getFilePath() { return "/.crosspoint/sky.json"; }

  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  const std::string& getToken() const { return token; }
  bool hasToken() const { return !token.empty(); }
  void setToken(const std::string& value);
};

#define SKY_TOKEN_STORE SkyTokenStore::getInstance()
