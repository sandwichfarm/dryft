// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_security_utils.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class NsiteSecurityUtilsTest : public testing::Test {
 public:
  NsiteSecurityUtilsTest() = default;
  ~NsiteSecurityUtilsTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NsiteSecurityUtilsTest, IsPathSafe_ValidPaths) {
  EXPECT_TRUE(NsiteSecurityUtils::IsPathSafe("/"));
  EXPECT_TRUE(NsiteSecurityUtils::IsPathSafe("/index.html"));
  EXPECT_TRUE(NsiteSecurityUtils::IsPathSafe("/css/style.css"));
  EXPECT_TRUE(NsiteSecurityUtils::IsPathSafe("/js/app.js"));
  EXPECT_TRUE(NsiteSecurityUtils::IsPathSafe("/images/logo.png"));
}

TEST_F(NsiteSecurityUtilsTest, IsPathSafe_PathTraversal) {
  EXPECT_FALSE(NsiteSecurityUtils::IsPathSafe("../etc/passwd"));
  EXPECT_FALSE(NsiteSecurityUtils::IsPathSafe("/app/../etc/passwd"));
  EXPECT_FALSE(NsiteSecurityUtils::IsPathSafe("/.../etc/passwd"));
  EXPECT_FALSE(NsiteSecurityUtils::IsPathSafe("/app/....//etc/passwd"));
  EXPECT_FALSE(NsiteSecurityUtils::IsPathSafe("/app%2e%2e%2fetc%2fpasswd"));
}

TEST_F(NsiteSecurityUtilsTest, IsPathSafe_NullBytes) {
  std::string path_with_null = "/index.html";
  path_with_null[5] = '\0';
  EXPECT_FALSE(NsiteSecurityUtils::IsPathSafe(path_with_null));
}

TEST_F(NsiteSecurityUtilsTest, IsPathSafe_EmptyPath) {
  EXPECT_FALSE(NsiteSecurityUtils::IsPathSafe(""));
}

TEST_F(NsiteSecurityUtilsTest, SanitizePath_BasicCases) {
  EXPECT_EQ("/", NsiteSecurityUtils::SanitizePath(""));
  EXPECT_EQ("/index.html", NsiteSecurityUtils::SanitizePath("index.html"));
  EXPECT_EQ("/index.html", NsiteSecurityUtils::SanitizePath("/index.html"));
  EXPECT_EQ("/css/style.css", NsiteSecurityUtils::SanitizePath("//css//style.css"));
}

TEST_F(NsiteSecurityUtilsTest, SanitizePath_WindowsSeparators) {
  EXPECT_EQ("/css/style.css", NsiteSecurityUtils::SanitizePath("\\css\\style.css"));
}

TEST_F(NsiteSecurityUtilsTest, SanitizePath_TrailingSlash) {
  EXPECT_EQ("/css", NsiteSecurityUtils::SanitizePath("/css/"));
  EXPECT_EQ("/", NsiteSecurityUtils::SanitizePath("/"));
}

TEST_F(NsiteSecurityUtilsTest, IsValidNpub_ValidCases) {
  // Valid npub format (63 chars, starts with npub1, lowercase bech32)
  EXPECT_TRUE(NsiteSecurityUtils::IsValidNpub(
      "npub1234567890abcdefghijklmnopqrstuvwxyz234567890abcdefghijk"));
}

TEST_F(NsiteSecurityUtilsTest, IsValidNpub_InvalidCases) {
  // Wrong prefix
  EXPECT_FALSE(NsiteSecurityUtils::IsValidNpub(
      "nsec1234567890abcdefghijklmnopqrstuvwxyz234567890abcdefghijk"));
  
  // Wrong length
  EXPECT_FALSE(NsiteSecurityUtils::IsValidNpub("npub1234"));
  EXPECT_FALSE(NsiteSecurityUtils::IsValidNpub(
      "npub1234567890abcdefghijklmnopqrstuvwxyz234567890abcdefghijkl"));
  
  // Mixed case (bech32 should be lowercase)
  EXPECT_FALSE(NsiteSecurityUtils::IsValidNpub(
      "npub1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ234567890abcdefghijk"));
  
  // Invalid characters
  EXPECT_FALSE(NsiteSecurityUtils::IsValidNpub(
      "npub1234567890abcdefghijklmnopqrstuvwxyz234567890abcdefgh!@"));
}

TEST_F(NsiteSecurityUtilsTest, IsValidSessionId_ValidCases) {
  EXPECT_TRUE(NsiteSecurityUtils::IsValidSessionId("abcd1234-5678-90ef-ghij-klmnopqrstuv"));
  EXPECT_TRUE(NsiteSecurityUtils::IsValidSessionId("1234567890abcdefghij"));
  EXPECT_TRUE(NsiteSecurityUtils::IsValidSessionId("ABC123-def456"));
}

