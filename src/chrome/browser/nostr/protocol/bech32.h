// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_PROTOCOL_BECH32_H_
#define CHROME_BROWSER_NOSTR_PROTOCOL_BECH32_H_

#include <string>
#include <vector>
#include <optional>

namespace nostr {
namespace bech32 {

// Bech32 encoding/decoding based on BIP-173
// https://github.com/bitcoin/bips/blob/master/bip-0173.mediawiki

// Decode a bech32 string, returning the HRP and data
struct DecodedBech32 {
  std::string hrp;
  std::vector<uint8_t> data;
};

// Decode a bech32 string
std::optional<DecodedBech32> Decode(const std::string& bech32);

// Encode data to bech32
std::string Encode(const std::string& hrp, const std::vector<uint8_t>& data);

// Convert from 5-bit to 8-bit encoding
std::vector<uint8_t> ConvertBits(const std::vector<uint8_t>& data,
                                  int from_bits,
                                  int to_bits,
                                  bool pad);

// Validate checksum
bool VerifyChecksum(const std::string& hrp, const std::vector<uint8_t>& data);

}  // namespace bech32
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_PROTOCOL_BECH32_H_