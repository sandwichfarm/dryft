# Low Level Design Document: NIP-07 Implementation
## dryft browser

### 1. Component Overview

The NIP-07 implementation provides a secure bridge between web applications and Nostr cryptographic operations. It injects the `window.nostr` object into web contexts and handles all signing operations in an isolated process.

### 2. Detailed Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   Web Page Context                       │
│  ┌─────────────────────────────────────────────────┐   │
│  │             window.nostr Object                  │   │
│  │  ┌─────────────┐  ┌──────────────┐             │   │
│  │  │getPublicKey │  │  signEvent   │             │   │
│  │  └─────────────┘  └──────────────┘             │   │
│  │  ┌─────────────┐  ┌──────────────┐             │   │
│  │  │ getRelays   │  │   nip04/44   │             │   │
│  │  └─────────────┘  └──────────────┘             │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
                    Renderer IPC Channel
                            │
┌─────────────────────────────────────────────────────────┐
│                   Browser Process                        │
│  ┌─────────────────────────────────────────────────┐   │
│  │            NIP-07 Request Handler                │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Permission   │  │  Request Router    │       │   │
│  │  │ Checker      │  │                    │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                 Key Service Process                      │
│  ┌─────────────────────────────────────────────────┐   │
│  │           Cryptographic Operations               │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │   Signing    │  │   Encryption      │       │   │
│  │  │   Engine     │  │   Engine          │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### 3. Class Definitions

```typescript
// content/renderer/nostr_injection.ts
class NostrInjection {
  private ipcRenderer: IPCRenderer;
  
  constructor() {
    this.setupWindowObject();
    this.initializeIPC();
  }
  
  private setupWindowObject(): void {
    Object.defineProperty(window, 'nostr', {
      value: this.createNostrObject(),
      writable: false,
      configurable: false
    });
  }
  
  private createNostrObject(): WindowNostr {
    return {
      getPublicKey: () => this.requestPublicKey(),
      signEvent: (event) => this.requestSignature(event),
      getRelays: () => this.requestRelays(),
      nip04: {
        encrypt: (pubkey, plaintext) => this.requestNip04Encrypt(pubkey, plaintext),
        decrypt: (pubkey, ciphertext) => this.requestNip04Decrypt(pubkey, ciphertext)
      },
      nip44: {
        encrypt: (pubkey, plaintext) => this.requestNip44Encrypt(pubkey, plaintext),
        decrypt: (pubkey, ciphertext) => this.requestNip44Decrypt(pubkey, ciphertext)
      }
    };
  }
}

// browser/nip07_handler.cc
class NIP07Handler : public content::WebContentsObserver {
 public:
  explicit NIP07Handler(content::WebContents* web_contents);
  
  void OnIPCMessage(const IPC::Message& message);
  
 private:
  void HandleGetPublicKey(int request_id, const url::Origin& origin);
  void HandleSignEvent(int request_id, const url::Origin& origin, 
                      const NostrEvent& event);
  void HandleGetRelays(int request_id, const url::Origin& origin);
  
  bool CheckPermission(const url::Origin& origin, 
                      const std::string& method);
  void ShowPermissionPrompt(const url::Origin& origin,
                           const std::string& method,
                           base::OnceCallback<void(bool)> callback);
                           
  PermissionManager* permission_manager_;
  KeyService* key_service_;
};

// services/key_service.cc
class KeyService : public service_manager::Service {
 public:
  KeyService();
  
  void GetPublicKey(const std::string& account_id,
                   GetPublicKeyCallback callback);
  void SignEvent(const std::string& account_id,
                const NostrEvent& event,
                SignEventCallback callback);
  void EncryptNIP04(const std::string& account_id,
                   const std::string& pubkey,
                   const std::string& plaintext,
                   EncryptCallback callback);
  void DecryptNIP04(const std::string& account_id,
                   const std::string& pubkey,
                   const std::string& ciphertext,
                   DecryptCallback callback);
                   
 private:
  std::unique_ptr<KeyStorage> key_storage_;
  std::unique_ptr<CryptoEngine> crypto_engine_;
};
```

### 4. Interface Specifications

```typescript
// TypeScript interface for window.nostr
interface WindowNostr {
  getPublicKey(): Promise<string>;
  signEvent(event: UnsignedEvent): Promise<SignedEvent>;
  getRelays(): Promise<{[url: string]: RelayPolicy}>;
  nip04: {
    encrypt(pubkey: string, plaintext: string): Promise<string>;
    decrypt(pubkey: string, ciphertext: string): Promise<string>;
  };
  nip44: {
    encrypt(pubkey: string, plaintext: string): Promise<string>;
    decrypt(pubkey: string, ciphertext: string): Promise<string>;
  };
}

interface UnsignedEvent {
  kind: number;
  created_at: number;
  tags: string[][];
  content: string;
}

interface SignedEvent extends UnsignedEvent {
  id: string;
  pubkey: string;
  sig: string;
}

interface RelayPolicy {
  read: boolean;
  write: boolean;
}
```

### 5. IPC Message Definitions

