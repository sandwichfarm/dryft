// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/protocol/bech32.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdint>

namespace nostr {
namespace bech32 {

namespace {

const char* kCharset = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

// Generator coefficients for checksum
const uint32_t kGenerator[] = {0x3b6a57b2, 0x26508e6d, 0x1ea119fa,
                               0x3d4233dd, 0x2a1462b3};

uint32_t PolyMod(const std::vector<uint8_t>& values) {
  uint32_t chk = 1;
  for (auto value : values) {
    uint8_t top = chk >> 25;
    chk = (chk & 0x1ffffff) << 5 ^ value;
    for (int i = 0; i < 5; ++i) {
      chk ^= ((top >> i) & 1) ? kGenerator[i] : 0;
    }
  }
  return chk;
}

std::vector<uint8_t> HrpExpand(const std::string& hrp) {
  std::vector<uint8_t> ret;
  ret.reserve(hrp.size() * 2 + 1);
  for (char c : hrp) {
    ret.push_back(c >> 5);
  }
  ret.push_back(0);
  for (char c : hrp) {
    ret.push_back(c & 31);
  }
  return ret;
}

std::vector<uint8_t> CreateChecksum(const std::string& hrp,
                                    const std::vector<uint8_t>& data) {
  std::vector<uint8_t> values = HrpExpand(hrp);
  values.insert(values.end(), data.begin(), data.end());
  values.insert(values.end(), 6, 0);
  uint32_t polymod = PolyMod(values) ^ 1;
  std::vector<uint8_t> checksum(6);
  for (int i = 0; i < 6; ++i) {
    checksum[i] = (polymod >> 5 * (5 - i)) & 31;
  }
  return checksum;
}

}  // namespace

bool VerifyChecksum(const std::string& hrp, const std::vector<uint8_t>& data) {
  if (data.size() < 6) {
    return false;
  }
  std::vector<uint8_t> values = HrpExpand(hrp);
  values.insert(values.end(), data.begin(), data.end());
  return PolyMod(values) == 1;
}

std::optional<DecodedBech32> Decode(const std::string& bech32) {
  // Find separator
  size_t pos = bech32.rfind('1');
  if (pos == std::string::npos || pos == 0 || pos + 7 > bech32.size()) {
    return std::nullopt;
  }

  // Extract HRP
  std::string hrp = bech32.substr(0, pos);
  
  // Validate HRP
  for (char c : hrp) {
    if (c < 33 || c > 126) {
      return std::nullopt;
    }
  }

  // Convert to lowercase for checksum
  std::string hrp_lower = hrp;
  std::transform(hrp_lower.begin(), hrp_lower.end(), hrp_lower.begin(),
                 [](char c) { return std::tolower(c); });

  // Decode data part
  std::vector<uint8_t> data;
  for (size_t i = pos + 1; i < bech32.size(); ++i) {
    char c = std::tolower(bech32[i]);
    const char* p = std::strchr(kCharset, c);
    if (!p) {
      return std::nullopt;
    }
    data.push_back(p - kCharset);
  }

  // Verify checksum
  if (!VerifyChecksum(hrp_lower, data)) {
    return std::nullopt;
  }

  // Remove checksum
  data.resize(data.size() - 6);

  return DecodedBech32{hrp_lower, data};
}

std::string Encode(const std::string& hrp, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> checksum = CreateChecksum(hrp, data);
  std::string result = hrp + '1';
  
  for (uint8_t d : data) {
    result += kCharset[d];
  }
  for (uint8_t c : checksum) {
    result += kCharset[c];
  }
  
  return result;
}

std::vector<uint8_t> ConvertBits(const std::vector<uint8_t>& data,
                                  int from_bits,
                                  int to_bits,
                                  bool pad) {
  int acc = 0;
  int bits = 0;
  std::vector<uint8_t> ret;
  int maxv = (1 << to_bits) - 1;
  int max_acc = (1 << (from_bits + to_bits - 1)) - 1;
  
  for (uint8_t value : data) {
    if (value >> from_bits) {
      return {};
    }
    acc = ((acc << from_bits) | value) & max_acc;
    bits += from_bits;
    while (bits >= to_bits) {
      bits -= to_bits;
      ret.push_back((acc >> bits) & maxv);
    }
  }
  
  if (pad) {
    if (bits) {
      ret.push_back((acc << (to_bits - bits)) & maxv);
    }
  } else if (bits >= from_bits || ((acc << (to_bits - bits)) & maxv)) {
    return {};
  }
  
  return ret;
}

}  // namespace bech32
}  // namespace nostr