# Low Level Design Document: Key Management
## dryft browser - Secure Nostr Key Storage and Management

### 1. Component Overview

The Key Management component provides secure storage, generation, and management of Nostr private keys. It integrates with platform-specific secure storage mechanisms, supports multiple accounts, implements key derivation from seed phrases, and ensures keys are never exposed to untrusted code.

### 2. Detailed Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    UI Layer                              │
│  ┌─────────────────────────────────────────────────┐   │
│  │           Key Management Interface                │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Account List │  │ Import/Export     │       │   │
│  │  │              │  │ Interface         │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                 Key Service Layer                        │
│  ┌─────────────────────────────────────────────────┐   │
│  │              Key Service API                      │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Account Mgmt │  │ Crypto Operations │       │   │
│  │  │              │  │                   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│              Secure Storage Layer                        │
│  ┌─────────────────────────────────────────────────┐   │
│  │         Platform-Specific Key Storage             │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │OS Keychain   │  │ Encryption Layer  │       │   │
│  │  │Integration   │  │                   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│           Key Derivation & Generation                    │
│  ┌─────────────────────────────────────────────────┐   │
│  │            BIP-39/BIP-32 Implementation           │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Seed Phrase  │  │ Key Derivation    │       │   │
│  │  │ Generation   │  │ Path              │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│              Hardware Security Module                    │
│  ┌─────────────────────────────────────────────────┐   │
│  │              HSM Integration (Optional)           │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Hardware Key │  │ Secure Element    │       │   │
│  │  │ Support      │  │ Interface         │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### 3. Platform-Specific Storage Implementation

