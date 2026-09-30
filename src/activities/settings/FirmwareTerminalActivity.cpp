#include "FirmwareTerminalActivity.h"

#include <Arduino.h>
#include <BoardConfig.h>
#include <I18n.h>
#include <Memory.h>
#include <WiFi.h>

#include <cctype>
#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void FirmwareTerminalActivity::onEnter() {
  Activity::onEnter();
  command_[0] = '\0';
  outputCount_ = 0;
  addLine(tr(STR_TERMINAL_WELCOME));
  addLine(tr(STR_TERMINAL_COMMANDS));
  requestUpdate();
}

void FirmwareTerminalActivity::addLine(const char* line) {
  if (outputCount_ >= OUTPUT_LINES) return;
  snprintf(output_[outputCount_++], sizeof(output_[0]), "%s", line);
}

void FirmwareTerminalActivity::runCommand(const char* input) {
  // The keyboard caps input at MAX_COMMAND. Normalize only ASCII command
  // names, then require an exact match so arguments cannot trigger actions.
  size_t start = 0;
  const size_t length = strlen(input);
  while (start < length && input[start] == ' ') ++start;
  size_t end = length;
  while (end > start && input[end - 1] == ' ') --end;
  const size_t count = end - start;
  if (count > MAX_COMMAND) return;
  for (size_t i = 0; i < count; ++i) {
    const unsigned char ch = static_cast<unsigned char>(input[start + i]);
    command_[i] = ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : static_cast<char>(ch);
  }
  command_[count] = '\0';
  outputCount_ = 0;

  if (strcmp(command_, "help") == 0 || command_[0] == '\0') {
    addLine(tr(STR_TERMINAL_COMMANDS));
    addLine(tr(STR_TERMINAL_HELP));
  } else if (strcmp(command_, "status") == 0) {
    char line[80];
    snprintf(line, sizeof(line), tr(STR_TERMINAL_DEVICE_FMT), BoardConfig::ACTIVE.name);
    addLine(line);
    snprintf(line, sizeof(line), tr(STR_TERMINAL_UPTIME_FMT), static_cast<unsigned long>(millis() / 1000));
    addLine(line);
  } else if (strcmp(command_, "memory") == 0) {
    char line[80];
    snprintf(line, sizeof(line), tr(STR_TERMINAL_HEAP_FMT), static_cast<unsigned>(ESP.getFreeHeap()));
    addLine(line);
    snprintf(line, sizeof(line), tr(STR_TERMINAL_MIN_HEAP_FMT), static_cast<unsigned>(ESP.getMinFreeHeap()));
    addLine(line);
  } else if (strcmp(command_, "wifi") == 0) {
    // No scan, connection attempt, SSID, address, or credential access.
    addLine(WiFi.status() == WL_CONNECTED ? tr(STR_TERMINAL_WIFI_CONNECTED) : tr(STR_TERMINAL_WIFI_DISCONNECTED));
  } else if (strcmp(command_, "version") == 0) {
    char line[80];
    snprintf(line, sizeof(line), tr(STR_TERMINAL_VERSION_FMT), CROSSPOINT_VERSION);
    addLine(line);
  } else {
    addLine(tr(STR_TERMINAL_UNKNOWN));
  }
  requestUpdate();
}

void FirmwareTerminalActivity::openKeyboard() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_TERMINAL_INPUT), "",
                                                           MAX_COMMAND, InputType::Text);
  if (!keyboard) {
    outputCount_ = 0;
    addLine(tr(STR_TERMINAL_NO_MEMORY));
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) return;
    if (const auto* typed = std::get_if<KeyboardResult>(&result.data)) runCommand(typed->text.c_str());
  });
}

void FirmwareTerminalActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  int x = 0;
  int y = 0;
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y)) {
    openKeyboard();
  }
}

void FirmwareTerminalActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, tr(STR_FIRMWARE_TERMINAL));
  renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, y, tr(STR_TERMINAL_LOCAL_ONLY));
  y += lineHeight + metrics.verticalSpacing;
  renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, y, "> ");
  renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding + 25, y, command_);
  y += lineHeight + metrics.verticalSpacing;
  for (size_t i = 0; i < outputCount_ && y + lineHeight < height - metrics.buttonHintsHeight; ++i) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, y, output_[i]);
    y += lineHeight + metrics.verticalSpacing;
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_TERMINAL_INPUT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
