// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/nostr_messages.h"

#include "base/values.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class NostrMessagesTest : public testing::Test {
 protected:
  NostrMessagesTest() = default;
  ~NostrMessagesTest() override = default;
};

// Test NostrPermissionRequest serialization
TEST_F(NostrMessagesTest, NostrPermissionRequestSerialization) {
  NostrPermissionRequest original;
  original.origin = url::Origin::Create(GURL("https://example.com"));
  original.method = "signEvent";
  original.details.Set("kinds", base::Value::List().Append(1).Append(4));
  original.timestamp = base::TimeTicks::Now();
  original.remember_decision = true;

  // Serialize and deserialize
  IPC::Message msg(0, 0, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, original);
  
  base::PickleIterator iter(msg);
  NostrPermissionRequest deserialized;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &deserialized));
  
  // Verify
  EXPECT_EQ(original.origin, deserialized.origin);
  EXPECT_EQ(original.method, deserialized.method);
  EXPECT_EQ(original.details, deserialized.details);
  EXPECT_EQ(original.remember_decision, deserialized.remember_decision);
}

// Test NostrEvent serialization
TEST_F(NostrMessagesTest, NostrEventSerialization) {
  NostrEvent original;
  original.id = "test_event_id";
  original.pubkey = "test_pubkey";
  original.created_at = 1234567890;
  original.kind = 1;
  original.tags = {{"p", "pubkey1"}, {"e", "event1"}};
  original.content = "Test content";
  original.sig = "test_signature";

  // Serialize and deserialize
  IPC::Message msg(0, 0, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, original);
  
  base::PickleIterator iter(msg);
  NostrEvent deserialized;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &deserialized));
  
  // Verify
  EXPECT_EQ(original.id, deserialized.id);
  EXPECT_EQ(original.pubkey, deserialized.pubkey);
  EXPECT_EQ(original.created_at, deserialized.created_at);
  EXPECT_EQ(original.kind, deserialized.kind);
  EXPECT_EQ(original.tags, deserialized.tags);
  EXPECT_EQ(original.content, deserialized.content);
  EXPECT_EQ(original.sig, deserialized.sig);
}

// Test NostrRelayPolicy serialization
TEST_F(NostrMessagesTest, NostrRelayPolicySerialization) {
  NostrRelayPolicy original;
  base::Value::Dict relay1;
  relay1.Set("read", true);
  relay1.Set("write", false);
  original.relays["wss://relay.damus.io"] = std::move(relay1);
  
  base::Value::Dict relay2;
  relay2.Set("read", true);
  relay2.Set("write", true);
  original.relays["wss://nos.lol"] = std::move(relay2);

  // Serialize and deserialize
  IPC::Message msg(0, 0, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, original);
  
  base::PickleIterator iter(msg);
  NostrRelayPolicy deserialized;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &deserialized));
  
  // Verify
  EXPECT_EQ(original.relays.size(), deserialized.relays.size());
  for (const auto& [url, policy] : original.relays) {
    auto it = deserialized.relays.find(url);
    ASSERT_NE(it, deserialized.relays.end());
    EXPECT_EQ(policy, it->second);
  }
}

// Test BlossomUploadResult serialization
TEST_F(NostrMessagesTest, BlossomUploadResultSerialization) {
  BlossomUploadResult original;
  original.hash = "sha256_hash_value";
  original.url = "https://blossom.example.com/sha256_hash_value";
  original.size = 1024;
  original.mime_type = "image/png";
  original.servers = {"server1.com", "server2.com"};

  // Serialize and deserialize
  IPC::Message msg(0, 0, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, original);
  
  base::PickleIterator iter(msg);
  BlossomUploadResult deserialized;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &deserialized));
  
  // Verify
  EXPECT_EQ(original.hash, deserialized.hash);
  EXPECT_EQ(original.url, deserialized.url);
  EXPECT_EQ(original.size, deserialized.size);
  EXPECT_EQ(original.mime_type, deserialized.mime_type);
  EXPECT_EQ(original.servers, deserialized.servers);
}

