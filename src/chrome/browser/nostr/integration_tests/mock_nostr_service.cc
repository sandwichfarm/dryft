// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/integration_tests/mock_nostr_service.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/nostr/integration_tests/test_helpers.h"

using ::testing::_;
using ::testing::Invoke;

namespace nostr {

MockNostrService::MockNostrService() {
  SetDefaultBehavior();
}

MockNostrService::~MockNostrService() = default;

void MockNostrService::SetDefaultBehavior() {
  // Set up default mock behavior
  ON_CALL(*this, GetPublicKey(_, _))
      .WillByDefault(Invoke([this](const GURL& origin,
                                  GetPublicKeyCallback callback) {
        if (origin_permissions_[origin]) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), true, test_public_key_));
        } else {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), false, ""));
        }
      }));
  
  ON_CALL(*this, SignEvent(_, _, _))
      .WillByDefault(Invoke([this](const GURL& origin,
                                  const std::string& event_json,
                                  SignEventCallback callback) {
        if (origin_permissions_[origin] && signing_enabled_) {
          // Mock sign the event
          std::string signed_event = test::SignEvent(event_json, "mock_privkey");
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), true, signed_event));
        } else {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), false, ""));
        }
      }));
  
  ON_CALL(*this, GetRelays(_, _))
      .WillByDefault(Invoke([this](const GURL& origin,
                                  GetRelaysCallback callback) {
        if (origin_permissions_[origin]) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), true, test_relays_));
        } else {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), false,
                            std::map<std::string, RelayPolicy>()));
        }
      }));
  
  ON_CALL(*this, CreateKey(_, _, _))
      .WillByDefault(Invoke([this](const std::string& name,
                                  const std::string& passphrase,
                                  CreateKeyCallback callback) {
        auto key_pair = test::GenerateTestKeyPair();
        AddTestKey(name, key_pair.public_key, key_pair.private_key);
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), true, key_pair.public_key));
      }));
  
  ON_CALL(*this, GetStoredKeys(_))
      .WillByDefault(Invoke([this](GetStoredKeysCallback callback) {
        std::vector<KeyInfo> keys;
        for (const auto& [pubkey, key] : test_keys_) {
          KeyInfo info;
          info.name = key.name;
          info.public_key = key.public_key;
          info.is_unlocked = key.unlocked;
          keys.push_back(info);
        }
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), std::move(keys)));
      }));
  
  ON_CALL(*this, SetActiveKey(_, _))
      .WillByDefault(Invoke([this](const std::string& public_key,
                                  SetActiveKeyCallback callback) {
        if (test_keys_.find(public_key) != test_keys_.end()) {
          active_key_pubkey_ = public_key;
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), true));
        } else {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), false));
        }
      }));
  
  ON_CALL(*this, GetActivePublicKey())
      .WillByDefault(Invoke([this]() {
        return active_key_pubkey_;
      }));
  
  ON_CALL(*this, IsKeyUnlocked(_))
      .WillByDefault(Invoke([this](const std::string& public_key) {
        auto it = test_keys_.find(public_key);
        return it != test_keys_.end() && it->second.unlocked;
      }));
}

void MockNostrService::SetPublicKey(const std::string& pubkey) {
  test_public_key_ = pubkey;
}

void MockNostrService::SetRelays(const std::map<std::string, RelayPolicy>& relays) {
  test_relays_ = relays;
}

void MockNostrService::SetSigningEnabled(bool enabled) {
  signing_enabled_ = enabled;
}

void MockNostrService::SetPermissionForOrigin(const GURL& origin, bool granted) {
  origin_permissions_[origin] = granted;
}

void MockNostrService::AddTestKey(const std::string& name,
                                 const std::string& pubkey,
                                 const std::string& privkey) {
  TestKey key;
  key.name = name;
  key.public_key = pubkey;
  key.private_key = privkey;
  key.unlocked = false;
  test_keys_[pubkey] = key;
  
  // Set as active if it's the first key
  if (active_key_pubkey_.empty()) {
    active_key_pubkey_ = pubkey;
  }
}

void MockNostrService::SetKeyUnlocked(const std::string& pubkey, bool unlocked) {
  if (test_keys_.find(pubkey) != test_keys_.end()) {
    test_keys_[pubkey].unlocked = unlocked;
  }
}

}  // namespace nostr