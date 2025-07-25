// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_input_validator.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class NostrInputValidatorTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(NostrInputValidatorTest, ValidateHexKey) {
  // Valid 64-character hex key
  std::string valid_key = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
  EXPECT_TRUE(NostrInputValidator::IsValidHexKey(valid_key));
  
  // Invalid lengths
  EXPECT_FALSE(NostrInputValidator::IsValidHexKey(""));
  EXPECT_FALSE(NostrInputValidator::IsValidHexKey("1234"));
  EXPECT_FALSE(NostrInputValidator::IsValidHexKey("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef00"));
  
  // Invalid characters
  EXPECT_FALSE(NostrInputValidator::IsValidHexKey("gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"));
  EXPECT_FALSE(NostrInputValidator::IsValidHexKey("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdefg"));
}

TEST_F(NostrInputValidatorTest, ValidateNpub) {
  // Valid npub (valid Bech32 characters only)
  std::string valid_npub = "npub1234567890acdefghjklmnpqrstuvwxyz234567890acdefghjklmnpqrstuv";
  EXPECT_TRUE(NostrInputValidator::IsValidNpub(valid_npub));
  
  // Invalid prefix
  EXPECT_FALSE(NostrInputValidator::IsValidNpub("nsec1234567890acdefghjklmnpqrstuvwxyz234567890acdefghjklmnpqrstuv"));
  EXPECT_FALSE(NostrInputValidator::IsValidNpub("1234567890acdefghjklmnpqrstuvwxyz234567890acdefghjklmnpqrstuv"));
  
  // Invalid Bech32 characters (contains 'b', 'i', 'o')
  EXPECT_FALSE(NostrInputValidator::IsValidNpub("npub1234567890abcdefghijklmnopqrstuvwxyz234567890abcdefghijklmnop"));
  
  // Empty string
  EXPECT_FALSE(NostrInputValidator::IsValidNpub(""));
  
  // Too long
  std::string too_long = "npub1234567890acdefghjklmnpqrstuvwxyz234567890acdefghjklmnpqrstuvwxyz";
  EXPECT_FALSE(NostrInputValidator::IsValidNpub(too_long));
}

TEST_F(NostrInputValidatorTest, ValidateEventKind) {
  // Valid kinds
  EXPECT_TRUE(NostrInputValidator::IsValidEventKind(0));
  EXPECT_TRUE(NostrInputValidator::IsValidEventKind(1));
  EXPECT_TRUE(NostrInputValidator::IsValidEventKind(3));
  EXPECT_TRUE(NostrInputValidator::IsValidEventKind(1000));
  EXPECT_TRUE(NostrInputValidator::IsValidEventKind(65535));
  
  // Invalid kinds
  EXPECT_FALSE(NostrInputValidator::IsValidEventKind(-1));
  EXPECT_FALSE(NostrInputValidator::IsValidEventKind(65536));
}

TEST_F(NostrInputValidatorTest, ValidateTimestamp) {
  // Valid timestamps (2020-2100 range)
  EXPECT_TRUE(NostrInputValidator::IsValidTimestamp(1577836800));  // Jan 1, 2020
  EXPECT_TRUE(NostrInputValidator::IsValidTimestamp(1640995200));  // Jan 1, 2022
  EXPECT_TRUE(NostrInputValidator::IsValidTimestamp(4102444800));  // Jan 1, 2100
  
  // Invalid timestamps
  EXPECT_FALSE(NostrInputValidator::IsValidTimestamp(0));
  EXPECT_FALSE(NostrInputValidator::IsValidTimestamp(1577836799));  // Before 2020
  EXPECT_FALSE(NostrInputValidator::IsValidTimestamp(4102444801));  // After 2100
}

TEST_F(NostrInputValidatorTest, SanitizeString) {
  // Normal string
  std::string normal = "Hello, World!";
  EXPECT_EQ(NostrInputValidator::SanitizeString(normal, 100), normal);
  
  // String with null bytes
  std::string with_null = "Hello\x00World";
  EXPECT_EQ(NostrInputValidator::SanitizeString(with_null, 100), "HelloWorld");
  
  // String with control characters
  std::string with_control = "Hello\x01\x02World";
  EXPECT_EQ(NostrInputValidator::SanitizeString(with_control, 100), "HelloWorld");
  
  // String with newlines and tabs (should be preserved)
  std::string with_whitespace = "Hello\n\tWorld";
  EXPECT_EQ(NostrInputValidator::SanitizeString(with_whitespace, 100), "Hello\n\tWorld");
  
  // String too long
  std::string long_string(200, 'a');
  EXPECT_EQ(NostrInputValidator::SanitizeString(long_string, 100).length(), 100u);
}

TEST_F(NostrInputValidatorTest, ValidateUnsignedEvent) {
  // Valid unsigned event
  base::Value::Dict valid_event;
  valid_event.Set("content", "Hello, Nostr!");
  valid_event.Set("kind", 1);
  valid_event.Set("created_at", 1640995200);
  
  base::Value::List tags;
  base::Value::List tag;
  tag.Append("t");
  tag.Append("nostr");
  tags.Append(std::move(tag));
  valid_event.Set("tags", std::move(tags));
  
  std::string error_message;
  EXPECT_TRUE(NostrInputValidator::ValidateUnsignedEvent(valid_event, &error_message));
  
  // Missing required fields
  base::Value::Dict missing_content;
  missing_content.Set("kind", 1);
  missing_content.Set("tags", base::Value::List());
  
  EXPECT_FALSE(NostrInputValidator::ValidateUnsignedEvent(missing_content, &error_message));
  EXPECT_EQ(error_message, "Event missing required fields");
  
  // Invalid kind
  base::Value::Dict invalid_kind;
  invalid_kind.Set("content", "Hello");
  invalid_kind.Set("kind", -1);
  invalid_kind.Set("tags", base::Value::List());
  
  EXPECT_FALSE(NostrInputValidator::ValidateUnsignedEvent(invalid_kind, &error_message));
  EXPECT_EQ(error_message, "Invalid event kind");
}

