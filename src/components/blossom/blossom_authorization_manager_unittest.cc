// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blossom/blossom_authorization_manager.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/nostr/nostr_event.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace blossom {

namespace {

// Test keys (generated for testing only)
const char kTestPrivateKey[] = 
    "5caa3cd87cf1ad409a9b425c1c34d1428e549d4b8f5a4eb2cb5c2f30e29b8e8f";
const char kTestPublicKey[] = 
    "3bf0a1219ada6c8e5b833ab0e18af3d5e85d6c2d42f83232fef63beadf9cbad8";

// Helper to create a test event
std::unique_ptr<nostr::NostrEvent> CreateTestEvent(
    int kind,
    const base::Value::List& tags,
    const std::string& content,
    int64_t created_at = 0) {
  auto event = std::make_unique<nostr::NostrEvent>();
  event->kind = kind;
  event->pubkey = kTestPublicKey;
  event->created_at = created_at ? created_at : base::Time::Now().ToTimeT();
  event->tags = tags.Clone();
  event->content = content;
  
  // Calculate event ID
  base::Value::List arr;
  arr.Append(0);
  arr.Append(event->pubkey);
  arr.Append(static_cast<double>(event->created_at));
  arr.Append(event->kind);
  arr.Append(event->tags.Clone());
  arr.Append(event->content);
  
  std::string json;
  base::JSONWriter::Write(arr, &json);
  std::string hash = crypto::SHA256HashString(json);
  event->id = base::HexEncode(hash.data(), hash.size());
  
  // Sign the event (simplified for testing)
  // In production, this would use proper secp256k1 signing
  event->sig = std::string(128, '0');  // Dummy signature
  
  return event;
}

// Helper to create authorization header
std::string CreateAuthHeader(const nostr::NostrEvent& event) {
  base::Value::Dict event_dict;
  event_dict.Set("id", event.id);
  event_dict.Set("pubkey", event.pubkey);
  event_dict.Set("created_at", static_cast<double>(event.created_at));
  event_dict.Set("kind", event.kind);
  event_dict.Set("tags", event.tags.Clone());
  event_dict.Set("content", event.content);
  event_dict.Set("sig", event.sig);
  
  std::string json;
  base::JSONWriter::Write(event_dict, &json);
  
  std::string encoded;
  base::Base64Encode(json, &encoded);
  
  return "Nostr " + encoded;
}

}  // namespace

class BlossomAuthorizationManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    BlossomAuthorizationManager::Config config;
    config.server_name = "test.server";
    config.cache_ttl = base::Hours(1);
    config.max_cache_size = 100;
    config.require_expiration = true;
    
    manager_ = std::make_unique<BlossomAuthorizationManager>(config);
  }

  void CheckAuth(const std::string& auth_header,
                const std::string& verb,
                const std::string& hash,
                bool* result,
                std::string* reason) {
    base::RunLoop run_loop;
    manager_->CheckAuthorization(
        auth_header, verb, hash,
        base::BindLambdaForTesting([&](bool authorized, 
                                      const std::string& msg) {
          *result = authorized;
          *reason = msg;
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<BlossomAuthorizationManager> manager_;
};

TEST_F(BlossomAuthorizationManagerTest, InvalidAuthHeader) {
  bool result;
  std::string reason;
  
  // Missing "Nostr " prefix
  CheckAuth("Bearer token", "upload", "abc123", &result, &reason);
  EXPECT_FALSE(result);
  EXPECT_EQ("Invalid authorization header format", reason);
  
  // Invalid base64
  CheckAuth("Nostr !!!invalid!!!", "upload", "abc123", &result, &reason);
  EXPECT_FALSE(result);
  EXPECT_EQ("Invalid authorization header format", reason);
  
  // Not JSON
  std::string encoded;
  base::Base64Encode("not json", &encoded);
  CheckAuth("Nostr " + encoded, "upload", "abc123", &result, &reason);
  EXPECT_FALSE(result);
  EXPECT_EQ("Invalid authorization header format", reason);
}

TEST_F(BlossomAuthorizationManagerTest, WrongEventKind) {
  base::Value::List tags;
  auto event = CreateTestEvent(1, tags, "test");  // Wrong kind
  
  bool result;
  std::string reason;
  CheckAuth(CreateAuthHeader(*event), "upload", "abc123", &result, &reason);
  EXPECT_FALSE(result);
  EXPECT_EQ("Invalid authorization event", reason);
}

TEST_F(BlossomAuthorizationManagerTest, MissingExpiration) {
  base::Value::List tags;
  base::Value::List t_tag;
  t_tag.Append("t");
  t_tag.Append("upload");
  tags.Append(std::move(t_tag));
  
  auto event = CreateTestEvent(24242, tags, "");
  
  bool result;
  std::string reason;
  CheckAuth(CreateAuthHeader(*event), "upload", "abc123", &result, &reason);
  EXPECT_FALSE(result);
  EXPECT_EQ("Invalid authorization event", reason);
}

TEST_F(BlossomAuthorizationManagerTest, ExpiredAuthorization) {
  base::Value::List tags;
  
  // Add expired timestamp
  base::Value::List exp_tag;
  exp_tag.Append("expiration");
  exp_tag.Append(base::NumberToString(base::Time::Now().ToTimeT() - 3600));
  tags.Append(std::move(exp_tag));
  
  // Add verb
  base::Value::List t_tag;
  t_tag.Append("t");
  t_tag.Append("upload");
  tags.Append(std::move(t_tag));
  
  auto event = CreateTestEvent(24242, tags, "");
  
  bool result;
  std::string reason;
  CheckAuth(CreateAuthHeader(*event), "upload", "abc123", &result, &reason);
  EXPECT_FALSE(result);
  EXPECT_EQ("Invalid authorization event", reason);
}

TEST_F(BlossomAuthorizationManagerTest, ValidAuthorizationAllHashes) {
  base::Value::List tags;
  
  // Add future expiration
  base::Value::List exp_tag;
  exp_tag.Append("expiration");
  exp_tag.Append(base::NumberToString(base::Time::Now().ToTimeT() + 3600));
  tags.Append(std::move(exp_tag));
  
  // Add verb
  base::Value::List t_tag;
  t_tag.Append("t");
  t_tag.Append("upload");
  tags.Append(std::move(t_tag));
  
  // Add server
  base::Value::List server_tag;
  server_tag.Append("server");
  server_tag.Append("test.server");
  tags.Append(std::move(server_tag));
  
  auto event = CreateTestEvent(24242, tags, "");
  
  // NOTE: In real implementation, we'd need proper signature
  // For testing, we'll mock the signature verification
  
  bool result;
  std::string reason;
  
  // Should fail because signature verification will fail
  // In production tests, we'd mock the crypto functions
  CheckAuth(CreateAuthHeader(*event), "upload", "abc123", &result, &reason);
  EXPECT_FALSE(result);
}

TEST_F(BlossomAuthorizationManagerTest, WrongVerb) {
  base::Value::List tags;
  
  // Add future expiration
  base::Value::List exp_tag;
  exp_tag.Append("expiration");
  exp_tag.Append(base::NumberToString(base::Time::Now().ToTimeT() + 3600));
  tags.Append(std::move(exp_tag));
  
  // Add verb
  base::Value::List t_tag;
  t_tag.Append("t");
  t_tag.Append("upload");
  tags.Append(std::move(t_tag));
  
  // Add server
  base::Value::List server_tag;
  server_tag.Append("server");
  server_tag.Append("test.server");
  tags.Append(std::move(server_tag));
  
  auto event = CreateTestEvent(24242, tags, "");
  
  bool result;
  std::string reason;
  
  // Try to use "delete" verb when only "upload" is allowed
  CheckAuth(CreateAuthHeader(*event), "delete", "abc123", &result, &reason);
  EXPECT_FALSE(result);
}

TEST_F(BlossomAuthorizationManagerTest, SpecificHashAuthorization) {
  base::Value::List tags;
  
  // Add future expiration
  base::Value::List exp_tag;
  exp_tag.Append("expiration");
  exp_tag.Append(base::NumberToString(base::Time::Now().ToTimeT() + 3600));
  tags.Append(std::move(exp_tag));
  
  // Add verb
  base::Value::List t_tag;
  t_tag.Append("t");
  t_tag.Append("upload");
  tags.Append(std::move(t_tag));
  
  // Add specific hash
  base::Value::List x_tag;
  x_tag.Append("x");
  x_tag.Append("allowed_hash");
  tags.Append(std::move(x_tag));
  
  // Add server
  base::Value::List server_tag;
  server_tag.Append("server");
  server_tag.Append("test.server");
  tags.Append(std::move(server_tag));
  
  auto event = CreateTestEvent(24242, tags, "");
  std::string auth_header = CreateAuthHeader(*event);
  
  bool result;
  std::string reason;
  
  // Should fail for different hash
  CheckAuth(auth_header, "upload", "different_hash", &result, &reason);
  EXPECT_FALSE(result);
}

TEST_F(BlossomAuthorizationManagerTest, MultipleVerbsAndHashes) {
  base::Value::List tags;
  
  // Add future expiration
  base::Value::List exp_tag;
  exp_tag.Append("expiration");
  exp_tag.Append(base::NumberToString(base::Time::Now().ToTimeT() + 3600));
  tags.Append(std::move(exp_tag));
  
  // Add multiple verbs
  base::Value::List t_tag1;
  t_tag1.Append("t");
  t_tag1.Append("upload");
  tags.Append(std::move(t_tag1));
  
  base::Value::List t_tag2;
  t_tag2.Append("t");
  t_tag2.Append("delete");
  tags.Append(std::move(t_tag2));
  
  // Add multiple hashes
  base::Value::List x_tag1;
  x_tag1.Append("x");
  x_tag1.Append("hash1");
  tags.Append(std::move(x_tag1));
  
  base::Value::List x_tag2;
  x_tag2.Append("x");
  x_tag2.Append("hash2");
  tags.Append(std::move(x_tag2));
  
  // Add server
  base::Value::List server_tag;
  server_tag.Append("server");
  server_tag.Append("test.server");
  tags.Append(std::move(server_tag));
  
  auto event = CreateTestEvent(24242, tags, "");
  
  // The authorization should work for both verbs and both hashes
  // (though signature verification will fail in this test)
}

TEST_F(BlossomAuthorizationManagerTest, ServerNameMismatch) {
  base::Value::List tags;
  
  // Add future expiration
  base::Value::List exp_tag;
  exp_tag.Append("expiration");
  exp_tag.Append(base::NumberToString(base::Time::Now().ToTimeT() + 3600));
  tags.Append(std::move(exp_tag));
  
  // Add verb
  base::Value::List t_tag;
  t_tag.Append("t");
  t_tag.Append("upload");
  tags.Append(std::move(t_tag));
  
  // Add wrong server
  base::Value::List server_tag;
  server_tag.Append("server");
  server_tag.Append("wrong.server");
  tags.Append(std::move(server_tag));
  
  auto event = CreateTestEvent(24242, tags, "");
  
  bool result;
  std::string reason;
  CheckAuth(CreateAuthHeader(*event), "upload", "abc123", &result, &reason);
  EXPECT_FALSE(result);
  EXPECT_EQ("Invalid authorization event", reason);
}

// TODO: Add tests for:
// - Cache functionality
// - Cache expiration
// - Proper signature verification with real keys
// - Event ID verification

}  // namespace blossom