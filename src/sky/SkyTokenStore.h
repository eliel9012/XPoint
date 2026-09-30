#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>

// Stores the SKY bearer token in device NVS. The legacy SD file is read only
// for migration and removed after the NVS copy has been verified. NVS content
// remains physically readable unless flash/NVS encryption is provisioned.
class SkyTokenStore final : public PersistableStore<SkyTokenStore> {
 private:
  std::string token;

  SkyTokenStore() = default;
  friend class PersistableStore<SkyTokenStore>;

 public:
  static constexpr size_t MAX_TOKEN_LENGTH = 256;
  static const char* getFilePath() { return "/.crosspoint/sky.json"; }

  bool loadFromFile();
  bool saveToFile() const;

  const std::string& getToken() const { return token; }
  bool hasToken() const { return !token.empty(); }
  void setToken(const std::string& value);
};

#define SKY_TOKEN_STORE SkyTokenStore::getInstance()