```cpp
// Platform abstraction layer
class SecureKeyStorage {
 public:
  virtual ~SecureKeyStorage() = default;
  
  virtual bool StoreKey(const std::string& account_id,
                       const std::string& encrypted_key) = 0;
  virtual bool RetrieveKey(const std::string& account_id,
                          std::string* encrypted_key) = 0;
  virtual bool DeleteKey(const std::string& account_id) = 0;
  virtual std::vector<std::string> ListAccounts() = 0;
  virtual bool IsAvailable() = 0;
  
  static std::unique_ptr<SecureKeyStorage> Create();
};

// macOS Keychain implementation
#if defined(OS_MAC)
class MacKeychainStorage : public SecureKeyStorage {
 public:
  bool StoreKey(const std::string& account_id,
               const std::string& encrypted_key) override {
    CFStringRef service = CFSTR("com.dryft.browser.nostr");
    CFStringRef account = CFStringCreateWithCString(
        kCFAllocatorDefault, account_id.c_str(), 
        kCFStringEncodingUTF8);
    CFDataRef data = CFDataCreate(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(encrypted_key.data()),
        encrypted_key.size());
    
    // Create query
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, service);
    CFDictionarySetValue(query, kSecAttrAccount, account);
    CFDictionarySetValue(query, kSecValueData, data);
    CFDictionarySetValue(query, kSecAttrAccessible,
                        kSecAttrAccessibleWhenUnlockedThisDeviceOnly);
    
    // Delete existing item if present
    SecItemDelete(query);
    
    // Add new item
    OSStatus status = SecItemAdd(query, nullptr);
    
    CFRelease(query);
    CFRelease(data);
    CFRelease(account);
    
    return status == errSecSuccess;
  }
  
  bool RetrieveKey(const std::string& account_id,
                  std::string* encrypted_key) override {
    CFStringRef service = CFSTR("com.dryft.browser.nostr");
    CFStringRef account = CFStringCreateWithCString(
        kCFAllocatorDefault, account_id.c_str(),
        kCFStringEncodingUTF8);
    
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, service);
    CFDictionarySetValue(query, kSecAttrAccount, account);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    
    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, &result);
    
    bool success = false;
    if (status == errSecSuccess && result) {
      CFDataRef data = static_cast<CFDataRef>(result);
      const UInt8* bytes = CFDataGetBytePtr(data);
      CFIndex length = CFDataGetLength(data);
      
      encrypted_key->assign(reinterpret_cast<const char*>(bytes),
                           length);
      success = true;
      
      CFRelease(result);
    }
    
    CFRelease(query);
    CFRelease(account);
    
    return success;
  }
};
#endif

// Windows Credential Manager implementation
#if defined(OS_WIN)
class WindowsCredentialStorage : public SecureKeyStorage {
 public:
  bool StoreKey(const std::string& account_id,
               const std::string& encrypted_key) override {
    std::wstring target = L"dryft/Nostr/" + 
                         base::UTF8ToWide(account_id);
    
    CREDENTIALW cred = {0};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(target.c_str());
    cred.Comment = L"Nostr private key";
    cred.CredentialBlobSize = encrypted_key.size();
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(
        const_cast<char*>(encrypted_key.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    
    return CredWriteW(&cred, 0) == TRUE;
  }
  
  bool RetrieveKey(const std::string& account_id,
                  std::string* encrypted_key) override {
    std::wstring target = L"dryft/Nostr/" + 
                         base::UTF8ToWide(account_id);
    
    PCREDENTIALW cred = nullptr;
    if (CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred) != TRUE) {
      return false;
    }
    
    encrypted_key->assign(
        reinterpret_cast<const char*>(cred->CredentialBlob),
        cred->CredentialBlobSize);
    
    CredFree(cred);
    return true;
  }
};
#endif

// Linux Secret Service implementation
#if defined(OS_LINUX)
class LinuxSecretServiceStorage : public SecureKeyStorage {
 public:
  LinuxSecretServiceStorage() {
    GError* error = nullptr;
    service_ = secret_service_get_sync(
        SECRET_SERVICE_NONE, nullptr, &error);
    
    if (error) {
      LOG(ERROR) << "Failed to connect to Secret Service: " 
                 << error->message;
      g_error_free(error);
    }
  }
  
  bool StoreKey(const std::string& account_id,
               const std::string& encrypted_key) override {
    if (!service_) return false;
    
    GHashTable* attributes = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, g_free);
    
    g_hash_table_insert(attributes,
                       g_strdup("application"),
                       g_strdup("dryft"));
    g_hash_table_insert(attributes,
                       g_strdup("account"),
                       g_strdup(account_id.c_str()));
    
    GError* error = nullptr;
    bool success = secret_service_store_sync(
        service_,
        &schema_,
        attributes,
        SECRET_COLLECTION_DEFAULT,
        "Nostr private key",
        encrypted_key.c_str(),
        nullptr,
        &error);
    
    g_hash_table_unref(attributes);
    
    if (error) {
      LOG(ERROR) << "Failed to store key: " << error->message;
      g_error_free(error);
      return false;
    }
    
    return success;
  }
  
 private:
  SecretService* service_ = nullptr;
  static const SecretSchema schema_;
};
#endif
```

### 4. Key Encryption Layer

```cpp
class KeyEncryption {
 public:
  struct EncryptedKey {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> salt;
    std::vector<uint8_t> nonce;
    int iterations;
    std::string algorithm;
  };
  
  // Encrypt key with password
  static EncryptedKey EncryptKey(const std::string& private_key,
                                const std::string& password) {
    EncryptedKey result;
    
    // Generate random salt
    result.salt.resize(32);
    crypto::RandBytes(result.salt.data(), result.salt.size());
    
    // Derive encryption key using Argon2
    std::vector<uint8_t> derived_key(32);
    result.iterations = 3;  // Argon2 iterations
    
    argon2id_hash_raw(
        result.iterations,
        1 << 16,  // 64MB memory
        1,        // 1 thread
        password.data(), password.size(),
        result.salt.data(), result.salt.size(),
        derived_key.data(), derived_key.size());
    
    // Generate nonce
    result.nonce.resize(12);
    crypto::RandBytes(result.nonce.data(), result.nonce.size());
    
    // Encrypt using AES-256-GCM
    result.algorithm = "AES-256-GCM";
    result.ciphertext = AES256GCM_Encrypt(
        private_key, derived_key, result.nonce);
    
    return result;
  }
  
  // Decrypt key with password
  static bool DecryptKey(const EncryptedKey& encrypted,
                        const std::string& password,
                        std::string* private_key) {
    // Derive decryption key
    std::vector<uint8_t> derived_key(32);
    
    argon2id_hash_raw(
        encrypted.iterations,
        1 << 16,  // 64MB memory
        1,        // 1 thread
        password.data(), password.size(),
        encrypted.salt.data(), encrypted.salt.size(),
        derived_key.data(), derived_key.size());
    
    // Decrypt
    return AES256GCM_Decrypt(
        encrypted.ciphertext, derived_key, 
        encrypted.nonce, private_key);
  }
  
 private:
  static std::vector<uint8_t> AES256GCM_Encrypt(
      const std::string& plaintext,
      const std::vector<uint8_t>& key,
      const std::vector<uint8_t>& nonce) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                      key.data(), nonce.data());
    
    std::vector<uint8_t> ciphertext(plaintext.size() + 16);
    int len;
    
    EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                     reinterpret_cast<const uint8_t*>(plaintext.data()),
                     plaintext.size());
    
    int final_len;
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &final_len);
    
    // Get tag
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16,
                       ciphertext.data() + plaintext.size());
    
    EVP_CIPHER_CTX_free(ctx);
    
    return ciphertext;
  }
};
```

