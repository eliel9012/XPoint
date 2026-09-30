#include <gtest/gtest.h>

#include <string>

#include "notifications/NotificationCenter.h"

namespace {

constexpr StrId kSnapshot = StrId::STR_READER_PREFERENCES;
constexpr StrId kEvent = StrId::STR_READER_PREFERENCES_UPDATED;

class NotificationCenterTest : public ::testing::Test {
 protected:
  void SetUp() override { NOTIFICATION_CENTER.clear(); }
  void TearDown() override { NOTIFICATION_CENTER.clear(); }
};

TEST_F(NotificationCenterTest, UnchangedSnapshotPreservesReadStateSequenceAndOrder) {
  NOTIFICATION_CENTER.upsert(kSnapshot, "Fonte A");
  NOTIFICATION_CENTER.markRead(0);
  const auto sequence = NOTIFICATION_CENTER.at(0).sequence;
  NOTIFICATION_CENTER.post(kEvent, "Atualizado");

  NOTIFICATION_CENTER.upsert(kSnapshot, "Fonte A");

  EXPECT_EQ(NOTIFICATION_CENTER.count(), 2u);
  EXPECT_EQ(NOTIFICATION_CENTER.unreadCount(), 1u);
  EXPECT_EQ(NOTIFICATION_CENTER.at(0).message, kEvent);
  EXPECT_EQ(NOTIFICATION_CENTER.at(1).sequence, sequence);
  EXPECT_TRUE(NOTIFICATION_CENTER.at(1).read);
}

TEST_F(NotificationCenterTest, ChangedSnapshotBecomesUnreadAndMovesToFront) {
  NOTIFICATION_CENTER.upsert(kSnapshot, "Fonte A");
  NOTIFICATION_CENTER.markRead(0);
  const auto sequence = NOTIFICATION_CENTER.at(0).sequence;
  NOTIFICATION_CENTER.post(kEvent);

  NOTIFICATION_CENTER.upsert(kSnapshot, "Fonte B");

  EXPECT_EQ(NOTIFICATION_CENTER.count(), 2u);
  EXPECT_EQ(NOTIFICATION_CENTER.unreadCount(), 2u);
  EXPECT_EQ(NOTIFICATION_CENTER.at(0).message, kSnapshot);
  EXPECT_GT(NOTIFICATION_CENTER.at(0).sequence, sequence);
  EXPECT_FALSE(NOTIFICATION_CENTER.at(0).read);
}

TEST_F(NotificationCenterTest, VisibleSnapshotDoesNotCreateUnreadOnOpenOrRefresh) {
  NOTIFICATION_CENTER.post(kEvent);
  NOTIFICATION_CENTER.upsert(kSnapshot, "Fonte A", true);
  EXPECT_EQ(NOTIFICATION_CENTER.unreadCount(), 1u);
  EXPECT_TRUE(NOTIFICATION_CENTER.at(0).read);

  NOTIFICATION_CENTER.upsert(kSnapshot, "Fonte A", true);
  EXPECT_EQ(NOTIFICATION_CENTER.unreadCount(), 1u);

  NOTIFICATION_CENTER.upsert(kSnapshot, "Fonte B", true);
  EXPECT_EQ(NOTIFICATION_CENTER.count(), 2u);
  EXPECT_EQ(NOTIFICATION_CENTER.unreadCount(), 1u);
  EXPECT_TRUE(NOTIFICATION_CENTER.at(0).read);
}

TEST_F(NotificationCenterTest, UnchangedUnreadSnapshotStaysUnreadOnOpen) {
  NOTIFICATION_CENTER.upsert(kSnapshot, "Fonte A");
  NOTIFICATION_CENTER.upsert(kSnapshot, "Fonte A", true);

  EXPECT_EQ(NOTIFICATION_CENTER.unreadCount(), 1u);
  EXPECT_FALSE(NOTIFICATION_CENTER.at(0).read);
}

TEST_F(NotificationCenterTest, StoredDetailLimitDeterminesWhetherContentChanged) {
  const std::string detail(NotificationCenter::DETAIL_CAPACITY - 1, 'A');
  NOTIFICATION_CENTER.upsert(kSnapshot, detail + "1");
  NOTIFICATION_CENTER.markRead(0);
  const auto sequence = NOTIFICATION_CENTER.at(0).sequence;

  NOTIFICATION_CENTER.upsert(kSnapshot, detail + "2");

  EXPECT_EQ(NOTIFICATION_CENTER.unreadCount(), 0u);
  EXPECT_EQ(NOTIFICATION_CENTER.at(0).sequence, sequence);
}

}  // namespace
