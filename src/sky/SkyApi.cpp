#include "SkyApi.h"

#include <ArduinoJson.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "network/HttpDownloader.h"

namespace sky {
namespace {
constexpr char API_URL[] = "https://app.meulab.fun/api/adsb/aircraft";
constexpr size_t MAX_RESPONSE_BYTES = 24 * 1024;

template <size_t N>
void copyString(JsonVariantConst value, char (&destination)[N]) {
  const char* source = value | "";
  if (source == nullptr) source = "";
  std::strncpy(destination, source, N - 1);
  destination[N - 1] = '\0';
}

void copyAircraft(JsonObjectConst object, Aircraft& aircraft) {
  copyString(object["hex"], aircraft.hex);
  copyString(object["callsign"], aircraft.callsign);
  copyString(object["model"], aircraft.model);
  copyString(object["airline"], aircraft.airline);
  aircraft.latitude = object["lat"] | 0.0f;
  aircraft.longitude = object["lon"] | 0.0f;
  aircraft.track = object["track"] | 0.0f;
  aircraft.altitudeFt = object["altitude_ft"] | 0;
  aircraft.speedKt = object["speed_kt"] | 0;
  aircraft.distanceNm = object["distance_nm"] | 0;
  aircraft.verticalRateFpm = object["vertical_rate_fpm"] | 0;
}
}  // namespace

FetchResult fetch(const char* token, Snapshot& out, std::atomic<bool>* cancelFlag) {
  // The item count is the validity boundary for the fixed array. Clearing the
  // whole snapshot here creates a several-KB temporary on the worker stack.
  out.itemCount = 0;
  out.reportedCount = 0;
  out.timestamp[0] = '\0';
  if (token == nullptr || token[0] == '\0') return FetchResult::NO_TOKEN;
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    return FetchResult::NETWORK_ERROR;
  }
  // Reserve the entire bounded response before TLS starts. X4 Pro stores this
  // in PSRAM; on C3 an allocation failure is a clean LOW_MEMORY result.
  PoolBytes body = poolMakeBytes(MAX_RESPONSE_BYTES + 1);
  if (!body || !HttpDownloader::heapAvailableForTransfer()) return FetchResult::LOW_MEMORY;
  size_t bodySize = 0;
  const bool fetched = HttpDownloader::fetchUrlBearer(
      API_URL,
      [&body, &bodySize](const uint8_t* data, size_t len) {
        if (len > MAX_RESPONSE_BYTES - bodySize) return false;
        std::memcpy(body.get() + bodySize, data, len);
        bodySize += len;
        return true;
      },
      token, cancelFlag);
  if (!fetched) {
    return FetchResult::NETWORK_ERROR;
  }
  body[bodySize] = '\0';

  JsonDocument doc;
  // Keep only the fields consumed below. The body cap and fixed item cap also
  // prevent an oversized server response from becoming an unbounded UI model.
  JsonDocument filter;
  filter["timestamp"] = true;
  filter["count"] = true;
  JsonObject filterItem = filter["items"].to<JsonArray>().add<JsonObject>();
  filterItem["hex"] = true;
  filterItem["callsign"] = true;
  filterItem["model"] = true;
  filterItem["airline"] = true;
  filterItem["lat"] = true;
  filterItem["lon"] = true;
  filterItem["track"] = true;
  filterItem["altitude_ft"] = true;
  filterItem["speed_kt"] = true;
  filterItem["distance_nm"] = true;
  filterItem["vertical_rate_fpm"] = true;

  // ArduinoJson can reference strings in a mutable input buffer. Aircraft
  // strings are copied into Snapshot before body goes out of scope.
  const DeserializationError error =
      deserializeJson(doc, reinterpret_cast<char*>(body.get()), bodySize, DeserializationOption::Filter(filter));
  if (error || doc.overflowed() || !doc.is<JsonObject>() || !doc["items"].is<JsonArray>()) {
    LOG_ERR("SKY", "Invalid or oversized aircraft response");
    return FetchResult::INVALID_JSON;
  }

  copyString(doc["timestamp"], out.timestamp);
  out.reportedCount = doc["count"] | 0U;
  const JsonArrayConst items = doc["items"].as<JsonArrayConst>();
  for (JsonObjectConst item : items) {
    if (out.itemCount >= MAX_AIRCRAFT) break;
    copyAircraft(item, out.items[out.itemCount]);
    ++out.itemCount;
  }
  return FetchResult::OK;
}

}  // namespace sky
