#include "WeatherStore.h"

#include <cmath>
#include <cstdlib>

void WeatherStore::toJson(JsonDocument& doc) const {
  doc["latitude"] = lat;
  doc["longitude"] = lon;
  doc["defaultLocation"] = defaultLocation;
}

bool WeatherStore::fromJson(JsonVariantConst doc) {
  if (!doc["latitude"].is<float>() && !doc["latitude"].is<int>()) return false;
  if (!doc["longitude"].is<float>() && !doc["longitude"].is<int>()) return false;
  const float savedLat = doc["latitude"].as<float>();
  const float savedLon = doc["longitude"].as<float>();
  if (!std::isfinite(savedLat) || !std::isfinite(savedLon) || savedLat < -90 || savedLat > 90 || savedLon < -180 ||
      savedLon > 180)
    return false;
  lat = savedLat;
  lon = savedLon;
  defaultLocation = doc["defaultLocation"] | false;
  return true;
}

bool WeatherStore::setCoordinates(const char* text) {
  if (!text) return false;
  char* end = nullptr;
  const float newLat = std::strtof(text, &end);
  if (end == text || *end != ',') return false;
  const char* longitudeStart = end + 1;
  const float newLon = std::strtof(longitudeStart, &end);
  if (end == longitudeStart) return false;
  while (*end == ' ') ++end;
  if (*end || !std::isfinite(newLat) || !std::isfinite(newLon) || newLat < -90 || newLat > 90 || newLon < -180 ||
      newLon > 180)
    return false;
  lat = newLat;
  lon = newLon;
  defaultLocation = false;
  return true;
}