### 5. Key Generation and Derivation

```cpp
class KeyGenerator {
 public:
  // Generate new private key
  static std::string GeneratePrivateKey() {
    std::vector<uint8_t> key(32);
    crypto::RandBytes(key.data(), key.size());
    return HexEncode(key);
  }
  
  // Derive public key from private key
  static std::string GetPublicKey(const std::string& private_key) {
    secp256k1_context* ctx = secp256k1_context_create(
        SECP256K1_CONTEXT_SIGN);
    
    std::vector<uint8_t> privkey_bytes = HexDecode(private_key);
    
    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_create(ctx, &pubkey, 
                                   privkey_bytes.data())) {
      secp256k1_context_destroy(ctx);
      return "";
    }
    
    // Get x-only pubkey (32 bytes)
    secp256k1_xonly_pubkey xonly_pubkey;
    secp256k1_xonly_pubkey_from_pubkey(ctx, &xonly_pubkey, 
                                       nullptr, &pubkey);
    
    unsigned char pubkey_bytes[32];
    secp256k1_xonly_pubkey_serialize(ctx, pubkey_bytes, 
                                     &xonly_pubkey);
    
    secp256k1_context_destroy(ctx);
    
    return HexEncode(pubkey_bytes, 32);
  }
};

// BIP-39 mnemonic implementation
class MnemonicGenerator {
 public:
  // Generate mnemonic phrase
  static std::vector<std::string> GenerateMnemonic(int strength = 128) {
    // Generate entropy
    int entropy_bytes = strength / 8;
    std::vector<uint8_t> entropy(entropy_bytes);
    crypto::RandBytes(entropy.data(), entropy.size());
    
    return EntropyToMnemonic(entropy);
  }
  
  // Convert mnemonic to seed
  static std::vector<uint8_t> MnemonicToSeed(
      const std::vector<std::string>& mnemonic,
      const std::string& passphrase = "") {
    std::string mnemonic_str = base::JoinString(mnemonic, " ");
    std::string salt = "mnemonic" + passphrase;
    
    std::vector<uint8_t> seed(64);
    
    PKCS5_PBKDF2_HMAC(
        mnemonic_str.c_str(), mnemonic_str.length(),
        reinterpret_cast<const uint8_t*>(salt.c_str()), salt.length(),
        2048,  // iterations
        EVP_sha512(),
        seed.size(), seed.data());
    
    return seed;
  }
  
  // Derive Nostr key from seed
  static std::string DeriveNostrKey(const std::vector<uint8_t>& seed,
                                   int account_index = 0) {
    // Use BIP-32 derivation path: m/44'/1237'/account'/0/0
    // 1237 is proposed for Nostr
    
    HDKey master = HDKey::FromSeed(seed);
    
    HDKey derived = master
        .Derive(44 | 0x80000000)   // purpose
        .Derive(1237 | 0x80000000) // coin type (Nostr)
        .Derive(account_index | 0x80000000) // account
        .Derive(0)                 // change
        .Derive(0);                // address index
    
    return HexEncode(derived.private_key);
  }
  
 private:
  static std::vector<std::string> EntropyToMnemonic(
      const std::vector<uint8_t>& entropy) {
    // Implementation of BIP-39 entropy to mnemonic conversion
    // Using standard 2048 word list
    static const std::vector<std::string> wordlist = LoadWordlist();
    
    // Add checksum
    std::vector<uint8_t> hash(32);
    SHA256(entropy.data(), entropy.size(), hash.data());
    
    // Convert to bit string and append checksum bits
    std::string bits = ToBitString(entropy);
    bits += ToBitString({hash[0]}).substr(0, entropy.size() * 8 / 32);
    
    // Split into 11-bit chunks and map to words
    std::vector<std::string> mnemonic;
    for (size_t i = 0; i < bits.length(); i += 11) {
      std::string chunk = bits.substr(i, 11);
      int index = std::stoi(chunk, nullptr, 2);
      mnemonic.push_back(wordlist[index]);
    }
    
    return mnemonic;
  }
};
```

