#include "NotificationCenterActivity.h"

#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "activities/reader/ReaderPreferences.h"
#include "components/UITheme.h"
#include "notifications/NotificationCenter.h"

namespace fui = freeink::ui;

namespace {
// BW1 icons keep read and unread labels aligned without adding UI text.
static constexpr uint8_t UNREAD_DOT[8] = {0x00, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x00};
static constexpr uint8_t READ_SPACER[8] = {};
}  // namespace

NotificationCenterActivity::NotificationCenterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("NotificationCenter", renderer, mappedInput) {}

void NotificationCenterActivity::onEnter() {
  char detail[NotificationCenter::DETAIL_CAPACITY] = {};
  ReaderPreferences::formatNotificationDetail(detail, sizeof(detail));
  NOTIFICATION_CENTER.upsert(StrId::STR_READER_PREFERENCES, detail, true);
  UiListActivity::onEnter();
}

int NotificationCenterActivity::listCount() const {
  return static_cast<int>(std::max<size_t>(1, NOTIFICATION_CENTER.count()));
}

const char* NotificationCenterActivity::headerTitle() const { return tr(STR_NOTIFICATIONS); }

void NotificationCenterActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});

  if (NOTIFICATION_CENTER.count() == 0) {
    screen.centeredText(tr(STR_NO_NOTIFICATIONS), screen.theme().bodyText);
    return;
  }

  for (size_t i = 0; i < NOTIFICATION_CENTER.count() && i < MAX_ROWS; ++i) {
    const auto& entry = NOTIFICATION_CENTER.at(i);
    rowItems_[i].label = I18N.get(entry.message);
    rowItems_[i].value = entry.detail[0] == '\0' ? nullptr : entry.detail;
    rowItems_[i].icon = fui::BitmapRef{entry.read ? READ_SPACER : UNREAD_DOT, 8, 8, fui::BitmapFormat::BW1};
    rowItems_[i].actionValue = static_cast<int16_t>(i);
    rowItems_[i].enabled = true;
  }

  fui::ListProps props;
  props.items = rowItems_;
  props.count = static_cast<uint16_t>(NOTIFICATION_CENTER.count());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}

void NotificationCenterActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(NOTIFICATION_CENTER.count())) return;
  NOTIFICATION_CENTER.markRead(static_cast<size_t>(index));
  app.clearTapFlash();
  requestUpdate();
}
