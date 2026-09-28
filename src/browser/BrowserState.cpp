#include "BrowserState.h"

#include <cstring>

namespace xpoint::browser {

void BrowserHistory::clear() {
  entries_.clear();
  position_ = 0;
}

bool BrowserHistory::navigate(const char* url, const char* title) {
  if (url == nullptr || url[0] == '\0') return false;
  while (entries_.size() > position_ + (entries_.empty() ? 0 : 1)) {
    BrowserHistoryEntry discarded;
    (void)discarded;
    // FixedVector intentionally has no erase API; rebuild the bounded tail.
    FixedVector<BrowserHistoryEntry, kMaxHistoryEntries> kept;
    for (size_t i = 0; i <= position_ && i < entries_.size(); ++i) kept.push_back(entries_[i]);
    entries_ = kept;
    break;
  }
  if (!entries_.empty() && position_ < entries_.size() && std::strcmp(entries_[position_].url.c_str(), url) == 0) {
    entries_[position_].title.assign(title);
    return true;
  }
  BrowserHistoryEntry entry;
  entry.url.assign(url);
  entry.title.assign(title);
  if (entries_.full()) {
    FixedVector<BrowserHistoryEntry, kMaxHistoryEntries> shifted;
    for (size_t i = 1; i < entries_.size(); ++i) shifted.push_back(entries_[i]);
    shifted.push_back(entry);
    entries_ = shifted;
    position_ = entries_.size() - 1;
  } else {
    entries_.push_back(entry);
    position_ = entries_.size() - 1;
  }
  return true;
}

bool BrowserHistory::canGoBack() const { return !entries_.empty() && position_ > 0; }
bool BrowserHistory::canGoForward() const { return !entries_.empty() && position_ + 1 < entries_.size(); }

bool BrowserHistory::back(BrowserHistoryEntry& out) {
  if (!canGoBack()) return false;
  --position_;
  out = entries_[position_];
  return true;
}

bool BrowserHistory::forward(BrowserHistoryEntry& out) {
  if (!canGoForward()) return false;
  ++position_;
  out = entries_[position_];
  return true;
}

bool BrowserBookmarks::contains(const char* url) const {
  if (url == nullptr) return false;
  for (const auto& entry : entries_) {
    if (std::strcmp(entry.url.c_str(), url) == 0) return true;
  }
  return false;
}

bool BrowserBookmarks::add(const char* url, const char* title) {
  if (url == nullptr || url[0] == '\0') return false;
  if (contains(url)) return true;
  BrowserBookmark bookmark;
  bookmark.url.assign(url);
  bookmark.title.assign(title);
  return entries_.push_back(bookmark);
}

bool BrowserBookmarks::remove(const char* url) {
  if (url == nullptr) return false;
  FixedVector<BrowserBookmark, kMaxBookmarks> kept;
  bool removed = false;
  for (const auto& entry : entries_) {
    if (std::strcmp(entry.url.c_str(), url) == 0) removed = true;
    else kept.push_back(entry);
  }
  entries_ = kept;
  return removed;
}

const BrowserBookmark* BrowserBookmarks::at(const size_t index) const {
  return index < entries_.size() ? &entries_[index] : nullptr;
}

bool BrowserCacheIndex::remember(const char* url, const uint32_t contentLength, const uint32_t storedAt) {
  if (url == nullptr || url[0] == '\0') return false;
  for (auto& entry : entries_) {
    if (std::strcmp(entry.url.c_str(), url) == 0) {
      entry.contentLength = contentLength;
      entry.storedAt = storedAt;
      return true;
    }
  }
  BrowserCacheEntry entry;
  entry.url.assign(url);
  entry.contentLength = contentLength;
  entry.storedAt = storedAt;
  if (entries_.full()) {
    FixedVector<BrowserCacheEntry, kMaxCacheEntries> shifted;
    for (size_t i = 1; i < entries_.size(); ++i) shifted.push_back(entries_[i]);
    entries_ = shifted;
  }
  return entries_.push_back(entry);
}

bool BrowserCacheIndex::forget(const char* url) {
  if (url == nullptr) return false;
  FixedVector<BrowserCacheEntry, kMaxCacheEntries> kept;
  bool removed = false;
  for (const auto& entry : entries_) {
    if (std::strcmp(entry.url.c_str(), url) == 0) removed = true;
    else kept.push_back(entry);
  }
  entries_ = kept;
  return removed;
}

bool BrowserCacheIndex::lookup(const char* url, BrowserCacheEntry& out) const {
  if (url == nullptr) return false;
  for (const auto& entry : entries_) {
    if (std::strcmp(entry.url.c_str(), url) == 0) {
      out = entry;
      return true;
    }
  }
  return false;
}

}  // namespace xpoint::browser
