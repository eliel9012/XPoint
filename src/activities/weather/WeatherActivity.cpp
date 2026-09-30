#include "WeatherActivity.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <cstdio>
#include <variant>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "weather/WeatherStore.h"

namespace fui = freeink::ui;

WeatherActivity::WeatherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("Weather", renderer, mappedInput) {}

void WeatherActivity::onEnter() {
  UiListActivity::onEnter();
  WEATHER_STORE.loadFromFile();
  wifiStartedByUs = false;
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    beginFetch();
  } else {
    state = State::OFFLINE;
    requestUpdate();
  }
}

void WeatherActivity::onExit() {
  if (fetchWork) {
    fetchWork->cancel.store(true, std::memory_order_relaxed);
    while (!fetchWork->done.load(std::memory_order_acquire)) vTaskDelay(pdMS_TO_TICKS(10));
    fetchWork.reset();
  }
  Activity::onExit();
  if (wifiStartedByUs && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void WeatherActivity::beginFetch() {
  if (fetchWork) return;
  state = State::LOADING;
  requestUpdate();
}

void WeatherActivity::launchFetch() {
  if (auto* fonts = renderer.getFontCacheManager()) fonts->releaseSdFontCaches();
  auto work = makeUniqueNoThrow<FetchWork>();
  if (!work) {
    LOG_ERR("WEATHER", "OOM: fetch work");
    state = State::LOW_MEMORY;
    requestUpdate();
    return;
  }
  work->destination = &snapshot;
  work->latitude = WEATHER_STORE.latitude();
  work->longitude = WEATHER_STORE.longitude();
  if (xTaskCreatePinnedToCore(
          [](void* arg) {
            auto* job = static_cast<FetchWork*>(arg);
            job->result = weather::fetch(job->latitude, job->longitude, *job->destination, job->cancel);
            job->done.store(true, std::memory_order_release);
            vTaskDelete(nullptr);
          },
          "weather_fetch", 12288, work.get(), 1, nullptr, 0) != pdPASS) {
    LOG_ERR("WEATHER", "Could not start fetch task");
    state = State::LOW_MEMORY;
    requestUpdate();
    return;
  }
  fetchWork = std::move(work);
}

void WeatherActivity::finishFetch(weather::FetchResult result) {
  switch (result) {
    case weather::FetchResult::OK:
      state = State::DATA;
      break;
    case weather::FetchResult::NO_WIFI:
      state = State::OFFLINE;
      break;
    case weather::FetchResult::LOW_MEMORY:
      state = State::LOW_MEMORY;
      break;
    case weather::FetchResult::NETWORK_ERROR:
    case weather::FetchResult::INVALID_DATA:
      state = State::ERROR;
      break;
  }
  requestUpdate();
}

void WeatherActivity::loop() {
  if (state == State::DATA && WiFi.status() != WL_CONNECTED) {
    state = State::OFFLINE;
    requestUpdate();
  }
  if (state == State::LOADING) {
    if (!fetchWork) {
      launchFetch();
    } else if (fetchWork->done.load(std::memory_order_acquire)) {
      const auto result = fetchWork->result;
      fetchWork.reset();
      finishFetch(result);
    }
  }
  UiListActivity::loop();
}

int WeatherActivity::listCount() const { return state == State::DATA ? 6 : 3; }

const char* WeatherActivity::condition(int code) {
  if (code == 0 || code == 1) return tr(STR_WEATHER_CLEAR);
  if (code == 2 || code == 3) return tr(STR_WEATHER_CLOUDY);
  if (code == 45 || code == 48) return tr(STR_WEATHER_FOG);
  if (code >= 51 && code <= 67) return tr(STR_WEATHER_RAIN);
  if (code >= 71 && code <= 77) return tr(STR_WEATHER_SNOW);
  if (code >= 80 && code <= 86) return tr(STR_WEATHER_SHOWERS);
  if (code >= 95 && code <= 99) return tr(STR_WEATHER_STORM);
  return tr(STR_WEATHER_UNKNOWN);
}

void WeatherActivity::rebuildRows() {
  for (int i = 0; i < listCount(); ++i) {
    rows[i].label = labels[i];
    rows[i].subtitle = subtitles[i];
    rows[i].actionValue = static_cast<int16_t>(i);
    labels[i][0] = '\0';
    subtitles[i][0] = '\0';
  }
  int actions = 1;
  if (state == State::DATA) {
    std::snprintf(labels[0], sizeof(labels[0]), "%s %s  %s", tr(STR_WEATHER_NOW), snapshot.time + 11,
                  condition(snapshot.code));
    std::snprintf(subtitles[0], sizeof(subtitles[0]), "%.0f°C  %s %.0f°C  %s %d%%  %s %.0f km/h", snapshot.temperature,
                  tr(STR_WEATHER_FEELS), snapshot.feelsLike, tr(STR_WEATHER_HUMIDITY), snapshot.humidity,
                  tr(STR_WEATHER_WIND), snapshot.windKmh);
    for (int i = 0; i < weather::FORECAST_DAYS; ++i) {
      const auto& day = snapshot.days[i];
      std::snprintf(labels[i + 1], sizeof(labels[i + 1]), "%s  %s", day.date, condition(day.code));
      std::snprintf(subtitles[i + 1], sizeof(subtitles[i + 1]), "%.0f° / %.0f°C  %s %d%%", day.high, day.low,
                    tr(STR_WEATHER_RAIN_CHANCE), day.rainChance);
    }
    actions = 4;
  } else {
    const char* message = tr(STR_WEATHER_LOADING);
    if (state == State::OFFLINE) message = tr(STR_WEATHER_OFFLINE);
    if (state == State::ERROR) message = tr(STR_WEATHER_ERROR);
    if (state == State::LOW_MEMORY) message = tr(STR_WEATHER_LOW_MEMORY);
    if (state == State::INVALID_LOCATION) message = tr(STR_WEATHER_INVALID_LOCATION);
    if (state == State::SAVE_ERROR) message = tr(STR_WEATHER_SAVE_ERROR);
    std::snprintf(labels[0], sizeof(labels[0]), "%s", message);
  }
  std::snprintf(labels[actions], sizeof(labels[actions]), "%s", tr(STR_WEATHER_LOCATION));
  std::snprintf(subtitles[actions], sizeof(subtitles[actions]), "%s  %.4f, %.4f",
                WEATHER_STORE.isDefault() ? tr(STR_WEATHER_SAO_PAULO) : tr(STR_WEATHER_COORDINATES),
                WEATHER_STORE.latitude(), WEATHER_STORE.longitude());
  std::snprintf(labels[actions + 1], sizeof(labels[actions + 1]), "%s",
                WiFi.status() != WL_CONNECTED ? tr(STR_WEATHER_CONNECT) : tr(STR_WEATHER_REFRESH));
}

void WeatherActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  rebuildRows();
  fui::ListProps props;
  props.items = rows;
  props.count = listCount();
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.subtitleText = screen.theme().smallText;
  props.subtitleText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}

void WeatherActivity::activateIndex(int index) {
  app.clearTapFlash();
  if (state == State::LOADING) return;
  const int actions = state == State::DATA ? 4 : 1;
  if (index == actions) {
    editCoordinates();
  } else if (index == actions + 1) {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0))
      beginFetch();
    else
      connectWifi();
  }
}

