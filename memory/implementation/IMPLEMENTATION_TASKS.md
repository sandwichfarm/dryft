# Tungsten Browser Implementation Tasks

## Milestone 1: Foundation

### Group A: Core Browser Integration

#### A-1: Setup Tungsten Fork Structure
**Dependencies**: None
**Description**: Create initial fork structure from Thorium, setup build configuration
- Fork Thorium repository
- Add Tungsten-specific directories
- Create BUILD.gn files for Nostr components
- Setup development environment

#### A-2: Implement Chrome Resource Handler
**Dependencies**: A-1
**Description**: Create resource handler for serving bundled libraries
- Implement NostrResourceHandler class
- Register chrome://resources/js/nostr/* URLs
- Add CORS headers for cross-origin access
- Test resource loading

#### A-3: Create IPC Message Definitions
**Dependencies**: A-1
**Description**: Define all IPC messages for Nostr communication
- Create nostr_messages.h
- Define message types for NIP-07 operations
- Define messages for local relay communication
- Define messages for Blossom operations

#### A-4: Integrate with Chromium Build System
**Dependencies**: A-1
**Description**: Modify Chromium build files to include Tungsten
- Update chrome/browser/BUILD.gn
- Update content/renderer/BUILD.gn
- Add Tungsten feature flags
- Create resource bundle for libraries

## Milestone 2: Nostr Core

### Group B: Nostr Protocol Implementation

#### B-1: Implement Basic NIP-07 Injection
**Dependencies**: A-3
**Description**: Create basic window.nostr object injection
- Create NostrBindings class
- Implement script injection in RenderFrameImpl
- Add basic getPublicKey stub
- Test injection on page load

#### B-2: Implement Key Storage Interface
**Dependencies**: A-2
**Description**: Create abstract key storage interface
- Design KeyStorage abstract class
- Define methods for store/retrieve/delete
- Create factory for platform-specific implementations
- Add encryption layer

#### B-3: Platform-Specific Key Storage (Windows)
**Dependencies**: B-2
**Description**: Implement Windows Credential Manager integration
- Create WindowsKeyStorage class
- Implement Credential Manager API calls
- Add error handling
- Write platform tests

#### B-4: Platform-Specific Key Storage (macOS)
**Dependencies**: B-2
**Description**: Implement macOS Keychain integration
- Create MacKeyStorage class
- Implement Keychain Services API calls
- Handle keychain access permissions
- Write platform tests

#### B-5: Platform-Specific Key Storage (Linux)
**Dependencies**: B-2
**Description**: Implement Linux Secret Service integration
- Create LinuxKeyStorage class
- Implement Secret Service D-Bus calls
- Add fallback for systems without Secret Service
- Write platform tests

#### B-6: Implement Permission System
**Dependencies**: B-1, A-3
**Description**: Create permission management for NIP-07
- Create PermissionManager class
- Implement per-origin permission storage (LMDB)
- Add permission checking logic
- Create permission request IPC

#### B-7: Complete NIP-07 Implementation
**Dependencies**: B-6, B-2
**Description**: Implement all NIP-07 methods
- Implement getPublicKey with permissions
- Implement signEvent with validation
- Implement getRelays
- Implement NIP-04 encryption methods

#### B-8: Multi-Account Support
**Dependencies**: B-7
**Description**: Add support for multiple Nostr accounts
- Extend key storage for multiple accounts
- Add account switching logic
- Update UI bindings
- Test account isolation

## Milestone 3: Local Services

### Group C: Local Services Implementation

#### C-1: Local Relay Database Schema
**Dependencies**: A-1
**Description**: Create SQLite schema for local relay
- Design events table with indexes
- Design tags table for efficient queries
- Create migration system
- Write schema tests

#### C-2: Local Relay WebSocket Server
**Dependencies**: C-1
**Description**: Implement WebSocket server for local relay
- Create LocalRelayService class
- Implement WebSocket protocol
- Add connection management
- Handle subscriptions

