#include "SkyTokenStore.h"

#include <ObfuscationUtils.h>

#include <algorithm>

void SkyTokenStore::toJson(JsonDocument& doc) const { doc["password_obf"] = obfuscation::obfuscateToBase64(token); }

bool SkyTokenStore::fromJson(JsonVariantConst doc) {
  bool needsResave = false;
  bool valid = false;
  std::string loaded = extractPassword(doc, needsResave, MAX_TOKEN_LENGTH, valid);
  if (!valid) loaded.clear();
  setToken(loaded);
  if (needsResave) requestResave();
  return true;
}

void SkyTokenStore::setToken(const std::string& value) {
  token.assign(value.data(), std::min(value.size(), MAX_TOKEN_LENGTH));
}
