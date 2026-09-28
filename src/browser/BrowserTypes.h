#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace xpoint::browser {

// Capacities are deliberately fixed: browser input is untrusted and the
// firmware must never grow containers from page content.
constexpr size_t kMaxUrlLength = 192;
constexpr size_t kMaxTitleLength = 96;
constexpr size_t kMaxLinkUrlLength = 192;
constexpr size_t kMaxLinkTextLength = 80;
constexpr size_t kMaxDocumentText = 8192;
constexpr size_t kMaxDocumentLinks = 24;
constexpr size_t kMaxHistoryEntries = 24;
constexpr size_t kMaxBookmarks = 32;
constexpr size_t kMaxCacheEntries = 16;

template <size_t Capacity>
class FixedString {
  static_assert(Capacity > 1, "FixedString needs room for a terminator");

 public:
  FixedString() { clear(); }

  bool assign(const char* value) { return assign(value, value == nullptr ? 0 : std::strlen(value)); }

  bool assign(const char* value, size_t length) {
    if (value == nullptr) {
      clear();
      return false;
    }
    const size_t copied = length < Capacity - 1 ? length : Capacity - 1;
    if (copied != 0) std::memcpy(data_, value, copied);
    data_[copied] = '\0';
    length_ = copied;
    return copied == length;
  }

  bool append(const char value) {
    if (length_ + 1 >= Capacity) return false;
    data_[length_++] = value;
    data_[length_] = '\0';
    return true;
  }

  bool append(const char* value) { return append(value, value == nullptr ? 0 : std::strlen(value)); }

  bool append(const char* value, size_t length) {
    if (value == nullptr) return false;
    const size_t available = Capacity - 1 - length_;
    const size_t copied = length < available ? length : available;
    if (copied != 0) std::memcpy(data_ + length_, value, copied);
    length_ += copied;
    data_[length_] = '\0';
    return copied == length;
  }

  void clear() {
    data_[0] = '\0';
    length_ = 0;
  }

  const char* c_str() const { return data_; }
  char* data() { return data_; }
  size_t size() const { return length_; }
  size_t capacity() const { return Capacity - 1; }
  bool empty() const { return length_ == 0; }
  char operator[](const size_t index) const { return index < length_ ? data_[index] : '\0'; }

 private:
  char data_[Capacity]{};
  size_t length_ = 0;
};

template <typename T, size_t Capacity>
class FixedVector {
 public:
  bool push_back(const T& value) {
    if (size_ >= Capacity) return false;
    values_[size_++] = value;
    return true;
  }

  void clear() { size_ = 0; }
  size_t size() const { return size_; }
  constexpr size_t capacity() const { return Capacity; }
  bool empty() const { return size_ == 0; }
  bool full() const { return size_ == Capacity; }
  T& operator[](const size_t index) { return values_[index]; }
  const T& operator[](const size_t index) const { return values_[index]; }
  T* begin() { return values_; }
  T* end() { return values_ + size_; }
  const T* begin() const { return values_; }
  const T* end() const { return values_ + size_; }

 private:
  T values_[Capacity]{};
  size_t size_ = 0;
};

struct BrowserLink {
  FixedString<kMaxLinkUrlLength> url;
  FixedString<kMaxLinkTextLength> text;
};

struct BrowserDocument {
  FixedString<kMaxUrlLength> url;
  FixedString<kMaxTitleLength> title;
  FixedString<kMaxDocumentText> text;
  FixedVector<BrowserLink, kMaxDocumentLinks> links;
  bool textTruncated = false;
  bool linksTruncated = false;

  void clear() {
    url.clear();
    title.clear();
    text.clear();
    links.clear();
    textTruncated = false;
    linksTruncated = false;
  }
};

struct BrowserHistoryEntry {
  FixedString<kMaxUrlLength> url;
  FixedString<kMaxTitleLength> title;
};

struct BrowserBookmark {
  FixedString<kMaxUrlLength> url;
  FixedString<kMaxTitleLength> title;
};

struct BrowserCacheEntry {
  FixedString<kMaxUrlLength> url;
  uint32_t contentLength = 0;
  uint32_t storedAt = 0;
};

}  // namespace xpoint::browser