#### C-3: Local Relay Event Storage
**Dependencies**: C-2
**Description**: Implement event storage and retrieval
- Implement EVENT command handling
- Implement REQ subscription handling
- Add filter matching logic
- Optimize query performance

#### C-4: Local Relay Configuration
**Dependencies**: C-3
**Description**: Add configuration options for local relay
- Add preferences for port, interface
- Implement storage limits
- Add retention policies
- Create access control

#### C-5: Blossom File Storage
**Dependencies**: A-1
**Description**: Implement file system storage for Blossom
- Create BlossomStorage class
- Implement sharded directory structure
- Add hash verification
- Implement size limits

#### C-6: Blossom HTTP Server
**Dependencies**: C-5
**Description**: Implement HTTP server for Blossom
- Create BlossomService class
- Implement BUD-01 endpoints (upload/download)
- Add CORS handling
- Implement authorization

#### C-7: Blossom Authorization (BUD-01)
**Dependencies**: C-6, B-7
**Description**: Implement kind 24242 authorization
- Parse authorization events
- Validate signatures
- Check expiration
- Implement verb checking

#### C-8: Blossom User Servers (BUD-03)
**Dependencies**: C-7
**Description**: Implement user server list support
- Fetch kind 10063 events
- Parse server lists
- Implement server discovery
- Add mirroring support

## Milestone 4: Protocol Support

### Group D: Protocol Handlers

#### D-1: Register nostr:// Protocol
**Dependencies**: A-1
**Description**: Register nostr:// URL scheme handler
- Register protocol in ChromeContentBrowserClient
- Create NostrProtocolHandler class
- Add URL parsing logic
- Test protocol registration

#### D-2: Implement Bech32 Decoding
**Dependencies**: D-1
**Description**: Add Bech32 decoding for nostr:// URLs
- Import/implement Bech32 decoder
- Handle npub, note, nprofile, nevent
- Add TLV parsing for extended formats
- Write decoding tests

#### D-3: Implement Profile/Event Resolution
**Dependencies**: D-2, C-3
**Description**: Resolve nostr:// URLs to content
- Query relays for profiles/events
- Handle relay hints from TLV
- Implement timeout handling
- Cache resolved content

#### D-4: Implement Nsite Resolution
**Dependencies**: D-1
**Description**: Handle nostr://nsite URLs
- Parse Nsite identifiers
- Resolve pubkeys from domains
- Implement NIP-05 resolution
- Add DNS TXT record support

#### D-5: Implement Nsite File Loading
**Dependencies**: D-4, C-3
**Description**: Load Nsite static files
- Query for kind 34128 events
- Match paths using 'd' tag
- Verify file hashes ('x' tag)
- Implement directory index handling

#### D-6: Implement Nsite Rendering
**Dependencies**: D-5
**Description**: Render Nsite content securely
- Create sandboxed renderer
- Apply strict CSP (no unsafe-inline)
- Implement custom URL loader
- Block external connections

## Milestone 5: Enhanced APIs

### Group E: Browser APIs

#### E-1: Extend window.nostr with relay property
**Dependencies**: C-3, B-7
**Description**: Add local relay access to window.nostr
- Add relay.url getter
- Add relay.connected property
- Implement relay.query method
- Add relay statistics

#### E-2: Bundle Nostr Libraries
**Dependencies**: A-4
**Description**: Bundle popular Nostr libraries
- Add NDK, nostr-tools, applesauce
- Create build scripts for bundling
- Optimize library sizes
- Generate source maps

#### E-3: Implement window.nostr.libs
**Dependencies**: E-2, A-2
**Description**: Expose library URLs via window.nostr.libs
- Return chrome:// URLs for each library
- Ensure importable via dynamic import
- Add version information
- Test library loading

#### E-4: Implement window.blossom API
**Dependencies**: C-6, B-7
**Description**: Create complete Blossom API
- Implement upload methods
- Implement download methods
- Add server management
- Add mirroring support

#### E-5: Implement Account Management API
**Dependencies**: B-8
**Description**: Add account management to window.nostr
- Implement accounts.list()
- Implement accounts.switch()
- Add account creation API
- Add import/export methods

