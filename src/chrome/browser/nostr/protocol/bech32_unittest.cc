// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/protocol/bech32.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {
namespace bech32 {

class Bech32Test : public testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(Bech32Test, DecodeValidBech32) {
  // Test vector from BIP-173
  std::string valid = "BC1QW508D6QEJXTDG4Y5R3ZARVARY0C5XW7KV8F3T4";
  auto result = Decode(valid);
  
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->hrp, "bc");
  
  // Verify data length
  EXPECT_EQ(result->data.size(), 34);
}

TEST_F(Bech32Test, DecodeInvalidChecksum) {
  // Modified last character to make checksum invalid
  std::string invalid = "BC1QW508D6QEJXTDG4Y5R3ZARVARY0C5XW7KV8F3T5";
  auto result = Decode(invalid);
  
  EXPECT_FALSE(result.has_value());
}

TEST_F(Bech32Test, DecodeMissingSeparator) {
  std::string invalid = "BC0W508D6QEJXTDG4Y5R3ZARVARY0C5XW7KV8F3T4";
  auto result = Decode(invalid);
  
  EXPECT_FALSE(result.has_value());
}

TEST_F(Bech32Test, DecodeInvalidCharacter) {
  std::string invalid = "BC1QW508D6QEJXTDG4Y5R3ZARVARYO0C5XW7KV8F3T4";
  auto result = Decode(invalid);
  
  EXPECT_FALSE(result.has_value());
}

TEST_F(Bech32Test, EncodeDecode) {
  std::string hrp = "test";
  std::vector<uint8_t> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  
  std::string encoded = Encode(hrp, data);
  auto decoded = Decode(encoded);
  
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->hrp, hrp);
  EXPECT_EQ(decoded->data, data);
}

TEST_F(Bech32Test, ConvertBits5to8) {
  // Test converting from 5-bit to 8-bit
  std::vector<uint8_t> data5 = {0x1f, 0x1c, 0x17, 0x14, 0x00, 0x00, 0x00, 0x00};
  auto data8 = ConvertBits(data5, 5, 8, false);
  
  ASSERT_FALSE(data8.empty());
  EXPECT_EQ(data8.size(), 5);
  EXPECT_EQ(data8[0], 0xff);
  EXPECT_EQ(data8[1], 0x00);
  EXPECT_EQ(data8[2], 0x00);
  EXPECT_EQ(data8[3], 0x00);
  EXPECT_EQ(data8[4], 0x00);
}

TEST_F(Bech32Test, ConvertBits8to5) {
  // Test converting from 8-bit to 5-bit
  std::vector<uint8_t> data8 = {0xff, 0x00, 0x00, 0x00, 0x00};
  auto data5 = ConvertBits(data8, 8, 5, true);
  
  ASSERT_FALSE(data5.empty());
  EXPECT_EQ(data5.size(), 8);
  EXPECT_EQ(data5[0], 0x1f);
  EXPECT_EQ(data5[1], 0x1c);
  EXPECT_EQ(data5[2], 0x00);
}

TEST_F(Bech32Test, VerifyChecksumValid) {
  std::string hrp = "bc";
  std::vector<uint8_t> data = {
    0, 14, 20, 15, 7, 13, 26, 0, 25, 18, 6, 11, 13, 8, 21, 4, 20, 3,
    17, 2, 29, 3, 12, 29, 3, 4, 15, 24, 20, 6, 14, 30, 22, 
    // Checksum
    13, 19, 16, 30, 29, 20
  };
  
  EXPECT_TRUE(VerifyChecksum(hrp, data));
}

TEST_F(Bech32Test, VerifyChecksumInvalid) {
  std::string hrp = "bc";
  std::vector<uint8_t> data = {
    0, 14, 20, 15, 7, 13, 26, 0, 25, 18, 6, 11, 13, 8, 21, 4, 20, 3,
    17, 2, 29, 3, 12, 29, 3, 4, 15, 24, 20, 6, 14, 30, 22, 
    // Invalid checksum
    13, 19, 16, 30, 29, 21
  };
  
  EXPECT_FALSE(VerifyChecksum(hrp, data));
}

}  // namespace bech32
}  // namespace nostr