#include "SkyActivity.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "sky/SkyTokenStore.h"

namespace fui = freeink::ui;

SkyActivity::SkyActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("SKY", renderer, mappedInput) {}

void SkyActivity::onEnter() {
  UiListActivity::onEnter();
  snapshot.itemCount = 0;
  snapshot.reportedCount = 0;
  snapshot.timestamp[0] = '\0';
  wifiStartedByUs = false;
  state = SKY_TOKEN_STORE.hasToken() ? State::READY : State::NO_TOKEN;
  if (state == State::READY) ensureWifi();
  requestUpdate();
}

void SkyActivity::onExit() {
  // The downloader has no cancellation hook. Its bounded network timeout is
  // the only safe point after which Wi-Fi and the work buffer can be released.
  if (fetchWork != nullptr) {
    fetchWork->cancel.store(true, std::memory_order_relaxed);
    while (!fetchWork->done.load(std::memory_order_acquire)) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    delete fetchWork;
    fetchWork = nullptr;
  }

  Activity::onExit();
  if (wifiStartedByUs && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void SkyActivity::ensureWifi() {
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    startFetch();
    return;
  }

  wifiStartedByUs = true;
  startActivityForResult(
      std::make_unique<WifiSelectionActivity>(renderer, mappedInput), [this](const ActivityResult& result) {
        if (result.isCancelled || WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
          state = State::NO_WIFI;
          requestUpdate();
          return;
        }
        startFetch();
      });
}

void SkyActivity::startFetch() {
  state = State::LOADING;
  requestUpdate();
}

void SkyActivity::launchFetch() {
  // Release rebuildable glyph caches before allocating the HTTPS buffers.
  if (auto* fonts = renderer.getFontCacheManager()) fonts->releaseSdFontCaches();

  auto* work = new (std::nothrow) FetchWork;
  if (work == nullptr) {
    finishFetch(sky::FetchResult::LOW_MEMORY);
    return;
  }
  work->snapshot = &snapshot;
  const std::string& token = SKY_TOKEN_STORE.getToken();
  std::strncpy(work->token, token.c_str(), sizeof(work->token) - 1);
  if (xTaskCreate(
          [](void* arg) {
            auto* job = static_cast<FetchWork*>(arg);
            job->result = sky::fetch(job->token, *job->snapshot, &job->cancel);
            job->done.store(true, std::memory_order_release);
            vTaskDelete(nullptr);
          },
          "sky_fetch", 12288, work, 1, nullptr) != pdPASS) {
    LOG_ERR("SKY", "Could not start fetch task");
    delete work;
    finishFetch(sky::FetchResult::LOW_MEMORY);
    return;
  }
  fetchWork = work;
}

void SkyActivity::finishFetch(const sky::FetchResult result) {
  switch (result) {
    case sky::FetchResult::OK:
      state = State::DATA;
      rebuildRows();
      break;
    case sky::FetchResult::NO_TOKEN:
      state = State::NO_TOKEN;
      break;
    case sky::FetchResult::LOW_MEMORY:
    case sky::FetchResult::NETWORK_ERROR:
    case sky::FetchResult::INVALID_JSON:
      state = State::ERROR;
      break;
  }
  requestUpdate();
}

void SkyActivity::loop() {
  if (state == State::LOADING) {
    if (fetchWork == nullptr) {
      launchFetch();
    } else if (fetchWork->done.load(std::memory_order_acquire)) {
      sky::FetchResult result = fetchWork->result;
      delete fetchWork;
      fetchWork = nullptr;
      finishFetch(result);
    }
  }
  UiListActivity::loop();
}

int SkyActivity::listCount() const { return state == State::DATA ? snapshot.itemCount : 0; }

void SkyActivity::rebuildRows() {
  for (size_t i = 0; i < snapshot.itemCount; ++i) {
    const sky::Aircraft& aircraft = snapshot.items[i];
    rowItems[i].label = aircraft.callsign[0] != '\0' ? aircraft.callsign : aircraft.hex;
    rowItems[i].actionValue = static_cast<int16_t>(i);
    std::snprintf(rowSubtitles[i], sizeof(rowSubtitles[i]), "%s %s  %ld ft  %ld kt  %ld nm", aircraft.airline,
                  aircraft.model, static_cast<long>(aircraft.altitudeFt), static_cast<long>(aircraft.speedKt),
                  static_cast<long>(aircraft.distanceNm));
    rowItems[i].subtitle = rowSubtitles[i];
  }
}

void SkyActivity::activateIndex(const int) {
  // The first SKY cut is intentionally read-only; the API data is refreshed
  // when the screen is opened so no per-aircraft action needs extra state.
  app.clearTapFlash();
}

const char* SkyActivity::headerTitle() const { return tr(STR_SKY); }

void SkyActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (state == State::LOADING || state == State::READY) {
    screen.centeredText(tr(STR_SKY_LOADING));
    return;
  }
  if (state == State::NO_TOKEN) {
    screen.centeredText(tr(STR_SKY_NO_TOKEN));
    return;
  }
  if (state == State::NO_WIFI) {
    screen.centeredText(tr(STR_SKY_NO_WIFI));
    return;
  }
  if (state == State::ERROR) {
    screen.centeredText(tr(STR_SKY_REQUEST_FAILED));
    return;
  }
  if (snapshot.itemCount == 0) {
    screen.centeredText(tr(STR_SKY_NO_AIRCRAFT));
    return;
  }

  fui::ListProps props;
  props.items = rowItems;
  props.count = snapshot.itemCount;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.subtitleText = screen.theme().smallText;
  props.subtitleText.maxLines = 1;
  syncListViewport(screen, props);
  screen.list(props);
}