## Milestone 6: User Interface

### Group F: User Interface

#### F-1: Create Nostr Settings Page
**Dependencies**: B-1
**Description**: Build main Nostr settings UI
- Create settings page structure
- Add navigation sections
- Implement React components
- Connect to preference system

#### F-2: Account Management UI
**Dependencies**: F-1, B-8
**Description**: Build account management interface
- Account list component
- Account creation flow
- Import/export dialogs
- Account switching UI

#### F-3: Permission Management UI
**Dependencies**: F-1, B-6
**Description**: Build permission management interface
- Per-site permission list
- Permission editing dialogs
- Kind-specific permissions
- Rate limit settings

#### F-4: Local Relay Settings UI
**Dependencies**: F-1, C-4
**Description**: Build local relay configuration
- Port and interface settings
- Storage limit controls
- Retention policy settings
- Access control options

#### F-5: Blossom Settings UI
**Dependencies**: F-1, C-8
**Description**: Build Blossom configuration interface
- Server list management
- Storage quota settings
- MIME type restrictions
- Mirroring configuration

#### F-6: NIP-07 Permission Dialogs
**Dependencies**: B-6
**Description**: Create permission request dialogs
- Design permission dialog
- Add remember checkbox
- Show request details
- Implement timeout

#### F-7: Status Indicators
**Dependencies**: C-3, C-6
**Description**: Add status indicators to browser UI
- Create Nostr status button
- Show connection status
- Add notification badges
- Create status menu

## Milestone 7: Polish

### Group G: Platform Integration & Testing

#### G-1: Platform Installer Integration
**Dependencies**: All previous
**Description**: Update platform installers
- Update Windows NSIS installer
- Update macOS DMG creation
- Update Linux package scripts
- Add file associations

#### G-2: Migration from Extensions
**Dependencies**: B-2
**Description**: Import data from Nostr extensions
- Detect Alby/nos2x extensions
- Import keys and settings
- Migrate relay lists
- Preserve permissions

#### G-3: Performance Testing Suite
**Dependencies**: All previous
**Description**: Create performance benchmarks
- Startup time benchmarks
- Memory usage tests
- Event processing benchmarks
- Library loading tests

#### G-4: Integration Test Suite
**Dependencies**: All previous
**Description**: Create comprehensive integration tests
- Cross-component tests
- IPC communication tests
- Protocol handler tests
- API functionality tests

#### G-5: Documentation
**Dependencies**: All previous
**Description**: Write user and developer documentation
- User guide for Nostr features
- Developer API documentation
- Configuration guide
- Troubleshooting guide

#### G-6: Security Audit Fixes
**Dependencies**: G-3, G-4
**Description**: Address security findings
- Fix any CSP issues
- Implement missing rate limits
- Add input validation
- Enhance key protection

#### G-7: Telemetry Implementation
**Dependencies**: G-4
**Description**: Add privacy-preserving telemetry
- Implement opt-in telemetry
- Add differential privacy
- Create analytics dashboard
- Test data collection

#### G-8: Release Preparation
**Dependencies**: All previous
**Description**: Prepare for initial release
- Create release builds
- Test auto-update system
- Prepare release notes
- Create distribution packages

## Task Execution Order

1. Complete all Milestone 1 tasks first (Foundation)
2. Then Milestone 2 (Nostr Core) - these are prerequisites
3. Milestone 3 & 4 can be worked on in parallel
4. Milestone 5 depends on 2, 3, and 4
5. Milestone 6 can start after Milestone 2
6. Milestone 7 requires all previous milestones

## Issue Linking Strategy

Each issue should include:
- **Milestone**: Which milestone it belongs to
- **Blocks**: List of issue numbers this blocks
- **Blocked by**: List of issue numbers blocking this
- **Related specs**: Links to relevant .md files in /memory/ (now organized in memory/design/, memory/specifications/, memory/reference/)

This ensures clear dependency tracking and allows working on multiple non-blocking issues in parallel.