#pragma once

#include <atomic>
#include <memory>

#include "activities/UiListActivity.h"
#include "weather/WeatherApi.h"

class WeatherActivity final : public UiListActivity {
 public:
  WeatherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

 private:
  enum class State : uint8_t { OFFLINE, LOADING, DATA, ERROR, LOW_MEMORY, INVALID_LOCATION, SAVE_ERROR };
  struct FetchWork {
    std::atomic<bool> done{false};
    std::atomic<bool> cancel{false};
    weather::FetchResult result = weather::FetchResult::NETWORK_ERROR;
    weather::Snapshot* destination = nullptr;
    float latitude = 0;
    float longitude = 0;
  };

  State state = State::OFFLINE;
  std::unique_ptr<FetchWork> fetchWork;
  weather::Snapshot snapshot;
  bool wifiStartedByUs = false;
  freeink::ui::ListItem rows[6] = {};
  char labels[6][64] = {};
  char subtitles[6][96] = {};

  void onEnter() override;
  void onExit() override;
  void loop() override;
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void beginFetch();
  void launchFetch();
  void finishFetch(weather::FetchResult result);
  void editCoordinates();
  void connectWifi();
  void rebuildRows();
  const char* condition(int code) const;
};
