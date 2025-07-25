# Technical Specifications Document
## dryft browser - Nostr-Native Web Browser

### 1. System Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     dryft browser                         │
├─────────────────────────────────────────────────────────────┤
│                    Chromium Core (Thorium)                   │
├─────────────────────────────────────────────────────────────┤
│                    Nostr Integration Layer                   │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │   NIP-07    │  │ Protocol     │  │  Local Services  │  │
│  │   Provider  │  │ Handler      │  │  ┌─────────────┐ │  │
│  │             │  │ (nostr://)   │  │  │ Local Relay │ │  │
│  └─────────────┘  └──────────────┘  │  └─────────────┘ │  │
│  ┌─────────────┐  ┌──────────────┐  │  ┌─────────────┐ │  │
│  │ Key Manager │  │ Nsite Engine │  │  │   Blossom   │ │  │
│  │             │  │              │  │  │   Server    │ │  │
│  └─────────────┘  └──────────────┘  │  └─────────────┘ │  │
│                                      └──────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                   Security & Storage Layer                   │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │  Keychain   │  │  IndexedDB   │  │   File System    │  │
│  │  API        │  │  Storage     │  │   API            │  │
│  └─────────────┘  └──────────────┘  └──────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 2. Core Components

#### 2.1 NIP-07 Provider Module

**Purpose**: Implement window.nostr interface for web applications

**Technical Requirements**:
- Inject window.nostr object into all web contexts
- Implement all NIP-07 specified methods
- Secure IPC between renderer and main process
- Permission system for method access

**API Specification**:
```typescript
interface Window {
  nostr: {
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
}
```

#### 2.2 Key Management System

**Purpose**: Secure storage and management of Nostr private keys

**Technical Requirements**:
- OS-level keychain integration (Keychain on macOS, Credential Manager on Windows, Secret Service on Linux)
- Hardware security module support
- Key derivation from seed phrases (BIP-39)
- Multi-account management

**Security Model**:
- Keys never exposed to renderer process
- All signing operations in isolated process
- Encrypted at rest using OS facilities
- Optional passphrase protection

#### 2.3 Protocol Handler (nostr://)

**Purpose**: Handle nostr:// URLs natively

**Technical Requirements**:
- Register nostr:// as browser protocol
- Parse all Nostr identifier types (npub, note, nevent, naddr, etc.)
- Resolve identifiers to content
- Render appropriate UI for each content type

**URL Format Support**:
```
nostr:npub1... → User profile
nostr:note1... → Single note
nostr:nevent1... → Event with relay hints
nostr:naddr1... → Parameterized replaceable event
nostr:nprofile1... → Profile with relay hints
nostr:nsite1... → Nsite reference
```

#### 2.4 Nsite Engine

**Purpose**: Render static websites from Nostr events

**Technical Requirements**:
- Parse NIP-1538 compliant events
- Resolve file references
- Handle routing for multi-page sites
- Cache rendered content
- Support for CSS, JavaScript, and media files

**Implementation Details**:
- Use nsyte library for parsing
- Sandboxed iframe rendering
- Content Security Policy enforcement
- CORS handling for Nostr resources

#### 2.5 Local Relay Service

**Purpose**: Provide local relay functionality for performance and privacy

**Technical Requirements**:
- WebSocket server on localhost
- Full NIP-01 relay implementation
- Event storage using IndexedDB
- Configurable retention policies
- Sync with external relays

**Database Schema**:
```sql
Events Table:
- id: TEXT PRIMARY KEY
- pubkey: TEXT
- created_at: INTEGER
- kind: INTEGER
- tags: JSON
- content: TEXT
- sig: TEXT

Indexes:
- pubkey_created_at
- kind_created_at
- tags_index (JSON index)
```

#### 2.6 Blossom Server

**Purpose**: Local media caching and serving

**Technical Requirements**:
- HTTP server for media content
- Content-addressed storage
- Configurable cache size
- LRU eviction policy
- Hash verification

**API Endpoints**:
```
GET /[sha256] - Retrieve content
PUT /upload - Upload content
HEAD /[sha256] - Check existence
GET /list - List cached content
```

### 3. Integration Points

#### 3.1 Chromium Integration

**Renderer Process Modifications**:
- Custom V8 extensions for window.nostr
- Protocol handler registration
- Custom scheme implementation

**Browser Process Modifications**:
- Service manager for local servers
- Key storage service
- Permission management UI

#### 3.2 External Dependencies

**Required Libraries**:
- nostr-tools (TypeScript Nostr library)
- nsyte (Nsite parsing and rendering)
- noble-secp256k1 (Cryptography)
- better-sqlite3 (Local relay storage)

### 4. Performance Specifications

- **Key Operations**: <50ms for signing
- **Local Relay Query**: <10ms for indexed queries
- **Nsite Load Time**: <200ms for cached sites
- **Memory Overhead**: <100MB for Nostr services
- **Blossom Cache**: Configurable, default 1GB

### 5. Security Specifications

#### 5.1 Threat Model

- **Key Extraction**: Prevented by OS keychain
- **XSS Attacks**: Isolated renderer processes
- **Relay Manipulation**: Event signature verification
- **DNS Hijacking**: Not applicable to Nostr content

#### 5.2 Security Measures

- Content Security Policy for nsites
- Permission prompts for NIP-07 access
- Sandboxed execution environments
- Regular security audits

### 6. Data Specifications

#### 6.1 Storage Requirements

- **Keys**: ~1KB per account
- **Local Relay**: ~1KB per event
- **Blossom Cache**: Variable, user-configurable
- **Nsite Cache**: ~100KB per site average

#### 6.2 Data Formats

- **Events**: JSON (NIP-01 format)
- **Keys**: Hex-encoded strings
- **Media**: Binary blobs with SHA-256 identifiers

### 7. Network Specifications

#### 7.1 WebSocket Connections

- Multiplexed connections to relays
- Automatic reconnection logic
- Exponential backoff
- Connection pooling

#### 7.2 HTTP Requests

- Blossom server on localhost:8080
- Local relay WebSocket on localhost:8081
- CORS headers for cross-origin access

### 8. Platform-Specific Considerations

#### 8.1 macOS
- Keychain Services API for key storage
- App Transport Security exceptions

#### 8.2 Windows
- Windows Credential Manager integration
- Windows Defender exceptions for local servers

#### 8.3 Linux
- Secret Service API (GNOME Keyring/KWallet)
- AppArmor/SELinux policies

### 9. Testing Specifications

#### 9.1 Unit Tests
- Cryptographic operations
- Protocol parsing
- Storage operations

#### 9.2 Integration Tests
- NIP-07 injection
- Protocol handler routing
- Relay synchronization

#### 9.3 End-to-End Tests
- Full user flows
- Cross-platform testing
- Performance benchmarks

### 10. Deployment Specifications

#### 10.1 Build Process
- Extend Thorium build system
- Additional build flags for Nostr features
- Separate debug/release configurations

#### 10.2 Update Mechanism
- Preserve user keys during updates
- Migrate local relay database
- Clear outdated caches