### 6. Account Management

```cpp
class NostrAccountManager {
 public:
  struct Account {
    std::string id;
    std::string name;
    std::string public_key;
    base::Time created_at;
    base::Time last_used;
    bool is_derived;  // From seed phrase
    int derivation_index;  // If derived
  };
  
  // Account operations
  Account CreateAccount(const std::string& name);
  Account ImportAccount(const std::string& name,
                       const std::string& private_key);
  Account DeriveAccount(const std::string& name,
                       const std::vector<std::string>& mnemonic,
                       int index);
  bool DeleteAccount(const std::string& account_id);
  std::vector<Account> ListAccounts();
  
  // Key operations
  bool UnlockAccount(const std::string& account_id,
                    const std::string& password);
  void LockAccount(const std::string& account_id);
  bool IsAccountUnlocked(const std::string& account_id);
  
  // Get decrypted private key (only if unlocked)
  bool GetPrivateKey(const std::string& account_id,
                    std::string* private_key);
  
 private:
  struct UnlockedKey {
    std::string private_key;
    base::Time unlock_time;
    base::TimeDelta timeout = base::TimeDelta::FromMinutes(5);
  };
  
  void SaveAccountMetadata(const Account& account);
  std::unique_ptr<Account> LoadAccountMetadata(
      const std::string& account_id);
  void CheckTimeouts();
  
  std::unique_ptr<SecureKeyStorage> key_storage_;
  std::map<std::string, UnlockedKey> unlocked_keys_;
  base::RepeatingTimer timeout_timer_;
  base::Lock lock_;
};

// Implementation
NostrAccountManager::Account NostrAccountManager::CreateAccount(
    const std::string& name) {
  base::AutoLock auto_lock(lock_);
  
  // Generate new private key
  std::string private_key = KeyGenerator::GeneratePrivateKey();
  std::string public_key = KeyGenerator::GetPublicKey(private_key);
  
  // Create account
  Account account;
  account.id = base::GenerateGUID();
  account.name = name;
  account.public_key = public_key;
  account.created_at = base::Time::Now();
  account.last_used = account.created_at;
  account.is_derived = false;
  account.derivation_index = -1;
  
  // Encrypt and store key
  std::string password = RequestPasswordFromUser();
  if (password.empty()) {
    throw std::runtime_error("Password required");
  }
  
  auto encrypted = KeyEncryption::EncryptKey(private_key, password);
  std::string encrypted_data = SerializeEncryptedKey(encrypted);
  
  if (!key_storage_->StoreKey(account.id, encrypted_data)) {
    throw std::runtime_error("Failed to store key");
  }
  
  // Save metadata
  SaveAccountMetadata(account);
  
  // Clear sensitive data
  SecureZeroMemory(private_key);
  SecureZeroMemory(password);
  
  return account;
}

bool NostrAccountManager::UnlockAccount(const std::string& account_id,
                                      const std::string& password) {
  base::AutoLock auto_lock(lock_);
  
  // Check if already unlocked
  if (unlocked_keys_.find(account_id) != unlocked_keys_.end()) {
    return true;
  }
  
  // Retrieve encrypted key
  std::string encrypted_data;
  if (!key_storage_->RetrieveKey(account_id, &encrypted_data)) {
    return false;
  }
  
  // Decrypt key
  auto encrypted = DeserializeEncryptedKey(encrypted_data);
  std::string private_key;
  if (!KeyEncryption::DecryptKey(encrypted, password, &private_key)) {
    return false;
  }
  
  // Store in memory (temporarily)
  UnlockedKey unlocked;
  unlocked.private_key = private_key;
  unlocked.unlock_time = base::Time::Now();
  unlocked_keys_[account_id] = unlocked;
  
  // Clear sensitive data
  SecureZeroMemory(private_key);
  
  // Start timeout timer if needed
  if (!timeout_timer_.IsRunning()) {
    timeout_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(30),
        base::BindRepeating(&NostrAccountManager::CheckTimeouts,
                          base::Unretained(this)));
  }
  
  return true;
}
```

