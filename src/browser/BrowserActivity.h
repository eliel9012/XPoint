#pragma once

#include "BrowserCore.h"
#include "activities/UiListActivity.h"

/**
 * Minimal e-ink browser screen. Integration only needs to construct it and
 * register it in its normal Activity flow; no ActivityManager changes are
 * required here.
 */
class BrowserActivity final : public UiListActivity {
 public:
  BrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* initialUrl = nullptr);

  void onEnter() override;
  void openUrl(const char* url);
  xpoint::browser::BrowserCore& browser() { return browser_; }
  const xpoint::browser::BrowserCore& browser() const { return browser_; }

 protected:
  const char* headerTitle() const override { return "Navegador"; }
  void buildScreen(UiScreen& screen) override;
  int listCount() const override;
  void activateIndex(int index) override;
  void onBackButton() override;
  bool handleCustomInput() override;
  void navigateButtons() override;
  bool preventAutoSleep() override { return true; }

 private:
  void promptUrl();
  void ensureWifiAndOpen();
  void launchWifiSelection();
  void rebuildRows();
  bool scrollText(int direction);
  void navigateDocument(int direction, bool page);

  xpoint::browser::BrowserCore browser_;
  xpoint::browser::FixedString<xpoint::browser::kMaxUrlLength> initialUrl_;
  freeink::ui::ListItem rows_[xpoint::browser::kMaxDocumentLinks]{};
  bool waitingForUrl_ = false;
  bool waitingForWifi_ = false;
  uint32_t textTopLine_ = 0;
  uint32_t textLineCount_ = 0;
  uint16_t textVisibleLines_ = 0;
  bool linksFocused_ = false;
};
