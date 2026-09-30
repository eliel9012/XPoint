#include "SkyTokenStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>
#include <Preferences.h>

#include <algorithm>
#include <cstring>

namespace {
constexpr char NVS_NAMESPACE[] = "cpsky";
constexpr char NVS_KEY[] = "token";
constexpr char BLOB_VERSION = 1;

enum class ReadResult { Missing, Valid, Invalid };

ReadResult readNvs(std::string& value) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return ReadResult::Invalid;
  if (!prefs.isKey(NVS_KEY)) {
    prefs.end();
    return ReadResult::Missing;
  }
  const size_t length = prefs.getBytesLength(NVS_KEY);
  if (length < 1 || length > SkyTokenStore::MAX_TOKEN_LENGTH + 1) {
    prefs.end();
    return ReadResult::Invalid;
  }
  // One bounded allocation avoids a 257-byte buffer on the small C3 stack.
  std::string blob(length, '\0');
  const size_t read = prefs.getBytes(NVS_KEY, blob.data(), blob.size());
  prefs.end();
  if (read != length || blob[0] != BLOB_VERSION) return ReadResult::Invalid;
  value.assign(blob.data() + 1, length - 1);
  return ReadResult::Valid;
}

bool writeNvs(const std::string& value) {
  if (value.size() > SkyTokenStore::MAX_TOKEN_LENGTH) return false;
  std::string blob(1, BLOB_VERSION);
  blob.append(value);
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  const bool written = prefs.putBytes(NVS_KEY, blob.data(), blob.size()) == blob.size();
  prefs.end();
  if (!written) return false;
  std::string verified;
  return readNvs(verified) == ReadResult::Valid && verified == value;
}

void removeLegacyFile() {
  if (Storage.exists(SkyTokenStore::getFilePath()) && !Storage.remove(SkyTokenStore::getFilePath())) {
    LOG_ERR("SKY", "Could not remove legacy token file from SD");
  }
}
}  // namespace

bool SkyTokenStore::loadFromFile() {
  std::lock_guard<std::mutex> lock(storeMutex);
  std::string nvsToken;
  const ReadResult nvsResult = readNvs(nvsToken);
  if (nvsResult == ReadResult::Valid) {
    token = std::move(nvsToken);
    removeLegacyFile();
    return true;
  }

  JsonDocument doc;
  if (!readDocFromFile(getFilePath(), doc)) {
    if (nvsResult == ReadResult::Invalid) LOG_ERR("SKY", "NVS token unavailable; no usable SD fallback");
    return false;
  }
  const JsonVariantConst legacyDoc = doc.as<JsonVariantConst>();
  const char* encoded = legacyDoc["password_obf"].as<const char*>();
  const char* plaintext = legacyDoc["password"].as<const char*>();
  bool decoded = false;
  bool tooLong = false;
  std::string legacy;
  if (encoded != nullptr && encoded[0] != '\0') {
    legacy = obfuscation::deobfuscateFromBase64(encoded, MAX_TOKEN_LENGTH, &decoded, &tooLong);
  } else if (encoded != nullptr && plaintext == nullptr) {
    decoded = true;  // Previous store wrote an empty obfuscated value for an empty token.
  }
  if (tooLong) {
    LOG_ERR("SKY", "Legacy token invalid; leaving SD file intact");
    return false;
  }
  if (!decoded && plaintext != nullptr) {
    const size_t length = std::strlen(plaintext);
    if (length <= MAX_TOKEN_LENGTH) {
      legacy.assign(plaintext, length);
      decoded = true;
    }
  }
  if (!decoded) {
    LOG_ERR("SKY", "Legacy token invalid; leaving SD file intact");
    return false;
  }
  token = std::move(legacy);
  if (!writeNvs(token)) {
    LOG_ERR("SKY", "NVS token migration failed; retaining SD fallback");
    return true;
  }
  removeLegacyFile();
  return true;
}

bool SkyTokenStore::saveToFile() const {
  std::lock_guard<std::mutex> lock(storeMutex);
  if (!writeNvs(token)) {
    LOG_ERR("SKY", "Could not save token to NVS");
    return false;
  }
  removeLegacyFile();
  return true;
}

void SkyTokenStore::setToken(const std::string& value) {
  token.assign(value.data(), std::min(value.size(), MAX_TOKEN_LENGTH));
}