### 7. Hardware Security Module Support

```cpp
class HardwareKeyManager {
 public:
  struct HardwareDevice {
    std::string name;
    std::string vendor;
    std::string serial;
    bool supports_nostr;
  };
  
  // Device discovery
  std::vector<HardwareDevice> ListDevices();
  bool ConnectDevice(const HardwareDevice& device);
  void DisconnectDevice();
  
  // Key operations
  std::string GetPublicKey(int account_index);
  std::string SignEvent(int account_index, const NostrEvent& event);
  
 private:
  // USB HID communication
  class USBDevice {
   public:
    bool Open(uint16_t vendor_id, uint16_t product_id);
    bool SendCommand(const std::vector<uint8_t>& command,
                    std::vector<uint8_t>* response);
    void Close();
    
   private:
    hid_device* device_ = nullptr;
  };
  
  // Protocol implementation (e.g., for Ledger)
  std::vector<uint8_t> BuildAPDU(uint8_t ins, uint8_t p1, uint8_t p2,
                                const std::vector<uint8_t>& data);
  bool ParseResponse(const std::vector<uint8_t>& response,
                    std::vector<uint8_t>* data);
  
  std::unique_ptr<USBDevice> device_;
};

// Example: Sign with hardware wallet
std::string HardwareKeyManager::SignEvent(int account_index,
                                        const NostrEvent& event) {
  if (!device_) {
    throw std::runtime_error("No device connected");
  }
  
  // Serialize event for signing
  std::string serialized = SerializeEventForSigning(event);
  std::vector<uint8_t> hash(32);
  SHA256(reinterpret_cast<const uint8_t*>(serialized.data()),
         serialized.size(), hash.data());
  
  // Build signing command
  std::vector<uint8_t> command;
  command.push_back(0x80);  // CLA
  command.push_back(0x50);  // INS: SIGN
  command.push_back(account_index);  // P1
  command.push_back(0x00);  // P2
  command.push_back(hash.size());  // Data length
  command.insert(command.end(), hash.begin(), hash.end());
  
  // Send to device
  std::vector<uint8_t> response;
  if (!device_->SendCommand(command, &response)) {
    throw std::runtime_error("Failed to sign");
  }
  
  // Extract signature
  if (response.size() != 64) {
    throw std::runtime_error("Invalid signature length");
  }
  
  return HexEncode(response);
}
```

### 8. Backup and Recovery

