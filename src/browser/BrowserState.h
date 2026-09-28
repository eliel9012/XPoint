#pragma once

#include <cstddef>
#include <cstdint>

#include "BrowserTypes.h"

namespace xpoint::browser {

class BrowserHistory {
 public:
  void clear();
  bool navigate(const char* url, const char* title);
  bool canGoBack() const;
  bool canGoForward() const;
  bool back(BrowserHistoryEntry& out);
  bool forward(BrowserHistoryEntry& out);
  size_t size() const { return entries_.size(); }
  size_t position() const { return position_; }

 private:
  FixedVector<BrowserHistoryEntry, kMaxHistoryEntries> entries_;
  size_t position_ = 0;
};

class BrowserBookmarks {
 public:
  bool add(const char* url, const char* title);
  bool remove(const char* url);
  bool contains(const char* url) const;
  void clear() { entries_.clear(); }
  size_t size() const { return entries_.size(); }
  const BrowserBookmark* at(size_t index) const;

 private:
  FixedVector<BrowserBookmark, kMaxBookmarks> entries_;
};

/** Metadata-only cache index. Payload storage can be SD, LittleFS, or PSRAM. */
class BrowserCacheIndex {
 public:
  bool remember(const char* url, uint32_t contentLength, uint32_t storedAt);
  bool forget(const char* url);
  bool lookup(const char* url, BrowserCacheEntry& out) const;
  void clear() { entries_.clear(); }
  size_t size() const { return entries_.size(); }

 private:
  FixedVector<BrowserCacheEntry, kMaxCacheEntries> entries_;
};

using BrowserCache = BrowserCacheIndex;

/**
 * Optional payload backend. BrowserCore never owns an unbounded cache; an
 * integration can implement this with an SD file, LittleFS file, or PSRAM.
 */
class IBrowserCacheStorage {
 public:
  virtual ~IBrowserCacheStorage() = default;
  virtual bool read(const char* url, uint8_t* destination, size_t capacity, size_t& length) = 0;
  virtual bool write(const char* url, const uint8_t* data, size_t length) = 0;
  virtual bool remove(const char* url) = 0;
};

}  // namespace xpoint::browser
