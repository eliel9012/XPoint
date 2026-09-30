#include "NotificationCenter.h"

#include <algorithm>
#include <cstring>

NotificationCenter& NotificationCenter::getInstance() {
  static NotificationCenter center;
  return center;
}

void NotificationCenter::writeEntry(Entry& entry, const StrId message, const std::string_view detail, const bool read) {
  entry.message = message;
  const size_t length = std::min(detail.size(), DETAIL_CAPACITY - 1);
  if (length > 0) memcpy(entry.detail, detail.data(), length);
  entry.detail[length] = '\0';
  entry.sequence = nextSequence_++;
  if (nextSequence_ == 0) nextSequence_ = 1;
  entry.read = read;
}

void NotificationCenter::moveToFront(const size_t index) {
  if (index == 0 || index >= count_) return;
  const Entry moved = entries_[index];
  for (size_t i = index; i > 0; --i) entries_[i] = entries_[i - 1];
  entries_[0] = moved;
}

void NotificationCenter::post(const StrId message, const std::string_view detail) {
  if (count_ < CAPACITY) {
    for (size_t i = count_; i > 0; --i) entries_[i] = entries_[i - 1];
    writeEntry(entries_[0], message, detail);
    ++count_;
    return;
  }

  for (size_t i = CAPACITY - 1; i > 0; --i) entries_[i] = entries_[i - 1];
  writeEntry(entries_[0], message, detail);
}

void NotificationCenter::upsert(const StrId message, const std::string_view detail, const bool readOnChange) {
  for (size_t i = 0; i < count_; ++i) {
    if (entries_[i].message != message) continue;
    if (std::string_view(entries_[i].detail) == detail.substr(0, DETAIL_CAPACITY - 1)) return;
    writeEntry(entries_[i], message, detail, readOnChange);
    moveToFront(i);
    return;
  }
  post(message, detail);
  if (readOnChange) entries_[0].read = true;
}

const NotificationCenter::Entry& NotificationCenter::at(const size_t index) const {
  static const Entry empty{};
  return index < count_ ? entries_[index] : empty;
}

size_t NotificationCenter::unreadCount() const {
  size_t unread = 0;
  for (size_t i = 0; i < count_; ++i) unread += entries_[i].read ? 0 : 1;
  return unread;
}

void NotificationCenter::markRead(const size_t index) {
  if (index < count_) entries_[index].read = true;
}

void NotificationCenter::markAllRead() {
  for (size_t i = 0; i < count_; ++i) entries_[i].read = true;
}

void NotificationCenter::clear() {
  count_ = 0;
  nextSequence_ = 1;
}
