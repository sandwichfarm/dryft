// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nostr/bech32/bech32_decoder.h"

#include <memory>
#include <vector>

#include "base/strings/hex_encode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {
namespace bech32 {

class Bech32DecoderTest : public testing::Test {
 protected:
  void SetUp() override {
    decoder_ = std::make_unique<Bech32Decoder>();
  }

  std::unique_ptr<Bech32Decoder> decoder_;
};

// Test basic Bech32 encoding/decoding
TEST_F(Bech32DecoderTest, BasicBech32EncodeDecode) {
  std::string hrp = "test";
  std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
  
  auto encoded = decoder_->EncodeBech32(hrp, data);
  ASSERT_TRUE(encoded.has_value());
  
  auto decoded = decoder_->DecodeBech32(encoded.value());
  ASSERT_TRUE(decoded.has_value());
  
  EXPECT_EQ(decoded->first, hrp);
  EXPECT_EQ(decoded->second, data);
}

// Test npub (public key) decoding
TEST_F(Bech32DecoderTest, DecodeNpub) {
  // Create a valid 32-byte pubkey
  std::vector<uint8_t> pubkey(32);
  for (int i = 0; i < 32; ++i) {
    pubkey[i] = i;
  }
  
  auto encoded = decoder_->EncodeBech32("npub", pubkey);
  ASSERT_TRUE(encoded.has_value());
  
  auto entity = decoder_->DecodeNostrEntity(encoded.value());
  ASSERT_TRUE(entity.has_value());
  
  EXPECT_EQ(entity.value()->type, NostrEntity::NPUB);
  
  auto simple_entity = static_cast<SimpleEntity*>(entity.value().get());
  EXPECT_EQ(simple_entity->data, pubkey);
}

// Test note (event ID) decoding
TEST_F(Bech32DecoderTest, DecodeNote) {
  // Create a valid 32-byte event ID
  std::vector<uint8_t> event_id(32);
  for (int i = 0; i < 32; ++i) {
    event_id[i] = 255 - i;
  }
  
  auto encoded = decoder_->EncodeBech32("note", event_id);
  ASSERT_TRUE(encoded.has_value());
  
  auto entity = decoder_->DecodeNostrEntity(encoded.value());
  ASSERT_TRUE(entity.has_value());
  
  EXPECT_EQ(entity.value()->type, NostrEntity::NOTE);
  
  auto simple_entity = static_cast<SimpleEntity*>(entity.value().get());
  EXPECT_EQ(simple_entity->data, event_id);
}

// Test nprofile (profile with relays) decoding
TEST_F(Bech32DecoderTest, DecodeNprofile) {
  // Create a valid 32-byte pubkey
  std::vector<uint8_t> pubkey(32);
  for (int i = 0; i < 32; ++i) {
    pubkey[i] = i;
  }
  
  // Add TLV data for relay
  std::vector<uint8_t> data = pubkey;
  
  // Add relay TLV entry (type 0)
  std::string relay = "wss://relay.example.com";
  data.push_back(0);  // type
  data.push_back(relay.length());  // length
  data.insert(data.end(), relay.begin(), relay.end());  // value
  
  auto encoded = decoder_->EncodeBech32("nprofile", data);
  ASSERT_TRUE(encoded.has_value());
  
  auto entity = decoder_->DecodeNostrEntity(encoded.value());
  ASSERT_TRUE(entity.has_value());
  
  EXPECT_EQ(entity.value()->type, NostrEntity::NPROFILE);
  
  auto complex_entity = static_cast<ComplexEntity*>(entity.value().get());
  EXPECT_EQ(complex_entity->primary_data, pubkey);
  
  auto relays = complex_entity->GetRelays();
  ASSERT_EQ(relays.size(), 1u);
  EXPECT_EQ(relays[0], relay);
}

// Test nevent (event with author and relays) decoding
TEST_F(Bech32DecoderTest, DecodeNevent) {
  // Create a valid 32-byte event ID
  std::vector<uint8_t> event_id(32);
  for (int i = 0; i < 32; ++i) {
    event_id[i] = i * 2;
  }
  
  // Add TLV data
  std::vector<uint8_t> data = event_id;
  
  // Add relay TLV entry (type 0)
  std::string relay = "wss://relay.example.com";
  data.push_back(0);  // type
  data.push_back(relay.length());  // length
  data.insert(data.end(), relay.begin(), relay.end());  // value
  
  // Add author TLV entry (type 1)
  std::vector<uint8_t> author(32);
  for (int i = 0; i < 32; ++i) {
    author[i] = 100 + i;
  }
  data.push_back(1);  // type
  data.push_back(32);  // length
  data.insert(data.end(), author.begin(), author.end());  // value
  
  auto encoded = decoder_->EncodeBech32("nevent", data);
  ASSERT_TRUE(encoded.has_value());
  
  auto entity = decoder_->DecodeNostrEntity(encoded.value());
  ASSERT_TRUE(entity.has_value());
  
  EXPECT_EQ(entity.value()->type, NostrEntity::NEVENT);
  
  auto complex_entity = static_cast<ComplexEntity*>(entity.value().get());
  EXPECT_EQ(complex_entity->primary_data, event_id);
  
  auto relays = complex_entity->GetRelays();
  ASSERT_EQ(relays.size(), 1u);
  EXPECT_EQ(relays[0], relay);
  
  std::string author_hex_str = complex_entity->GetAuthor();
  EXPECT_EQ(author_hex_str, base::HexEncode(author.data(), author.size()));
}

// Test naddr (parameterized replaceable event) decoding
TEST_F(Bech32DecoderTest, DecodeNaddr) {
  // Create a valid 32-byte identifier
  std::vector<uint8_t> identifier_bytes(32);
  for (int i = 0; i < 32; ++i) {
    identifier_bytes[i] = i + 50;
  }
  
  // Add TLV data
  std::vector<uint8_t> data = identifier_bytes;
  
  // Add kind TLV entry (type 2)
  uint32_t kind = 30023;  // Long-form content kind
  data.push_back(2);  // type
  data.push_back(4);  // length (32-bit integer)
  data.push_back((kind >> 24) & 0xFF);
  data.push_back((kind >> 16) & 0xFF);
  data.push_back((kind >> 8) & 0xFF);
  data.push_back(kind & 0xFF);
  
  // Add identifier TLV entry (type 3)
  std::string identifier = "my-article-identifier";
  data.push_back(3);  // type
  data.push_back(identifier.length());  // length
  data.insert(data.end(), identifier.begin(), identifier.end());  // value
  
  auto encoded = decoder_->EncodeBech32("naddr", data);
  ASSERT_TRUE(encoded.has_value());
  
  auto entity = decoder_->DecodeNostrEntity(encoded.value());
  ASSERT_TRUE(entity.has_value());
  
  EXPECT_EQ(entity.value()->type, NostrEntity::NADDR);
  
  auto complex_entity = static_cast<ComplexEntity*>(entity.value().get());
  EXPECT_EQ(complex_entity->primary_data, identifier_bytes);
  
  std::string kind_str = complex_entity->GetKind();
  EXPECT_EQ(kind_str, "30023");
  
  std::string identifier_str = complex_entity->GetIdentifier();
  EXPECT_EQ(identifier_str, identifier);
}

// Test nsec blocking for security
TEST_F(Bech32DecoderTest, BlockNsecForSecurity) {
  // Create a valid 32-byte private key
  std::vector<uint8_t> private_key(32);
  for (int i = 0; i < 32; ++i) {
    private_key[i] = i + 200;
  }
  
  auto encoded = decoder_->EncodeBech32("nsec", private_key);
  ASSERT_TRUE(encoded.has_value());
  
  auto entity = decoder_->DecodeNostrEntity(encoded.value());
  ASSERT_FALSE(entity.has_value());
  EXPECT_EQ(entity.error(), DecodeError::UNSUPPORTED_ENTITY);
}

// Test invalid Bech32 strings
TEST_F(Bech32DecoderTest, InvalidBech32Strings) {
  // Invalid character
  auto result1 = decoder_->DecodeBech32("test1invalid!");
  ASSERT_FALSE(result1.has_value());
  EXPECT_EQ(result1.error(), DecodeError::INVALID_CHARACTER);
  
  // Invalid length (too short)
  auto result2 = decoder_->DecodeBech32("test1");
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(result2.error(), DecodeError::INVALID_LENGTH);
  
  // No separator
  auto result3 = decoder_->DecodeBech32("testqpzry9x8gf2tvdw0s3jn54khce6mua7l");
  ASSERT_FALSE(result3.has_value());
  EXPECT_EQ(result3.error(), DecodeError::INVALID_LENGTH);
}

// Test invalid entity sizes
TEST_F(Bech32DecoderTest, InvalidEntitySizes) {
  // npub with wrong size (should be 32 bytes)
  std::vector<uint8_t> short_data = {0x01, 0x02, 0x03};
  auto encoded = decoder_->EncodeBech32("npub", short_data);
  ASSERT_TRUE(encoded.has_value());
  
  auto entity = decoder_->DecodeNostrEntity(encoded.value());
  ASSERT_FALSE(entity.has_value());
}

// Test unknown HRP
TEST_F(Bech32DecoderTest, UnknownHrp) {
  std::vector<uint8_t> data(32, 0);
  auto encoded = decoder_->EncodeBech32("unknown", data);
  ASSERT_TRUE(encoded.has_value());
  
  auto entity = decoder_->DecodeNostrEntity(encoded.value());
  ASSERT_FALSE(entity.has_value());
  EXPECT_EQ(entity.error(), DecodeError::UNKNOWN_HRP);
}

// Test error messages
TEST_F(Bech32DecoderTest, ErrorMessages) {
  EXPECT_EQ(Bech32Decoder::GetErrorMessage(DecodeError::INVALID_CHARACTER),
            "Invalid character in bech32 string");
  EXPECT_EQ(Bech32Decoder::GetErrorMessage(DecodeError::INVALID_CHECKSUM),
            "Invalid bech32 checksum");
  EXPECT_EQ(Bech32Decoder::GetErrorMessage(DecodeError::UNKNOWN_HRP),
            "Unknown human-readable part");
}

// Test IsValidNostrEntity utility function
TEST_F(Bech32DecoderTest, IsValidNostrEntity) {
  // Valid npub
  std::vector<uint8_t> pubkey(32, 0);
  auto encoded = decoder_->EncodeBech32("npub", pubkey);
  ASSERT_TRUE(encoded.has_value());
  EXPECT_TRUE(Bech32Decoder::IsValidNostrEntity(encoded.value()));
  
  // Invalid string
  EXPECT_FALSE(Bech32Decoder::IsValidNostrEntity("invalid"));
  
  // nsec (blocked)
  auto nsec_encoded = decoder_->EncodeBech32("nsec", pubkey);
  ASSERT_TRUE(nsec_encoded.has_value());
  EXPECT_FALSE(Bech32Decoder::IsValidNostrEntity(nsec_encoded.value()));
}

// Test complex entity with multiple relays
TEST_F(Bech32DecoderTest, MultipleRelays) {
  std::vector<uint8_t> pubkey(32, 1);
  std::vector<uint8_t> data = pubkey;
  
  // Add multiple relay TLV entries
  std::vector<std::string> relays = {
    "wss://relay1.example.com",
    "wss://relay2.example.com",
    "wss://relay3.example.com"
  };
  
  for (const auto& relay : relays) {
    data.push_back(0);  // type
    data.push_back(relay.length());  // length
    data.insert(data.end(), relay.begin(), relay.end());  // value
  }
  
  auto encoded = decoder_->EncodeBech32("nprofile", data);
  ASSERT_TRUE(encoded.has_value());
  
  auto entity = decoder_->DecodeNostrEntity(encoded.value());
  ASSERT_TRUE(entity.has_value());
  
  auto complex_entity = static_cast<ComplexEntity*>(entity.value().get());
  auto decoded_relays = complex_entity->GetRelays();
  
  ASSERT_EQ(decoded_relays.size(), relays.size());
  for (size_t i = 0; i < relays.size(); ++i) {
    EXPECT_EQ(decoded_relays[i], relays[i]);
  }
}

}  // namespace bech32
}  // namespace nostr