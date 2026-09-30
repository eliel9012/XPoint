#include <gtest/gtest.h>

#include "SkyTokenStore.h"
#include "stubs/TestStorage.h"

namespace {
std::string sample() { return std::string(48, 'x'); }
std::string encodedSample() { return "enc:" + sample(); }

class SkyTokenStoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fake::reset();
    SKY_TOKEN_STORE.setToken("");
  }
};

TEST_F(SkyTokenStoreTest, MigratesLegacyAndRemovesSdOnlyAfterVerifiedNvsWrite) {
  fake::sdExists = true;
  fake::sdDocument["password_obf"] = encodedSample();
  ASSERT_TRUE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_EQ(SKY_TOKEN_STORE.getToken(), sample());
  EXPECT_TRUE(fake::nvsPresent);
  EXPECT_FALSE(fake::sdExists);
}

TEST_F(SkyTokenStoreTest, KeepsLegacyWhenNvsWriteFails) {
  fake::sdExists = true;
  fake::sdDocument["password_obf"] = encodedSample();
  fake::nvsWriteFails = true;
  ASSERT_TRUE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_EQ(SKY_TOKEN_STORE.getToken(), sample());
  EXPECT_TRUE(fake::sdExists);
}

TEST_F(SkyTokenStoreTest, KeepsLegacyWhenNvsReadbackFails) {
  fake::sdExists = true;
  fake::sdDocument["password_obf"] = encodedSample();
  fake::nvsReadFails = true;
  ASSERT_TRUE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_TRUE(fake::sdExists);
}

TEST_F(SkyTokenStoreTest, SaveFailureKeepsLegacyFile) {
  fake::sdExists = true;
  fake::sdDocument["password_obf"] = encodedSample();
  SKY_TOKEN_STORE.setToken(sample());
  fake::nvsWriteFails = true;
  EXPECT_FALSE(SKY_TOKEN_STORE.saveToFile());
  EXPECT_TRUE(fake::sdExists);
  EXPECT_FALSE(fake::nvsPresent);
}

TEST_F(SkyTokenStoreTest, NvsWinsAndRetriesLegacyCleanup) {
  SKY_TOKEN_STORE.setToken(sample());
  ASSERT_TRUE(SKY_TOKEN_STORE.saveToFile());
  fake::sdExists = true;
  fake::sdDocument["password_obf"] = "enc:" + std::string(32, 'z');
  SKY_TOKEN_STORE.setToken("");
  ASSERT_TRUE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_EQ(SKY_TOKEN_STORE.getToken(), sample());
  EXPECT_FALSE(fake::sdExists);
}

TEST_F(SkyTokenStoreTest, EmptyTokenCannotResurrectLegacy) {
  fake::sdExists = true;
  fake::sdDocument["password_obf"] = encodedSample();
  SKY_TOKEN_STORE.setToken("");
  fake::sdRemoveFails = true;
  ASSERT_TRUE(SKY_TOKEN_STORE.saveToFile());
  ASSERT_TRUE(fake::sdExists);
  fake::sdRemoveFails = false;
  SKY_TOKEN_STORE.setToken(sample());
  ASSERT_TRUE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_FALSE(SKY_TOKEN_STORE.hasToken());
  EXPECT_FALSE(fake::sdExists);
}

TEST_F(SkyTokenStoreTest, NewTokenNeverWritesSd) {
  SKY_TOKEN_STORE.setToken(sample());
  ASSERT_TRUE(SKY_TOKEN_STORE.saveToFile());
  EXPECT_FALSE(fake::sdExists);
  SKY_TOKEN_STORE.setToken("");
  ASSERT_TRUE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_EQ(SKY_TOKEN_STORE.getToken(), sample());
}

TEST_F(SkyTokenStoreTest, InvalidNvsFallsBackToLegacyWithoutDeletingItOnFailure) {
  fake::nvsPresent = true;
  fake::nvsBlob.assign(1, '\x02');
  fake::sdExists = true;
  fake::sdDocument["password_obf"] = encodedSample();
  fake::nvsWriteFails = true;
  ASSERT_TRUE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_EQ(SKY_TOKEN_STORE.getToken(), sample());
  EXPECT_TRUE(fake::sdExists);
}

TEST_F(SkyTokenStoreTest, RejectsMalformedLegacyWithoutErasingSdOrOverwritingMemory) {
  SKY_TOKEN_STORE.setToken(sample());
  fake::sdExists = true;
  fake::sdDocument["password_obf"] = "malformed";
  EXPECT_FALSE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_TRUE(fake::sdExists);
  EXPECT_FALSE(fake::nvsPresent);
  EXPECT_EQ(SKY_TOKEN_STORE.getToken(), sample());
}

TEST_F(SkyTokenStoreTest, RejectsOversizedDecodedLegacyEvenWithPlaintextFallback) {
  fake::sdExists = true;
  fake::sdDocument["password_obf"] = "enc:" + std::string(SkyTokenStore::MAX_TOKEN_LENGTH + 1, 'x');
  fake::sdDocument["password"] = sample();
  EXPECT_FALSE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_TRUE(fake::sdExists);
  EXPECT_FALSE(fake::nvsPresent);
}

TEST_F(SkyTokenStoreTest, MigratesPlaintextLegacyWhenObfuscatedValueIsInvalid) {
  fake::sdExists = true;
  fake::sdDocument["password_obf"] = "malformed";
  fake::sdDocument["password"] = sample();
  ASSERT_TRUE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_EQ(SKY_TOKEN_STORE.getToken(), sample());
  EXPECT_TRUE(fake::nvsPresent);
  EXPECT_FALSE(fake::sdExists);
}

TEST_F(SkyTokenStoreTest, MigratesLegacyEmptyToken) {
  fake::sdExists = true;
  fake::sdDocument["password_obf"] = "";
  ASSERT_TRUE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_FALSE(SKY_TOKEN_STORE.hasToken());
  EXPECT_TRUE(fake::nvsPresent);
  EXPECT_FALSE(fake::sdExists);
}

TEST_F(SkyTokenStoreTest, PlaintextTakesPrecedenceOverEmptyObfuscatedValue) {
  fake::sdExists = true;
  fake::sdDocument["password_obf"] = "";
  fake::sdDocument["password"] = sample();
  ASSERT_TRUE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_EQ(SKY_TOKEN_STORE.getToken(), sample());
  EXPECT_FALSE(fake::sdExists);
}

TEST_F(SkyTokenStoreTest, RejectsMalformedNvsWithoutLegacy) {
  fake::nvsPresent = true;
  fake::nvsBlob.assign(1, '\x02');
  EXPECT_FALSE(SKY_TOKEN_STORE.loadFromFile());
  EXPECT_FALSE(SKY_TOKEN_STORE.hasToken());
}
}  // namespace
