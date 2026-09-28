#pragma once

#include <string>

class SleepWallpaper final {
 public:
  enum class Kind : unsigned char { None, Asset, LastBookCover, LastBookTitle };

  struct Fallback {
    const char* coverBmpPath = nullptr;
    const char* title = nullptr;
  };

  struct Selection {
    Kind kind = Kind::None;
    std::string path;
    const char* title = nullptr;

    bool hasImage() const { return kind == Kind::Asset || kind == Kind::LastBookCover; }
    explicit operator bool() const { return kind != Kind::None; }
  };

  // Selects only streamable, e-ink-safe BMP input. Selection does not decode
  // an image or allocate a framebuffer; the caller owns the render pass.
  static Selection select(const Fallback& fallback, bool commitRecent = true);
};
