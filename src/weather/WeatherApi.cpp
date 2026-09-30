#include "WeatherApi.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "network/HttpDownloader.h"

namespace weather {
namespace {
constexpr size_t MAX_RESPONSE_BYTES = 4096;
constexpr int REQUEST_TIMEOUT_MS = 8000;

bool validNumber(JsonVariantConst value, float lower, float upper) {
  if (!value.is<float>() && !value.is<int>()) return false;
  const float number = value.as<float>();
  return std::isfinite(number) && number >= lower && number <= upper;
}

bool decode(char* body, size_t size, Snapshot& out) {
  JsonDocument doc;
  const auto error = deserializeJson(doc, body, size);
  if (error || doc.overflowed() || !doc["current"].is<JsonObject>() || !doc["daily"].is<JsonObject>()) return false;
  const JsonObjectConst current = doc["current"].as<JsonObjectConst>();
  const JsonObjectConst daily = doc["daily"].as<JsonObjectConst>();
  const char* time = current["time"] | "";
  if (std::strlen(time) != 16 || time[10] != 'T' || !validNumber(current["temperature_2m"], -100, 70) ||
      !validNumber(current["apparent_temperature"], -150, 100) ||
      !validNumber(current["relative_humidity_2m"], 0, 100) || !validNumber(current["wind_speed_10m"], 0, 500) ||
      !validNumber(current["weather_code"], 0, 99))
    return false;

  const JsonArrayConst dates = daily["time"].as<JsonArrayConst>();
  const JsonArrayConst highs = daily["temperature_2m_max"].as<JsonArrayConst>();
  const JsonArrayConst lows = daily["temperature_2m_min"].as<JsonArrayConst>();
  const JsonArrayConst codes = daily["weather_code"].as<JsonArrayConst>();
  const JsonArrayConst rain = daily["precipitation_probability_max"].as<JsonArrayConst>();
  if (dates.size() != FORECAST_DAYS || highs.size() != FORECAST_DAYS || lows.size() != FORECAST_DAYS ||
      codes.size() != FORECAST_DAYS || rain.size() != FORECAST_DAYS)
    return false;

  Snapshot next;
  std::memcpy(next.time, time, sizeof(next.time) - 1);
  next.temperature = current["temperature_2m"].as<float>();
  next.feelsLike = current["apparent_temperature"].as<float>();
  next.humidity = current["relative_humidity_2m"].as<int>();
  next.windKmh = current["wind_speed_10m"].as<float>();
  next.code = current["weather_code"].as<int>();
  for (int i = 0; i < FORECAST_DAYS; ++i) {
    const char* date = dates[i] | "";
    if (std::strlen(date) != 10 || date[4] != '-' || date[7] != '-' || !validNumber(highs[i], -100, 70) ||
        !validNumber(lows[i], -100, 70) || !validNumber(codes[i], 0, 99) || !validNumber(rain[i], 0, 100))
      return false;
    std::memcpy(next.days[i].date, date, sizeof(next.days[i].date) - 1);
    next.days[i].high = highs[i].as<float>();
    next.days[i].low = lows[i].as<float>();
    next.days[i].code = codes[i].as<int>();
    next.days[i].rainChance = rain[i].as<int>();
  }
  out = next;
  return true;
}
}  // namespace

FetchResult fetch(float lat, float lon, Snapshot& out, std::atomic<bool>& cancel) {
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) return FetchResult::NO_WIFI;
  PoolBytes body = poolMakeBytes(MAX_RESPONSE_BYTES + 1);
  if (!body || !HttpDownloader::heapAvailableForTransfer()) return FetchResult::LOW_MEMORY;

  PoolBytes url = poolMakeBytes(320);
  if (!url) return FetchResult::LOW_MEMORY;
  const int length =
      std::snprintf(reinterpret_cast<char*>(url.get()), 320,
                    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,"
                    "relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m&daily=weather_code,"
                    "temperature_2m_max,temperature_2m_min,precipitation_probability_max&timezone=auto&forecast_days=3",
                    lat, lon);
  if (length <= 0 || length >= 320) return FetchResult::INVALID_DATA;

  esp_http_client_config_t config = {};
  config.url = reinterpret_cast<const char*>(url.get());
  config.timeout_ms = REQUEST_TIMEOUT_MS;
  config.buffer_size = 1024;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.disable_auto_redirect = true;
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) return FetchResult::LOW_MEMORY;
  FetchResult result = FetchResult::NETWORK_ERROR;
  size_t used = 0;
  const uint32_t deadline = millis() + 25000;
  if (esp_http_client_open(client, 0) == ESP_OK) {
    const int64_t announced = esp_http_client_fetch_headers(client);
    // A chunked response reports no content length; the read loop still caps it.
    if (announced <= static_cast<int64_t>(MAX_RESPONSE_BYTES) && esp_http_client_get_status_code(client) == 200) {
      while (!cancel.load(std::memory_order_relaxed) && used < MAX_RESPONSE_BYTES &&
             static_cast<int32_t>(millis() - deadline) < 0) {
        const int n = esp_http_client_read(client, reinterpret_cast<char*>(body.get() + used),
                                           static_cast<int>(MAX_RESPONSE_BYTES - used));
        if (n < 0) break;
        if (n == 0) {
          if (!esp_http_client_is_complete_data_received(client)) break;
          body[used] = '\0';
          result = decode(reinterpret_cast<char*>(body.get()), used, out) ? FetchResult::OK : FetchResult::INVALID_DATA;
          break;
        }
        used += static_cast<size_t>(n);
      }
      if (used == MAX_RESPONSE_BYTES && result != FetchResult::OK) result = FetchResult::INVALID_DATA;
    }
  }
  esp_http_client_cleanup(client);
  if (cancel.load(std::memory_order_relaxed)) return FetchResult::NETWORK_ERROR;
  if (result != FetchResult::OK)
    LOG_ERR("WEATHER", "Open-Meteo request failed (%u bytes)", static_cast<unsigned>(used));
  return result;
}

}  // namespace weather
