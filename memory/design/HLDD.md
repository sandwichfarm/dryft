# High Level Design Document (HLDD)
## dryft browser - Nostr-Native Web Browser

### 1. System Overview

Tungsten extends the Thorium browser with native Nostr protocol support, creating a seamless bridge between traditional web browsing and decentralized social protocols. The system integrates at multiple levels of the browser architecture to provide transparent access to Nostr content while maintaining security and performance.

### 2. Architectural Principles

#### 2.1 Design Principles
- **Security First**: All cryptographic operations isolated from web content
- **Minimal Overhead**: Nostr features should not impact regular browsing
- **Modular Design**: Each component independently maintainable
- **Progressive Enhancement**: Graceful degradation when features unavailable
- **Privacy by Default**: Local-first architecture with opt-in external connections

#### 2.2 Integration Strategy
- Leverage existing Chromium extension points
- Minimize modifications to core browser code
- Use service-oriented architecture for Nostr components
- Maintain clear boundaries between Nostr and traditional web features

### 3. High-Level Architecture

```
┌────────────────────────────────────────────────────────────────┐
│                          User Interface                         │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐   │
│  │ Address Bar  │  │   Tab UI     │  │ Settings/Prefs    │   │
│  │ (nostr://)   │  │              │  │ (Key Management)  │   │
│  └──────────────┘  └──────────────┘  └───────────────────┘   │
└────────────────────────────────────────────────────────────────┘
                                │
┌────────────────────────────────────────────────────────────────┐
│                      Browser Process                            │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐   │
│  │   Protocol   │  │   Service    │  │    Permission     │   │
│  │   Registry   │  │   Manager    │  │    Manager        │   │
│  └──────────────┘  └──────────────┘  └───────────────────┘   │
└────────────────────────────────────────────────────────────────┘
                                │
┌────────────────────────────────────────────────────────────────┐
│                    Nostr Service Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐   │
│  │ Key Service  │  │ Relay Service│  │  Blossom Service  │   │
│  │              │  │              │  │                   │   │
│  └──────────────┘  └──────────────┘  └───────────────────┘   │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐   │
│  │ NIP-07 Bridge│  │ Nsite Renderer│ │  Protocol Handler │   │
│  │              │  │              │  │                   │   │
│  └──────────────┘  └──────────────┘  └───────────────────┘   │
└────────────────────────────────────────────────────────────────┘
                                │
┌────────────────────────────────────────────────────────────────┐
│                    Renderer Processes                           │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐   │
│  │ Web Content  │  │ Nsite Content│  │  Extension APIs   │   │
│  │ (window.nostr)│ │              │  │                   │   │
│  └──────────────┘  └──────────────┘  └───────────────────┘   │
└────────────────────────────────────────────────────────────────┘
```

### 4. Component Interactions

#### 4.1 Request Flow for nostr:// URLs
```
User Input → URL Parser → Protocol Handler → Content Resolver
                                                    ↓
                                            Relay Service
                                                    ↓
                                            Content Renderer
```

#### 4.2 NIP-07 Request Flow
```
Web Page → window.nostr → Renderer IPC → Browser Process
                                              ↓
                                      Permission Check
                                              ↓
                                        Key Service
                                              ↓
                                      Response via IPC
```

#### 4.3 Local Service Architecture
```
External Relays ←→ Local Relay Service ←→ Browser Components
                          ↓
                    Local Storage
                          ↓
                   Blossom Cache
```

### 5. Major Subsystems

#### 5.1 Security Subsystem
- **Key Isolation**: Separate process for key operations
- **Permission System**: Granular access control
- **Audit Logging**: Track all key usage
- **Secure Storage**: OS-level encryption

#### 5.2 Protocol Subsystem
- **URL Parsing**: Handle all Nostr identifier types
- **Content Resolution**: Fetch from appropriate sources
- **Fallback Logic**: Handle missing content gracefully
- **Caching Layer**: Reduce redundant fetches

#### 5.3 Storage Subsystem
- **Key Storage**: OS keychain integration
- **Event Database**: IndexedDB for local relay
- **Media Cache**: File system for Blossom
- **Preference Storage**: Browser settings

#### 5.4 Network Subsystem
- **WebSocket Management**: Connection pooling
- **HTTP Server**: Local services
- **Relay Communication**: NIP-01 protocol
- **Bandwidth Management**: Rate limiting

### 6. Data Flow

#### 6.1 Key Management Flow
```
User Creates Key → Key Service → OS Keychain
                        ↓
                  Permission DB
                        ↓
                  Available to NIP-07
```

#### 6.2 Event Processing Flow
```
Relay Event → Local Relay → Event Validation
                   ↓              ↓
             Local Storage   Event Router
                              ↓
                        UI Components
```

#### 6.3 Nsite Rendering Flow
```
nostr:// URL → Fetch Metadata → Resolve Files
                    ↓               ↓
              Verify Hashes    Assemble Site
                               ↓
                          Render in Sandbox
```

### 7. Interface Definitions

#### 7.1 Service Interfaces
- **IKeyService**: Key management operations
- **IRelayService**: Relay communication
- **IBlossomService**: Media storage
- **IProtocolHandler**: URL handling