void WeatherActivity::editCoordinates() {
  char initial[32];
  std::snprintf(initial, sizeof(initial), "%.4f,%.4f", WEATHER_STORE.latitude(), WEATHER_STORE.longitude());
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_WEATHER_LOCATION), initial, 31,
                                                           InputType::Text);
  if (!keyboard) {
    LOG_ERR("WEATHER", "OOM: location keyboard");
    state = State::LOW_MEMORY;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) return;
    const auto& value = std::get<KeyboardResult>(result.data).text;
    if (!WEATHER_STORE.setCoordinates(value.c_str())) {
      state = State::INVALID_LOCATION;
    } else if (!WEATHER_STORE.saveToFile()) {
      LOG_ERR("WEATHER", "Could not save location");
      state = State::SAVE_ERROR;
    } else if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
      beginFetch();
    } else {
      state = State::OFFLINE;
    }
    requestUpdate();
  });
}

void WeatherActivity::connectWifi() {
  auto selector = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput);
  if (!selector) {
    LOG_ERR("WEATHER", "OOM: Wi-Fi selector");
    state = State::LOW_MEMORY;
    requestUpdate();
    return;
  }
  wifiStartedByUs = true;
  startActivityForResult(std::move(selector), [this](const ActivityResult& result) {
    if (!result.isCancelled && WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0))
      beginFetch();
    else {
      state = State::OFFLINE;
      requestUpdate();
    }
  });
}

const char* WeatherActivity::headerTitle() const { return tr(STR_WEATHER); }
