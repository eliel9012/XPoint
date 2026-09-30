#pragma once

#include "activities/Activity.h"

// Local diagnostics only. Input is matched against a fixed list; it is never
// passed to a shell, a network service, or a file operation.
class FirmwareTerminalActivity final : public Activity {
 public:
  FirmwareTerminalActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("FirmwareTerminal", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  static constexpr size_t MAX_COMMAND = 16;
  static constexpr size_t OUTPUT_LINES = 4;
  char command_[MAX_COMMAND + 1]{};
  char output_[OUTPUT_LINES][80]{};
  size_t outputCount_ = 0;

  void openKeyboard();
  void runCommand(const char* input);
  void addLine(const char* line);
};
