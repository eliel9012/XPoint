#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

class WeatherStore final : public PersistableStore<WeatherStore> {
  WeatherStore() = default;
  friend class PersistableStore<WeatherStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/weather.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);
  bool setCoordinates(const char* text);
  float latitude() const { return lat; }
  float longitude() const { return lon; }
  bool isDefault() const { return defaultLocation; }

 private:
  float lat = -23.5505f;
  float lon = -46.6333f;
  bool defaultLocation = true;
};

#define WEATHER_STORE WeatherStore::getInstance()