#### 7.2 IPC Messages
- **RequestSignature**: Request event signing
- **GetPublicKey**: Retrieve public key
- **RelayCommand**: Relay operations
- **CacheMedia**: Blossom operations

### 8. Deployment Architecture

#### 8.1 Process Model
```
Main Browser Process
├── Key Service Process (Isolated)
├── Network Service Process
│   ├── Local Relay Thread
│   └── Blossom Server Thread
└── Renderer Processes (Sandboxed)
    └── NIP-07 Injection
```

#### 8.2 Resource Allocation
- **Memory**: Shared between services
- **CPU**: Background priority for services
- **Disk**: Configurable quotas
- **Network**: Bandwidth throttling

### 9. Scalability Considerations

#### 9.1 Performance Targets
- Support 10,000+ cached events
- Handle 100+ concurrent relay connections
- Sub-second nsite loading
- Minimal impact on browser startup

#### 9.2 Optimization Strategies
- Lazy loading of Nostr components
- Event batching for relay communication
- Intelligent cache eviction
- Connection multiplexing

### 10. Error Handling Strategy

#### 10.1 Failure Modes
- **Relay Unavailable**: Use cached data
- **Key Corruption**: Restore from backup
- **Service Crash**: Automatic restart
- **Network Issues**: Offline mode

#### 10.2 Recovery Mechanisms
- Service health monitoring
- Automatic error reporting
- Graceful degradation
- User notification system

### 11. Monitoring and Diagnostics

#### 11.1 Metrics Collection
- Service health status
- Performance counters
- Error rates
- Usage statistics

#### 11.2 Diagnostic Tools
- Debug console for Nostr operations
- Event inspector
- Relay connection monitor
- Performance profiler

### 12. Migration Path

#### 12.1 From Existing Solutions
- Import keys from other clients
- Migrate relay lists
- Transfer cached data
- Preserve user preferences

#### 12.2 Upgrade Strategy
- Backward compatibility
- Data migration tools
- Feature flags for rollout
- Rollback capabilities

### 13. Nsite Streaming Server Architecture

#### 13.1 Server Overview
The Nsite Streaming Server is a persistent local HTTP server that serves static Nostr websites (nsites) from a local cache while maintaining real-time updates from the Nostr network. It dynamically binds to an available port in a safe range (49152-65535) and communicates this port to the browser for seamless access to decentralized websites.

#### 13.2 Architectural Components
```
┌─────────────────────────────────────────────────────────┐
│                Browser Navigation Layer                  │
│  ┌─────────────────────────────────────────────────┐   │
│  │   nostr:// Protocol Handler                      │   │
│  │   ↓                                              │   │
│  │   Redirect to localhost:8081/nsite/<npub>/<path> │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│              Local Streaming Server (Port 8081)          │
│  ┌─────────────────────────────────────────────────┐   │
│  │   HTTP Request Router                            │   │
│  │   - Extract npub from URL path                   │   │
│  │   - Parse requested resource path                │   │
│  │   - Route to appropriate handler                 │   │
│  └─────────────────────────────────────────────────┘   │
│                          │                               │
│  ┌─────────────────────────────────────────────────┐   │
│  │   Cache Manager                                  │   │
│  │   - Check local cache for requested file         │   │
│  │   - Serve immediately if found                   │   │
│  │   - Trigger background update check              │   │
│  └─────────────────────────────────────────────────┘   │
│                          │                               │
│  ┌─────────────────────────────────────────────────┐   │
│  │   Nsite Resolver                                 │   │
│  │   - Fetch kind 34128 events for path            │   │
│  │   - Verify content hashes                        │   │
│  │   - Download from Blossom servers                │   │
│  └─────────────────────────────────────────────────┘   │
│                          │                               │
│  ┌─────────────────────────────────────────────────┐   │
│  │   Update Monitor                                 │   │
│  │   - Check for newer versions in background       │   │
│  │   - Notify browser of available updates          │   │
│  │   - Manage cache invalidation                    │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

#### 13.3 URL Resolution Strategy
Since localhost cannot have subdomains and nsites must be served from root:
- `nostr://nsite/<identifier>/<path>` → `http://localhost:<dynamic-port>/<path>` with `X-Nsite-Pubkey: <npub>` header
- The protocol handler queries the browser service for the current server port
- The protocol handler injects the npub as a custom header
- The server reads the header to determine which nsite to serve
- All paths are served from root as the nsite expects

#### 13.4 Caching and Update Strategy
1. **Immediate Serving**: Cached content is served immediately for instant page loads
2. **Background Updates**: The server checks for updates after serving cached content
3. **Version Detection**: Compares event timestamps and content hashes
4. **User Notification**: Browser displays update banner when new version available
5. **Progressive Updates**: Downloads new files without interrupting current session

#### 13.5 Integration Points
- **Protocol Handler**: Redirects nostr:// URLs to local server
- **Browser IPC**: Communicates update notifications via extension messaging
- **Local Relay**: Queries for kind 34128 events
- **Blossom Client**: Fetches file content from user servers
- **Cache Storage**: Persistent storage for nsite files