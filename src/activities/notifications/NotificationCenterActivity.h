#pragma once

#include "activities/UiListActivity.h"
#include "notifications/NotificationCenter.h"

class NotificationCenterActivity final : public UiListActivity {
 public:
  explicit NotificationCenterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  static constexpr int MAX_ROWS = 8;

  int listCount() const override;
  const char* headerTitle() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;

  freeink::ui::ListItem rowItems_[MAX_ROWS]{};
};
