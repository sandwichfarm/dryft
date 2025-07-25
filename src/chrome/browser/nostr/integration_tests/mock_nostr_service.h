// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_INTEGRATION_TESTS_MOCK_NOSTR_SERVICE_H_
#define CHROME_BROWSER_NOSTR_INTEGRATION_TESTS_MOCK_NOSTR_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/nostr/nostr_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace nostr {

class MockNostrService : public NostrService {
 public:
  MockNostrService();
  ~MockNostrService() override;

  // NostrService implementation with MOCK_METHOD macros
  MOCK_METHOD(void, GetPublicKey,
              (const GURL& origin,
               GetPublicKeyCallback callback),
              (override));
  
  MOCK_METHOD(void, SignEvent,
              (const GURL& origin,
               const std::string& event_json,
               SignEventCallback callback),
              (override));
  
  MOCK_METHOD(void, GetRelays,
              (const GURL& origin,
               GetRelaysCallback callback),
              (override));
  
  MOCK_METHOD(void, Nip04Encrypt,
              (const GURL& origin,
               const std::string& peer_pubkey,
               const std::string& plaintext,
               Nip04Callback callback),
              (override));
  
  MOCK_METHOD(void, Nip04Decrypt,
              (const GURL& origin,
               const std::string& peer_pubkey,
               const std::string& ciphertext,
               Nip04Callback callback),
              (override));
  
  MOCK_METHOD(void, Nip44Encrypt,
              (const GURL& origin,
               const std::string& peer_pubkey,
               const std::string& plaintext,
               Nip44Callback callback),
              (override));
  
  MOCK_METHOD(void, Nip44Decrypt,
              (const GURL& origin,
               const std::string& peer_pubkey,
               const std::string& ciphertext,
               Nip44Callback callback),
              (override));
  
  MOCK_METHOD(NostrPermissionManager*, GetPermissionManager, (), (override));
  
  MOCK_METHOD(void, CreateKey,
              (const std::string& name,
               const std::string& passphrase,
               CreateKeyCallback callback),
              (override));
  
  MOCK_METHOD(void, ImportKey,
              (const std::string& name,
               const std::string& private_key,
               const std::string& passphrase,
               ImportKeyCallback callback),
              (override));
  
  MOCK_METHOD(void, DeleteKey,
              (const std::string& public_key,
               DeleteKeyCallback callback),
              (override));
  
  MOCK_METHOD(void, GetStoredKeys,
              (GetStoredKeysCallback callback),
              (override));
  
  MOCK_METHOD(void, SetActiveKey,
              (const std::string& public_key,
               SetActiveKeyCallback callback),
              (override));
  
  MOCK_METHOD(std::string, GetActivePublicKey, (), (const, override));
  
  MOCK_METHOD(void, UnlockKey,
              (const std::string& public_key,
               const std::string& passphrase,
               UnlockKeyCallback callback),
              (override));
  
  MOCK_METHOD(void, LockKey,
              (const std::string& public_key),
              (override));
  
  MOCK_METHOD(bool, IsKeyUnlocked,
              (const std::string& public_key),
              (const, override));

  // Helper methods for testing
  void SetDefaultBehavior();
  void SetPublicKey(const std::string& pubkey);
  void SetRelays(const std::map<std::string, RelayPolicy>& relays);
  void SetSigningEnabled(bool enabled);
  void SetPermissionForOrigin(const GURL& origin, bool granted);
  
  // Simulate key operations
  void AddTestKey(const std::string& name,
                  const std::string& pubkey,
                  const std::string& privkey);
  void SetKeyUnlocked(const std::string& pubkey, bool unlocked);

 private:
  std::string test_public_key_;
  std::map<std::string, RelayPolicy> test_relays_;
  bool signing_enabled_ = true;
  std::map<GURL, bool> origin_permissions_;
  
  struct TestKey {
    std::string name;
    std::string public_key;
    std::string private_key;
    bool unlocked = false;
  };
  std::map<std::string, TestKey> test_keys_;
  std::string active_key_pubkey_;
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_INTEGRATION_TESTS_MOCK_NOSTR_SERVICE_H_