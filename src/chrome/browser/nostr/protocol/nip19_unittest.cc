// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/protocol/nip19.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {
namespace nip19 {

class Nip19Test : public testing::Test {
 protected:
  void SetUp() override {}
};

// Test vectors from NIP-19
TEST_F(Nip19Test, DecodeNpub) {
  std::string npub = "npub180cvv07tjdrrgpa0j7j7tmnyl2yr6yr7l8j4s3evf6u64th6gkwsyjh6w6";
  auto result = Decode(npub);
  
  ASSERT_TRUE(result.has_value());
  auto* entity = std::get_if<Entity>(&result.value());
  ASSERT_NE(entity, nullptr);
  
  EXPECT_EQ(entity->type, EntityType::kNpub);
  EXPECT_EQ(entity->hex_id, "3bf0c63fcb93463407af97a5e5ee64fa883d107ef9e558472c4eb9aaaefa459d");
}

TEST_F(Nip19Test, DecodeNote) {
  std::string note = "note1370p7v5cmk3d8q9xua5myuqxyhxlh9uh0qexr9sd6n3w3az234sdetuyy";
  auto result = Decode(note);
  
  ASSERT_TRUE(result.has_value());
  auto* entity = std::get_if<Entity>(&result.value());
  ASSERT_NE(entity, nullptr);
  
  EXPECT_EQ(entity->type, EntityType::kNote);
  EXPECT_EQ(entity->hex_id.size(), 64);  // 32 bytes as hex
}

TEST_F(Nip19Test, DecodeNprofile) {
  // Example nprofile with relay hints
  std::string nprofile = "nprofile1qqsrhuxx8l9ex335q7he0f09aej04zpazpl0ne2cgukyawd24mayt8gpp4mhxue69uhhytnc9e3k7mgpz4mhxue69uhkg6nzv9ejuumpv34kytnrdaksjlyr9p";
  auto result = Decode(nprofile);
  
  ASSERT_TRUE(result.has_value());
  auto* entity = std::get_if<ExtendedEntity>(&result.value());
  ASSERT_NE(entity, nullptr);
  
  EXPECT_EQ(entity->type, EntityType::kNprofile);
  EXPECT_EQ(entity->hex_id.size(), 64);
  EXPECT_GT(entity->relays.size(), 0);
}

TEST_F(Nip19Test, DecodeNevent) {
  // Example nevent with event ID, relay hints, and author
  std::string nevent = "nevent1qqstna2yrezu5wghjvswqqculvvwxsrcvu7uc0f78gan4xqhvz49d9spr3mhxue69uhkummnw3ez6ur4vgh8wetvd3hhyer9wghxuet59uq32amnwvaz7tmjv4kxz7fwv3sk6atn9e5k7tcpz4mhxue69uhkg6nzv9ejuumpv34kytnrdaksjlyr9p";
  auto result = Decode(nevent);
  
  ASSERT_TRUE(result.has_value());
  auto* entity = std::get_if<ExtendedEntity>(&result.value());
  ASSERT_NE(entity, nullptr);
  
  EXPECT_EQ(entity->type, EntityType::kNevent);
  EXPECT_EQ(entity->hex_id.size(), 64);
  EXPECT_GT(entity->relays.size(), 0);
  EXPECT_TRUE(entity->author.has_value());
}

TEST_F(Nip19Test, DecodeNaddr) {
  // Example naddr for parameterized replaceable event
  std::string naddr = "naddr1qqxnzdesxqmnxvpexqunzvpcqyt8wumn8ghj7un9d3shjtnwdaehgu3wvfskueqzypve7elhmamff3sr5mgxxms4a0rppkmhmn7504h96pfcdkpplvl2jqcyqqq823cnmhuld";
  auto result = Decode(naddr);
  
  ASSERT_TRUE(result.has_value());
  auto* entity = std::get_if<ExtendedEntity>(&result.value());
  ASSERT_NE(entity, nullptr);
  
  EXPECT_EQ(entity->type, EntityType::kNaddr);
  EXPECT_EQ(entity->hex_id.size(), 64);
  EXPECT_TRUE(entity->kind.has_value());
  EXPECT_TRUE(entity->identifier.has_value());
}

TEST_F(Nip19Test, DecodeInvalidBech32) {
  std::string invalid = "npub1invalid";
  auto result = Decode(invalid);
  
  EXPECT_FALSE(result.has_value());
}

TEST_F(Nip19Test, DecodeUnknownHRP) {
  std::string unknown = "unkn180cvv07tjdrrgpa0j7j7tmnyl2yr6yr7l8j4s3evf6u64th6gkwsqgm5f7";
  auto result = Decode(unknown);
  
  EXPECT_FALSE(result.has_value());
}

TEST_F(Nip19Test, EncodeDecodeNpub) {
  std::string hex_pubkey = "3bf0c63fcb93463407af97a5e5ee64fa883d107ef9e558472c4eb9aaaefa459d";
  std::string encoded = EncodeNpub(hex_pubkey);
  
  auto decoded = Decode(encoded);
  ASSERT_TRUE(decoded.has_value());
  auto* entity = std::get_if<Entity>(&decoded.value());
  ASSERT_NE(entity, nullptr);
  
  EXPECT_EQ(entity->type, EntityType::kNpub);
  EXPECT_EQ(entity->hex_id, hex_pubkey);
}

TEST_F(Nip19Test, EncodeDecodeNote) {
  std::string hex_event_id = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  std::string encoded = EncodeNote(hex_event_id);
  
  auto decoded = Decode(encoded);
  ASSERT_TRUE(decoded.has_value());
  auto* entity = std::get_if<Entity>(&decoded.value());
  ASSERT_NE(entity, nullptr);
  
  EXPECT_EQ(entity->type, EntityType::kNote);
  EXPECT_EQ(entity->hex_id, hex_event_id);
}

TEST_F(Nip19Test, EncodeDecodeNprofile) {
  std::string hex_pubkey = "3bf0c63fcb93463407af97a5e5ee64fa883d107ef9e558472c4eb9aaaefa459d";
  std::vector<std::string> relays = {
    "wss://relay.nostr.band",
    "wss://nos.lol"
  };
  
  std::string encoded = EncodeNprofile(hex_pubkey, relays);
  
  auto decoded = Decode(encoded);
  ASSERT_TRUE(decoded.has_value());
  auto* entity = std::get_if<ExtendedEntity>(&decoded.value());
  ASSERT_NE(entity, nullptr);
  
  EXPECT_EQ(entity->type, EntityType::kNprofile);
  EXPECT_EQ(entity->hex_id, hex_pubkey);
  EXPECT_EQ(entity->relays, relays);
}

TEST_F(Nip19Test, EncodeDecodeNevent) {
  std::string hex_event_id = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  std::string hex_author = "3bf0c63fcb93463407af97a5e5ee64fa883d107ef9e558472c4eb9aaaefa459d";
  std::vector<std::string> relays = {
    "wss://relay.nostr.band"
  };
  
  std::string encoded = EncodeNevent(hex_event_id, relays, hex_author);
  
  auto decoded = Decode(encoded);
  ASSERT_TRUE(decoded.has_value());
  auto* entity = std::get_if<ExtendedEntity>(&decoded.value());
  ASSERT_NE(entity, nullptr);
  
  EXPECT_EQ(entity->type, EntityType::kNevent);
  EXPECT_EQ(entity->hex_id, hex_event_id);
  EXPECT_EQ(entity->relays, relays);
  EXPECT_TRUE(entity->author.has_value());
  EXPECT_EQ(entity->author.value(), hex_author);
}

TEST_F(Nip19Test, EncodeDecodeNaddr) {
  std::string hex_pubkey = "3bf0c63fcb93463407af97a5e5ee64fa883d107ef9e558472c4eb9aaaefa459d";
  std::string identifier = "my-article";
  int kind = 30023;
  std::vector<std::string> relays = {
    "wss://relay.nostr.band"
  };
  
  std::string encoded = EncodeNaddr(kind, hex_pubkey, identifier, relays);
  
  auto decoded = Decode(encoded);
  ASSERT_TRUE(decoded.has_value());
  auto* entity = std::get_if<ExtendedEntity>(&decoded.value());
  ASSERT_NE(entity, nullptr);
  
  EXPECT_EQ(entity->type, EntityType::kNaddr);
  EXPECT_EQ(entity->hex_id, hex_pubkey);
  EXPECT_TRUE(entity->identifier.has_value());
  EXPECT_EQ(entity->identifier.value(), identifier);
  EXPECT_TRUE(entity->kind.has_value());
  EXPECT_EQ(entity->kind.value(), kind);
  EXPECT_EQ(entity->relays, relays);
}

TEST_F(Nip19Test, GetEntityTypeTest) {
  EXPECT_EQ(GetEntityType("npub"), EntityType::kNpub);
  EXPECT_EQ(GetEntityType("nsec"), EntityType::kNsec);
  EXPECT_EQ(GetEntityType("note"), EntityType::kNote);
  EXPECT_EQ(GetEntityType("nprofile"), EntityType::kNprofile);
  EXPECT_EQ(GetEntityType("nevent"), EntityType::kNevent);
  EXPECT_EQ(GetEntityType("naddr"), EntityType::kNaddr);
  EXPECT_EQ(GetEntityType("unknown"), EntityType::kUnknown);
}

TEST_F(Nip19Test, ParseTLVEmpty) {
  ExtendedEntity entity;
  entity.type = EntityType::kNprofile;
  std::vector<uint8_t> empty_data;
  
  EXPECT_TRUE(ParseTLV(empty_data, entity));
}

TEST_F(Nip19Test, ParseTLVInvalidLength) {
  ExtendedEntity entity;
  entity.type = EntityType::kNprofile;
  std::vector<uint8_t> invalid_data = {0x00, 0xFF};  // Type 0, length 255, but no data
  
  EXPECT_FALSE(ParseTLV(invalid_data, entity));
}

}  // namespace nip19
}  // namespace nostr