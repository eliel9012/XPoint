#pragma once

#include <atomic>
#include <string>

#include "activities/UiListActivity.h"
#include "sky/SkyApi.h"

class SkyActivity final : public UiListActivity {
 public:
  explicit SkyActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

 private:
  enum class State : uint8_t { READY, LOADING, NO_TOKEN, NO_WIFI, ERROR, DATA };

  State state = State::READY;
  struct FetchWork {
    std::atomic<bool> done{false};
    std::atomic<bool> cancel{false};
    sky::FetchResult result = sky::FetchResult::NETWORK_ERROR;
    sky::Snapshot* snapshot = nullptr;
    char token[257] = {};
  };
  FetchWork* fetchWork = nullptr;
  bool wifiStartedByUs = false;
  sky::Snapshot snapshot;
  char rowSubtitles[sky::MAX_AIRCRAFT][128] = {};
  freeink::ui::ListItem rowItems[sky::MAX_AIRCRAFT] = {};

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void loop() override;
  void onEnter() override;
  void onExit() override;

  void ensureWifi();
  void startFetch();
  void launchFetch();
  void finishFetch(sky::FetchResult result);
  void rebuildRows();
};