```cpp
class KeyBackupManager {
 public:
  struct BackupData {
    int version;
    std::string timestamp;
    std::vector<EncryptedAccount> accounts;
    std::string checksum;
  };
  
  struct EncryptedAccount {
    std::string id;
    std::string name;
    std::string encrypted_key;
    std::string public_key;
  };
  
  // Create encrypted backup
  std::string CreateBackup(const std::string& password);
  
  // Restore from backup
  bool RestoreBackup(const std::string& backup_data,
                    const std::string& password);
  
  // Export single account
  std::string ExportAccount(const std::string& account_id,
                          ExportFormat format);
  
  enum ExportFormat {
    NSEC_BECH32,     // nsec1...
    HEX_FORMAT,      // Raw hex
    ENCRYPTED_JSON,  // Password-protected JSON
  };
  
 private:
  std::string SerializeBackup(const BackupData& data);
  std::unique_ptr<BackupData> DeserializeBackup(
      const std::string& data);
  
  NostrAccountManager* account_manager_;
};

// Backup creation
std::string KeyBackupManager::CreateBackup(const std::string& password) {
  BackupData backup;
  backup.version = 1;
  backup.timestamp = base::Time::Now().ToISO8601String();
  
  // Get all accounts
  auto accounts = account_manager_->ListAccounts();
  
  for (const auto& account : accounts) {
    // Get encrypted key data
    std::string encrypted_key;
    if (!key_storage_->RetrieveKey(account.id, &encrypted_key)) {
      continue;
    }
    
    EncryptedAccount enc_account;
    enc_account.id = account.id;
    enc_account.name = account.name;
    enc_account.encrypted_key = encrypted_key;
    enc_account.public_key = account.public_key;
    
    backup.accounts.push_back(enc_account);
  }
  
  // Calculate checksum
  std::string data_to_hash = SerializeBackup(backup);
  backup.checksum = CalculateSHA256(data_to_hash);
  
  // Encrypt entire backup
  auto encrypted = KeyEncryption::EncryptKey(
      SerializeBackup(backup), password);
  
  return base::Base64Encode(SerializeEncryptedKey(encrypted));
}

// Export formats
std::string KeyBackupManager::ExportAccount(
    const std::string& account_id,
    ExportFormat format) {
  // Require unlock first
  if (!account_manager_->IsAccountUnlocked(account_id)) {
    throw std::runtime_error("Account must be unlocked");
  }
  
  std::string private_key;
  if (!account_manager_->GetPrivateKey(account_id, &private_key)) {
    throw std::runtime_error("Failed to get private key");
  }
  
  switch (format) {
    case NSEC_BECH32: {
      // Convert to bech32 with "nsec" prefix
      std::vector<uint8_t> key_bytes = HexDecode(private_key);
      return Bech32Encode("nsec", key_bytes);
    }
    
    case HEX_FORMAT:
      return private_key;
      
    case ENCRYPTED_JSON: {
      // Create password-protected JSON export
      std::string password = RequestPasswordFromUser();
      auto encrypted = KeyEncryption::EncryptKey(private_key, password);
      
      rapidjson::Document doc;
      doc.SetObject();
      doc.AddMember("version", 1, doc.GetAllocator());
      doc.AddMember("encrypted", true, doc.GetAllocator());
      doc.AddMember("data", 
                   base::Base64Encode(SerializeEncryptedKey(encrypted)),
                   doc.GetAllocator());
      
      return JsonToString(doc);
    }
  }
  
  return "";
}
```

### 9. Security Utilities

```cpp
class SecurityUtils {
 public:
  // Secure memory clearing
  static void SecureZeroMemory(std::string& str) {
    if (!str.empty()) {
      OPENSSL_cleanse(&str[0], str.size());
      str.clear();
    }
  }
  
  static void SecureZeroMemory(std::vector<uint8_t>& vec) {
    if (!vec.empty()) {
      OPENSSL_cleanse(vec.data(), vec.size());
      vec.clear();
    }
  }
  
  // Password strength validation
  static bool ValidatePasswordStrength(const std::string& password) {
    if (password.length() < 8) return false;
    
    bool has_upper = false, has_lower = false;
    bool has_digit = false, has_special = false;
    
    for (char c : password) {
      if (std::isupper(c)) has_upper = true;
      else if (std::islower(c)) has_lower = true;
      else if (std::isdigit(c)) has_digit = true;
      else if (!std::isalnum(c)) has_special = true;
    }
    
    return has_upper && has_lower && has_digit;
  }
  
  // Time-constant comparison
  static bool ConstantTimeCompare(const std::string& a,
                                 const std::string& b) {
    if (a.size() != b.size()) return false;
    
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
  }
};

// Secure password input dialog
class PasswordDialog {
 public:
  static std::string RequestPassword(const std::string& prompt,
                                   bool confirm = false) {
    // Platform-specific secure password input
    std::string password = ShowSecurePasswordDialog(prompt);
    
    if (confirm) {
      std::string confirmation = ShowSecurePasswordDialog(
          "Confirm password");
      
      if (!SecurityUtils::ConstantTimeCompare(password, confirmation)) {
        SecurityUtils::SecureZeroMemory(password);
        SecurityUtils::SecureZeroMemory(confirmation);
        throw std::runtime_error("Passwords do not match");
      }
      
      SecurityUtils::SecureZeroMemory(confirmation);
    }
    
    return password;
  }
};
```

