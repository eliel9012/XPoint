#pragma once

#include <atomic>
#include <cstdint>

namespace weather {

constexpr int FORECAST_DAYS = 3;

struct Day {
  char date[11] = {};
  float high = 0;
  float low = 0;
  int code = -1;
  int rainChance = 0;
};

struct Snapshot {
  char time[17] = {};
  float temperature = 0;
  float feelsLike = 0;
  int humidity = 0;
  float windKmh = 0;
  int code = -1;
  Day days[FORECAST_DAYS] = {};
};

enum class FetchResult : uint8_t { OK, NO_WIFI, LOW_MEMORY, NETWORK_ERROR, INVALID_DATA };

FetchResult fetch(float lat, float lon, Snapshot& out, std::atomic<bool>& cancel);

}  // namespace weather