```cpp
// IPC message types
IPC_MESSAGE_ROUTED2(NostrMsg_GetPublicKey,
                   int /* request_id */,
                   url::Origin /* origin */)

IPC_MESSAGE_ROUTED3(NostrMsg_SignEvent,
                   int /* request_id */,
                   url::Origin /* origin */,
                   NostrEvent /* event */)

IPC_MESSAGE_ROUTED2(NostrMsg_GetRelays,
                   int /* request_id */,
                   url::Origin /* origin */)

// Response messages
IPC_MESSAGE_ROUTED3(NostrMsg_PublicKeyResponse,
                   int /* request_id */,
                   bool /* success */,
                   std::string /* pubkey */)

IPC_MESSAGE_ROUTED3(NostrMsg_SignedEventResponse,
                   int /* request_id */,
                   bool /* success */,
                   NostrEvent /* signed_event */)
```

### 6. Security Implementation

#### 6.1 Permission Model

```cpp
class NostrPermissionManager {
 public:
  enum Permission {
    PERMISSION_GET_PUBLIC_KEY = 1 << 0,
    PERMISSION_SIGN_EVENT = 1 << 1,
    PERMISSION_GET_RELAYS = 1 << 2,
    PERMISSION_ENCRYPT = 1 << 3,
    PERMISSION_DECRYPT = 1 << 4,
  };
  
  struct PermissionEntry {
    url::Origin origin;
    uint32_t granted_permissions;
    base::Time grant_time;
    base::Time expiry_time;
  };
  
  bool HasPermission(const url::Origin& origin, Permission permission);
  void GrantPermission(const url::Origin& origin, 
                      Permission permission,
                      base::TimeDelta duration);
  void RevokePermission(const url::Origin& origin, 
                       Permission permission);
  
 private:
  std::map<url::Origin, PermissionEntry> permissions_;
  base::FilePath storage_path_;
};
```

#### 6.2 Content Security Policy

```cpp
// Injected CSP for pages using window.nostr
const char kNostrCSP[] = 
    "script-src 'self' 'unsafe-inline' 'unsafe-eval'; "
    "connect-src *; "
    "frame-ancestors 'none';";
```

### 7. Event Signing Implementation

```cpp
class NostrCryptoEngine {
 public:
  std::string SignEvent(const std::string& private_key,
                       const NostrEvent& event) {
    // 1. Serialize event for signing
    std::string serialized = SerializeEventForSigning(event);
    
    // 2. Calculate event ID (SHA256)
    std::vector<uint8_t> event_hash = SHA256(serialized);
    std::string event_id = HexEncode(event_hash);
    
    // 3. Sign with Schnorr
    secp256k1_context* ctx = secp256k1_context_create(
        SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    
    secp256k1_keypair keypair;
    secp256k1_keypair_create(ctx, &keypair, 
                            HexDecode(private_key).data());
    
    unsigned char signature[64];
    secp256k1_schnorrsig_sign(ctx, signature, event_hash.data(),
                             &keypair, nullptr);
    
    secp256k1_context_destroy(ctx);
    
    return HexEncode(signature, 64);
  }
  
 private:
  std::string SerializeEventForSigning(const NostrEvent& event) {
    // [0, pubkey, created_at, kind, tags, content]
    rapidjson::Document doc;
    doc.SetArray();
    doc.PushBack(0, doc.GetAllocator());
    doc.PushBack(rapidjson::Value(event.pubkey.c_str(), 
                                  doc.GetAllocator()), 
                doc.GetAllocator());
    doc.PushBack(event.created_at, doc.GetAllocator());
    doc.PushBack(event.kind, doc.GetAllocator());
    
    // Add tags array
    rapidjson::Value tags(rapidjson::kArrayType);
    for (const auto& tag : event.tags) {
      rapidjson::Value tag_array(rapidjson::kArrayType);
      for (const auto& value : tag) {
        tag_array.PushBack(rapidjson::Value(value.c_str(), 
                                           doc.GetAllocator()),
                          doc.GetAllocator());
      }
      tags.PushBack(tag_array, doc.GetAllocator());
    }
    doc.PushBack(tags, doc.GetAllocator());
    
    doc.PushBack(rapidjson::Value(event.content.c_str(), 
                                 doc.GetAllocator()),
                doc.GetAllocator());
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    
    return std::string(buffer.GetString());
  }
};
```

### 8. NIP-04 Encryption Implementation