TEST_F(NostrInputValidatorTest, ValidateEventTags) {
  // Valid tags
  base::Value::List valid_tags;
  base::Value::List tag1;
  tag1.Append("t");
  tag1.Append("nostr");
  valid_tags.Append(std::move(tag1));
  
  base::Value::List tag2;
  tag2.Append("p");
  tag2.Append("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef");
  valid_tags.Append(std::move(tag2));
  
  std::string error_message;
  EXPECT_TRUE(NostrInputValidator::ValidateEventTags(valid_tags, &error_message));
  
  // Invalid tag - empty
  base::Value::List invalid_tags;
  invalid_tags.Append(base::Value::List());
  
  EXPECT_FALSE(NostrInputValidator::ValidateEventTags(invalid_tags, &error_message));
  EXPECT_EQ(error_message, "Invalid tag format");
  
  // Invalid tag - non-string element
  base::Value::List invalid_tags2;
  base::Value::List tag3;
  tag3.Append("t");
  tag3.Append(123);  // Number instead of string
  invalid_tags2.Append(std::move(tag3));
  
  EXPECT_FALSE(NostrInputValidator::ValidateEventTags(invalid_tags2, &error_message));
  EXPECT_EQ(error_message, "Tag contains non-string element");
}

TEST_F(NostrInputValidatorTest, ValidateJsonDepth) {
  // Valid depth
  base::Value::Dict simple_dict;
  simple_dict.Set("key", "value");
  EXPECT_TRUE(NostrInputValidator::ValidateJsonDepth(simple_dict));
  
  // Nested but valid depth
  base::Value::Dict nested_dict;
  base::Value::Dict inner_dict;
  inner_dict.Set("inner_key", "inner_value");
  nested_dict.Set("outer_key", std::move(inner_dict));
  EXPECT_TRUE(NostrInputValidator::ValidateJsonDepth(nested_dict));
  
  // Too deep nesting would require more complex setup
  // This test verifies the basic functionality works
}

TEST_F(NostrInputValidatorTest, SanitizeEventContentWithJSON) {
  // Valid JSON for metadata event (kind 0)
  std::string valid_json = "{\"name\":\"test\",\"about\":\"description\"}";
  auto result = NostrInputValidator::SanitizeEventContent(valid_json, 0);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), valid_json);
  
  // Invalid JSON for metadata event (kind 0)
  std::string invalid_json = "{\"name\":\"test\",\"about\":}";
  auto result2 = NostrInputValidator::SanitizeEventContent(invalid_json, 0);
  EXPECT_FALSE(result2.has_value());
  
  // Valid JSON for contact list event (kind 3)
  std::string valid_contacts = "[]";
  auto result3 = NostrInputValidator::SanitizeEventContent(valid_contacts, 3);
  EXPECT_TRUE(result3.has_value());
  EXPECT_EQ(result3.value(), valid_contacts);
  
  // Non-JSON content for other kinds (should pass)
  std::string text_note = "Hello, Nostr!";
  auto result4 = NostrInputValidator::SanitizeEventContent(text_note, 1);
  EXPECT_TRUE(result4.has_value());
  EXPECT_EQ(result4.value(), text_note);
}

TEST_F(NostrInputValidatorTest, ValidateRelayUrl) {
  // Valid relay URLs
  EXPECT_TRUE(NostrInputValidator::IsValidRelayUrl("wss://relay.example.com"));
  EXPECT_TRUE(NostrInputValidator::IsValidRelayUrl("ws://relay.example.com"));
  EXPECT_TRUE(NostrInputValidator::IsValidRelayUrl("wss://relay.example.com:443"));
  
  // Valid localhost for local relay
  EXPECT_TRUE(NostrInputValidator::IsValidRelayUrl("ws://127.0.0.1:8081"));
  EXPECT_TRUE(NostrInputValidator::IsValidRelayUrl("ws://localhost:8081"));
  
  // Invalid localhost ports
  EXPECT_FALSE(NostrInputValidator::IsValidRelayUrl("ws://127.0.0.1:8080"));
  EXPECT_FALSE(NostrInputValidator::IsValidRelayUrl("ws://localhost:3000"));
  
  // Invalid schemes
  EXPECT_FALSE(NostrInputValidator::IsValidRelayUrl("http://relay.example.com"));
  EXPECT_FALSE(NostrInputValidator::IsValidRelayUrl("https://relay.example.com"));
  EXPECT_FALSE(NostrInputValidator::IsValidRelayUrl("ftp://relay.example.com"));
  
  // Invalid URLs
  EXPECT_FALSE(NostrInputValidator::IsValidRelayUrl(""));
  EXPECT_FALSE(NostrInputValidator::IsValidRelayUrl("not-a-url"));
  EXPECT_FALSE(NostrInputValidator::IsValidRelayUrl("wss://"));
  
  // Private IP ranges (should be rejected)
  EXPECT_FALSE(NostrInputValidator::IsValidRelayUrl("wss://192.168.1.1"));
  EXPECT_FALSE(NostrInputValidator::IsValidRelayUrl("wss://10.0.0.1"));
  EXPECT_FALSE(NostrInputValidator::IsValidRelayUrl("wss://172.16.0.1"));
}

}  // namespace nostr