TEST_F(NsiteSecurityUtilsTest, IsValidSessionId_InvalidCases) {
  // Too short
  EXPECT_FALSE(NsiteSecurityUtils::IsValidSessionId("abc123"));
  
  // Too long
  EXPECT_FALSE(NsiteSecurityUtils::IsValidSessionId(
      "this-session-id-is-way-too-long-and-should-be-rejected-by-validation"));
  
  // Invalid characters
  EXPECT_FALSE(NsiteSecurityUtils::IsValidSessionId("abc123!@#"));
  EXPECT_FALSE(NsiteSecurityUtils::IsValidSessionId("abc 123"));
}

TEST_F(NsiteSecurityUtilsTest, SanitizeInput_BasicCases) {
  EXPECT_EQ("hello world", NsiteSecurityUtils::SanitizeInput("hello world", 100));
  EXPECT_EQ("hello", NsiteSecurityUtils::SanitizeInput("hello world", 5));
  EXPECT_EQ("", NsiteSecurityUtils::SanitizeInput("", 100));
}

TEST_F(NsiteSecurityUtilsTest, SanitizeInput_ControlCharacters) {
  std::string input = "hello\x01\x02world\x03";
  EXPECT_EQ("helloworld", NsiteSecurityUtils::SanitizeInput(input, 100));
  
  // Keep tabs, newlines, carriage returns
  input = "hello\tworld\ntest\r";
  EXPECT_EQ("hello\tworld\ntest\r", NsiteSecurityUtils::SanitizeInput(input, 100));
}

TEST_F(NsiteSecurityUtilsTest, GetSafeErrorMessage) {
  EXPECT_EQ("Bad Request", NsiteSecurityUtils::GetSafeErrorMessage(400));
  EXPECT_EQ("Unauthorized", NsiteSecurityUtils::GetSafeErrorMessage(401));
  EXPECT_EQ("Forbidden", NsiteSecurityUtils::GetSafeErrorMessage(403));
  EXPECT_EQ("Not Found", NsiteSecurityUtils::GetSafeErrorMessage(404));
  EXPECT_EQ("Too Many Requests", NsiteSecurityUtils::GetSafeErrorMessage(429));
  EXPECT_EQ("Internal Server Error", NsiteSecurityUtils::GetSafeErrorMessage(500));
  EXPECT_EQ("Service Unavailable", NsiteSecurityUtils::GetSafeErrorMessage(503));
  EXPECT_EQ("Unknown Error", NsiteSecurityUtils::GetSafeErrorMessage(999));
}

TEST_F(NsiteSecurityUtilsTest, SecureStringEquals) {
  EXPECT_TRUE(NsiteSecurityUtils::SecureStringEquals("hello", "hello"));
  EXPECT_FALSE(NsiteSecurityUtils::SecureStringEquals("hello", "world"));
  EXPECT_FALSE(NsiteSecurityUtils::SecureStringEquals("hello", "hello2"));
  EXPECT_TRUE(NsiteSecurityUtils::SecureStringEquals("", ""));
}

class RateLimiterTest : public testing::Test {
 public:
  RateLimiterTest() : rate_limiter_(5) {}  // 5 requests per minute
  
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NsiteSecurityUtils::RateLimiter rate_limiter_;
};

TEST_F(RateLimiterTest, AllowsRequestsUnderLimit) {
  std::string client_id = "127.0.0.1:12345";
  
  // Should allow 5 requests
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(rate_limiter_.IsAllowed(client_id));
  }
  
  // 6th request should be blocked
  EXPECT_FALSE(rate_limiter_.IsAllowed(client_id));
}

TEST_F(RateLimiterTest, ResetsAfterMinute) {
  std::string client_id = "127.0.0.1:12345";
  
  // Use up the limit
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(rate_limiter_.IsAllowed(client_id));
  }
  EXPECT_FALSE(rate_limiter_.IsAllowed(client_id));
  
  // Advance time by 1 minute
  task_environment_.FastForwardBy(base::Minutes(1));
  
  // Should allow requests again
  EXPECT_TRUE(rate_limiter_.IsAllowed(client_id));
}

TEST_F(RateLimiterTest, DifferentClientsIndependent) {
  std::string client1 = "127.0.0.1:12345";
  std::string client2 = "127.0.0.1:54321";
  
  // Use up limit for client1
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(rate_limiter_.IsAllowed(client1));
  }
  EXPECT_FALSE(rate_limiter_.IsAllowed(client1));
  
  // client2 should still be allowed
  EXPECT_TRUE(rate_limiter_.IsAllowed(client2));
}

TEST_F(RateLimiterTest, CleanupRemovesOldEntries) {
  std::string client_id = "127.0.0.1:12345";
  
  // Make a request
  EXPECT_TRUE(rate_limiter_.IsAllowed(client_id));
  
  // Advance time beyond cleanup threshold
  task_environment_.FastForwardBy(base::Minutes(6));
  
  // Cleanup should remove the old entry
  rate_limiter_.Cleanup();
  
  // Client should be treated as new (full limit available)
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(rate_limiter_.IsAllowed(client_id));
  }
}

}  // namespace nostr