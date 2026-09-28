#pragma once

#include <I18n.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "I18nKeys.h"

// A small, RAM-only notification history for user-visible events. The fixed
// storage is intentional: notifications are produced from render/input paths
// on boards with a very small DRAM budget, so this module must not allocate or
// write the SD card.
class NotificationCenter {
 public:
  static constexpr size_t CAPACITY = 8;
  static constexpr size_t DETAIL_CAPACITY = 112;

  struct Entry {
    StrId message = StrId::STR_NOTIFICATIONS;
    char detail[DETAIL_CAPACITY] = {};
    uint32_t sequence = 0;
    bool read = false;
  };

  static NotificationCenter& getInstance();

  void post(StrId message, std::string_view detail = {});
  // Keep one live snapshot per message. This is used for the reader
  // preferences row so opening the center refreshes it instead of growing
  // duplicate entries.
  void upsert(StrId message, std::string_view detail = {});

  size_t count() const { return count_; }
  const Entry& at(size_t index) const;
  size_t unreadCount() const;
  void markRead(size_t index);
  void markAllRead();
  void clear();

 private:
  NotificationCenter() = default;

  void writeEntry(Entry& entry, StrId message, std::string_view detail);
  void moveToFront(size_t index);

  Entry entries_[CAPACITY]{};
  size_t count_ = 0;
  uint32_t nextSequence_ = 1;
};

#define NOTIFICATION_CENTER NotificationCenter::getInstance()
