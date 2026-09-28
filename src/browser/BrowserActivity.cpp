#include "BrowserActivity.h"

#include <algorithm>
#include <WiFi.h>

#include <I18n.h>
#include "components/UITheme.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"

namespace fui = freeink::ui;
using xpoint::browser::kMaxDocumentLinks;
using xpoint::browser::kMaxUrlLength;

BrowserActivity::BrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* initialUrl)
    : UiListActivity("Browser", renderer, mappedInput) {
  initialUrl_.assign(initialUrl);
}

void BrowserActivity::onEnter() {
  UiListActivity::onEnter();
  if (!initialUrl_.empty()) {
    ensureWifiAndOpen();
  } else {
    promptUrl();
  }
}

void BrowserActivity::promptUrl() {
  waitingForUrl_ = true;
  auto keyboard = std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_BROWSER),
                                                          initialUrl_.c_str(), kMaxUrlLength - 1, InputType::Url);
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    waitingForUrl_ = false;
    if (result.isCancelled) {
      onBackButton();
      return;
    }
    const auto& keyboardResult = std::get<KeyboardResult>(result.data);
    initialUrl_.assign(keyboardResult.text.c_str());
    ensureWifiAndOpen();
  });
}

void BrowserActivity::ensureWifiAndOpen() {
  if (initialUrl_.empty()) {
    promptUrl();
    return;
  }
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    openUrl(initialUrl_.c_str());
    return;
  }
  launchWifiSelection();
}

void BrowserActivity::launchWifiSelection() {
  waitingForWifi_ = true;
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput), [this](const ActivityResult& result) {
    waitingForWifi_ = false;
    if (result.isCancelled || WiFi.status() != WL_CONNECTED) {
      requestUpdate();
      return;
    }
    openUrl(initialUrl_.c_str());
  });
}

void BrowserActivity::openUrl(const char* url) {
  if (url == nullptr || url[0] == '\0') return;
  browser_.open(url);
  activeNav().reset();
  rebuildRows();
  requestUpdate();
}

int BrowserActivity::listCount() const { return static_cast<int>(browser_.document().links.size()); }

void BrowserActivity::rebuildRows() {
  const auto& links = browser_.document().links;
  for (size_t i = 0; i < kMaxDocumentLinks; ++i) rows_[i] = {};
  for (size_t i = 0; i < links.size(); ++i) {
    rows_[i].label = links[i].text.empty() ? links[i].url.c_str() : links[i].text.c_str();
    rows_[i].subtitle = links[i].url.c_str();
    rows_[i].actionValue = static_cast<int16_t>(i);
  }
}

void BrowserActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  openUrl(browser_.document().links[static_cast<size_t>(index)].url.c_str());
}

void BrowserActivity::onBackButton() {
  if (browser_.goBack()) {
    activeNav().reset();
    rebuildRows();
    requestUpdate();
    return;
  }
  UiListActivity::onBackButton();
}

void BrowserActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                  static_cast<int16_t>(metrics.buttonHintsHeight), 0});

  const auto& document = browser_.document();
  const int16_t urlLineHeight = screen.target().lineHeight(screen.theme().smallText.font);
  screen.target().text(screen.takeTop(urlLineHeight, screen.theme().spaceSm), document.url.c_str(),
                       screen.theme().smallText);
  if (document.text.empty()) {
    screen.centeredText(document.url.empty() ? "Digite uma URL" : "Não foi possível carregar a página");
    return;
  }

  const int16_t linkBand = document.links.empty()
                               ? 0
                               : static_cast<int16_t>(std::min<int>(screen.body().height / 3,
                                                                      24 + static_cast<int>(document.links.size()) *
                                                                               screen.theme().rowHeight));
  fui::TextAreaProps text;
  text.text = document.text.c_str();
  text.style = screen.theme().bodyText;
  text.showCaret = false;
  text.topLine = 0;
  screen.textArea(text, static_cast<int16_t>(screen.body().height - linkBand));

  if (!document.links.empty()) {
    fui::ListProps props;
    props.items = rows_;
    props.count = static_cast<uint16_t>(document.links.size());
    props.action = ACTION_ROW;
    props.inputMask = fui::InputTouch;
    props.subtitleText = screen.theme().smallText;
    props.selectedIndex = activeNav().selected;
    props.topIndex = 0;
    screen.list(props);
  }
}