### 10. Testing Strategy

```cpp
// Unit tests
TEST(KeyGeneratorTest, GeneratesValidKeys) {
  std::string private_key = KeyGenerator::GeneratePrivateKey();
  EXPECT_EQ(private_key.length(), 64);  // 32 bytes hex
  
  std::string public_key = KeyGenerator::GetPublicKey(private_key);
  EXPECT_EQ(public_key.length(), 64);  // 32 bytes hex
}

TEST(KeyEncryptionTest, EncryptDecryptRoundTrip) {
  std::string private_key = "test_private_key";
  std::string password = "test_password_123";
  
  auto encrypted = KeyEncryption::EncryptKey(private_key, password);
  
  std::string decrypted;
  EXPECT_TRUE(KeyEncryption::DecryptKey(encrypted, password, 
                                       &decrypted));
  EXPECT_EQ(decrypted, private_key);
  
  // Wrong password should fail
  EXPECT_FALSE(KeyEncryption::DecryptKey(encrypted, "wrong_password",
                                        &decrypted));
}

TEST(MnemonicTest, GeneratesValidMnemonic) {
  auto mnemonic = MnemonicGenerator::GenerateMnemonic();
  EXPECT_EQ(mnemonic.size(), 12);  // 128 bits = 12 words
  
  // Verify all words are in wordlist
  for (const auto& word : mnemonic) {
    EXPECT_TRUE(IsValidBIP39Word(word));
  }
}

// Integration tests
TEST(AccountManagerTest, CreateAndUnlockAccount) {
  NostrAccountManager manager;
  
  // Create account
  auto account = manager.CreateAccount("Test Account");
  EXPECT_FALSE(account.id.empty());
  EXPECT_FALSE(account.public_key.empty());
  
  // Unlock with correct password
  EXPECT_TRUE(manager.UnlockAccount(account.id, "test_password"));
  EXPECT_TRUE(manager.IsAccountUnlocked(account.id));
  
  // Get private key
  std::string private_key;
  EXPECT_TRUE(manager.GetPrivateKey(account.id, &private_key));
  EXPECT_FALSE(private_key.empty());
}
```

### 11. Configuration

```cpp
class KeyManagementConfig {
 public:
  struct Config {
    // Security settings
    int unlock_timeout_minutes = 5;
    bool require_password_on_startup = true;
    int min_password_length = 8;
    
    // Backup settings
    bool auto_backup_enabled = false;
    base::FilePath backup_directory;
    int backup_retention_days = 30;
    
    // Hardware wallet
    bool hardware_wallet_support = true;
    std::vector<std::string> supported_devices;
    
    // Advanced
    std::string key_derivation_path = "m/44'/1237'/0'/0/0";
    int argon2_memory_kb = 65536;  // 64MB
    int argon2_iterations = 3;
  };
  
  static Config Load();
  static void Save(const Config& config);
};
```

### 12. Monitoring and Audit

```cpp
class KeyAuditLog {
 public:
  enum EventType {
    ACCOUNT_CREATED,
    ACCOUNT_DELETED,
    ACCOUNT_UNLOCKED,
    ACCOUNT_LOCKED,
    KEY_USED_FOR_SIGNING,
    BACKUP_CREATED,
    BACKUP_RESTORED,
    FAILED_UNLOCK_ATTEMPT,
  };
  
  struct AuditEntry {
    EventType type;
    std::string account_id;
    base::Time timestamp;
    std::string details;
  };
  
  void LogEvent(EventType type, const std::string& account_id,
               const std::string& details = "");
  std::vector<AuditEntry> GetLogs(base::Time since);
  void ClearOldLogs(base::TimeDelta retention);
  
 private:
  void PersistLog(const AuditEntry& entry);
  std::deque<AuditEntry> recent_logs_;
  base::FilePath log_file_;
  base::Lock lock_;
};
```