```cpp
class NIP04Crypto {
 public:
  std::string Encrypt(const std::string& sender_privkey,
                     const std::string& recipient_pubkey,
                     const std::string& plaintext) {
    // 1. Generate shared secret using ECDH
    std::vector<uint8_t> shared_secret = ComputeSharedSecret(
        sender_privkey, recipient_pubkey);
    
    // 2. Generate random IV
    std::vector<uint8_t> iv(16);
    crypto::RandBytes(iv.data(), 16);
    
    // 3. Encrypt with AES-256-CBC
    std::vector<uint8_t> ciphertext = AES256CBC_Encrypt(
        plaintext, shared_secret, iv);
    
    // 4. Format: base64(ciphertext) + "?iv=" + base64(iv)
    return base64::Encode(ciphertext) + "?iv=" + base64::Encode(iv);
  }
  
  std::string Decrypt(const std::string& recipient_privkey,
                     const std::string& sender_pubkey,
                     const std::string& encrypted) {
    // 1. Parse ciphertext and IV
    size_t iv_pos = encrypted.find("?iv=");
    std::string ciphertext_b64 = encrypted.substr(0, iv_pos);
    std::string iv_b64 = encrypted.substr(iv_pos + 4);
    
    std::vector<uint8_t> ciphertext = base64::Decode(ciphertext_b64);
    std::vector<uint8_t> iv = base64::Decode(iv_b64);
    
    // 2. Generate shared secret
    std::vector<uint8_t> shared_secret = ComputeSharedSecret(
        recipient_privkey, sender_pubkey);
    
    // 3. Decrypt
    return AES256CBC_Decrypt(ciphertext, shared_secret, iv);
  }
  
 private:
  std::vector<uint8_t> ComputeSharedSecret(
      const std::string& privkey,
      const std::string& pubkey) {
    secp256k1_context* ctx = secp256k1_context_create(
        SECP256K1_CONTEXT_SIGN);
    
    secp256k1_pubkey pub;
    secp256k1_ec_pubkey_parse(ctx, &pub, 
                             HexDecode(pubkey).data(), 33);
    
    std::vector<uint8_t> shared_secret(32);
    secp256k1_ecdh(ctx, shared_secret.data(), &pub,
                   HexDecode(privkey).data(), nullptr, nullptr);
    
    secp256k1_context_destroy(ctx);
    return shared_secret;
  }
};
```

### 9. State Management

```cpp
class NIP07State {
 public:
  struct PendingRequest {
    int request_id;
    url::Origin origin;
    RequestType type;
    base::Time timestamp;
    base::OnceCallback<void(bool, const std::string&)> callback;
  };
  
  void AddPendingRequest(int request_id, PendingRequest request);
  void ResolvePendingRequest(int request_id, bool success, 
                           const std::string& result);
  void TimeoutPendingRequests();
  
 private:
  std::map<int, PendingRequest> pending_requests_;
  base::RepeatingTimer timeout_timer_;
  static constexpr base::TimeDelta kRequestTimeout = 
      base::TimeDelta::FromSeconds(30);
};
```

### 10. Error Handling

```typescript
// Error types returned to web pages
enum NostrErrorCode {
  USER_REJECTED = 'USER_REJECTED',
  PERMISSION_DENIED = 'PERMISSION_DENIED',
  KEY_NOT_FOUND = 'KEY_NOT_FOUND',
  INVALID_EVENT = 'INVALID_EVENT',
  INTERNAL_ERROR = 'INTERNAL_ERROR',
  TIMEOUT = 'TIMEOUT'
}

class NostrError extends Error {
  code: NostrErrorCode;
  
  constructor(code: NostrErrorCode, message: string) {
    super(message);
    this.code = code;
    this.name = 'NostrError';
  }
}
```

### 11. Testing Strategy

```cpp
// Unit tests for crypto operations
TEST(NostrCryptoTest, SignEventProducesValidSignature) {
  NostrCryptoEngine engine;
  NostrEvent event;
  event.kind = 1;
  event.content = "Hello Nostr";
  event.created_at = base::Time::Now().ToTimeT();
  
  std::string privkey = GeneratePrivateKey();
  std::string pubkey = GetPublicKey(privkey);
  event.pubkey = pubkey;
  
  std::string signature = engine.SignEvent(privkey, event);
  
  EXPECT_TRUE(VerifySignature(pubkey, event, signature));
}

// Integration tests for IPC
TEST(NIP07IPCTest, GetPublicKeyRoundTrip) {
  TestBrowserContext context;
  NIP07Handler handler(context.web_contents());
  
  base::RunLoop run_loop;
  handler.HandleGetPublicKey(1, url::Origin::Create(
      GURL("https://example.com")));
  
  // Verify IPC message sent
  EXPECT_TRUE(context.HasPendingMessage(NostrMsg_GetPublicKey));
  
  run_loop.Run();
}
```

### 12. Performance Considerations

- **Lazy Loading**: Only inject window.nostr when needed
- **Request Batching**: Combine multiple permission prompts
- **Caching**: Cache public keys and relay lists
- **Async Operations**: All crypto operations on background thread

### 13. Browser Integration Points

```cpp
// Modified files in Chromium:
// content/renderer/render_frame_impl.cc
void RenderFrameImpl::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int world_id) {
  // Inject window.nostr for main world
  if (world_id == ISOLATED_WORLD_ID_GLOBAL) {
    NostrInjection::InjectIntoContext(context);
  }
}

// chrome/browser/profiles/profile.cc
void Profile::InitializeServices() {
  // Add NIP-07 handler to profile services
  nip07_handler_ = std::make_unique<NIP07Handler>(this);
}

// chrome/browser/ui/webui/settings/site_settings_handler.cc
void SiteSettingsHandler::HandleGetNostrPermissions(
    const base::ListValue* args) {
  // Add UI for managing Nostr permissions
}
```