// Test NostrRateLimitInfo serialization
TEST_F(NostrMessagesTest, NostrRateLimitInfoSerialization) {
  NostrRateLimitInfo original;
  original.requests_per_minute = 60;
  original.signs_per_hour = 100;
  original.window_start = base::TimeTicks::Now();
  original.current_count = 25;

  // Serialize and deserialize
  IPC::Message msg(0, 0, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, original);
  
  base::PickleIterator iter(msg);
  NostrRateLimitInfo deserialized;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &deserialized));
  
  // Verify
  EXPECT_EQ(original.requests_per_minute, deserialized.requests_per_minute);
  EXPECT_EQ(original.signs_per_hour, deserialized.signs_per_hour);
  EXPECT_EQ(original.window_start, deserialized.window_start);
  EXPECT_EQ(original.current_count, deserialized.current_count);
}

// Test message creation and basic structure
TEST_F(NostrMessagesTest, MessageCreation) {
  // Test GetPublicKey message
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  IPC::Message* msg = new NostrHostMsg_GetPublicKey(1, 42, origin);
  EXPECT_EQ(static_cast<uint32_t>(NostrHostMsg_GetPublicKey::ID), msg->type());
  delete msg;

  // Test SignEvent message
  base::Value::Dict event;
  event.Set("kind", 1);
  event.Set("content", "Hello Nostr");
  NostrRateLimitInfo rate_limit;
  rate_limit.requests_per_minute = 60;
  
  msg = new NostrHostMsg_SignEvent(1, 43, origin, event, rate_limit);
  EXPECT_EQ(static_cast<uint32_t>(NostrHostMsg_SignEvent::ID), msg->type());
  delete msg;

  // Test response message
  msg = new NostrMsg_GetPublicKeyResponse(1, 42, true, "pubkey123");
  EXPECT_EQ(static_cast<uint32_t>(NostrMsg_GetPublicKeyResponse::ID), 
            msg->type());
  delete msg;
}

// Test complex message with multiple parameters
TEST_F(NostrMessagesTest, ComplexMessageSerialization) {
  // Create a Blossom upload message
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  std::string mime_type = "application/octet-stream";
  base::Value::Dict metadata;
  metadata.Set("name", "test.bin");
  metadata.Set("description", "Test file");
  
  IPC::Message* msg = new NostrHostMsg_BlossomUpload(
      1, 44, origin, data, mime_type, metadata);
  
  // Verify we can extract the parameters back
  base::PickleIterator iter(*msg);
  int routing_id, request_id;
  url::Origin extracted_origin;
  std::vector<uint8_t> extracted_data;
  std::string extracted_mime;
  base::Value::Dict extracted_metadata;
  
  EXPECT_TRUE(iter.ReadInt(&routing_id));
  EXPECT_TRUE(IPC::ReadParam(msg, &iter, &request_id));
  EXPECT_TRUE(IPC::ReadParam(msg, &iter, &extracted_origin));
  EXPECT_TRUE(IPC::ReadParam(msg, &iter, &extracted_data));
  EXPECT_TRUE(IPC::ReadParam(msg, &iter, &extracted_mime));
  EXPECT_TRUE(IPC::ReadParam(msg, &iter, &extracted_metadata));
  
  EXPECT_EQ(44, request_id);
  EXPECT_EQ(origin, extracted_origin);
  EXPECT_EQ(data, extracted_data);
  EXPECT_EQ(mime_type, extracted_mime);
  EXPECT_EQ(metadata, extracted_metadata);
  
  delete msg;
}

// Test error handling in message parsing
TEST_F(NostrMessagesTest, MalformedMessageHandling) {
  // Create a message with wrong parameters
  IPC::Message msg(1, NostrHostMsg_GetPublicKey::ID, 
                   IPC::Message::PRIORITY_NORMAL);
  
  // Write incorrect data
  IPC::WriteParam(&msg, 42);  // Just an int, not the expected parameters
  
  // Try to read as GetPublicKey message
  base::PickleIterator iter(msg);
  int routing_id, request_id;
  url::Origin origin;
  
  EXPECT_TRUE(iter.ReadInt(&routing_id));
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &request_id));
  EXPECT_FALSE(IPC::ReadParam(&msg, &iter, &origin));  // Should fail
}

}  // namespace