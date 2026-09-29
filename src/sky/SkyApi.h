#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace sky {

constexpr size_t MAX_AIRCRAFT = 24;

struct Aircraft {
  char hex[16] = {};
  char callsign[24] = {};
  char model[32] = {};
  char airline[32] = {};
  float latitude = 0.0f;
  float longitude = 0.0f;
  float track = 0.0f;
  int32_t altitudeFt = 0;
  int32_t speedKt = 0;
  int32_t distanceNm = 0;
  int32_t verticalRateFpm = 0;
};

struct Snapshot {
  char timestamp[32] = {};
  uint16_t reportedCount = 0;
  uint8_t itemCount = 0;
  Aircraft items[MAX_AIRCRAFT] = {};
};

enum class FetchResult : uint8_t {
  OK,
  NO_TOKEN,
  LOW_MEMORY,
  NETWORK_ERROR,
  INVALID_JSON,
};

FetchResult fetch(const char* token, Snapshot& out, std::atomic<bool>* cancelFlag = nullptr);

}  // namespace sky
