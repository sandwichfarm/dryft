# Product Requirements Document (PRD)
## dryft browser - Nostr-Native Web Browser

### 1. Executive Summary

dryft is a fork of the Thorium browser that integrates native Nostr protocol capabilities, enabling decentralized social communication and content storage directly within the browser. This browser will serve as a gateway to the Nostr ecosystem while maintaining full compatibility with the traditional web.

### 2. Product Vision

To create the first truly Nostr-native web browser that seamlessly integrates decentralized social protocols with traditional web browsing, enabling users to interact with both Web 2.0 and Web 3.0 content without friction.

### 3. Target Users

#### Primary Users
- **Nostr Power Users**: Individuals already active in the Nostr ecosystem who want deeper integration
- **Privacy-Conscious Users**: Users seeking decentralized alternatives to traditional web services
- **Developers**: Building Nostr applications and nsites

#### Secondary Users
- **Content Creators**: Looking for censorship-resistant publishing platforms
- **General Web Users**: Interested in exploring decentralized web technologies

### 4. Core Features

#### 4.1 NIP-07 Implementation
- **Built-in Key Management**: Secure storage and management of Nostr private keys
- **Event Signing**: Native support for signing Nostr events without external extensions
- **Permission Management**: Granular control over which sites can access NIP-07 functions
- **Multi-Account Support**: Ability to manage multiple Nostr identities

#### 4.2 Nostr:// Protocol Handler
- **Native URL Scheme**: Direct support for nostr:// URLs
- **Content Resolution**: Automatic resolution of Nostr identifiers (npub, note, nevent, etc.)
- **Fallback Handling**: Graceful degradation when content is unavailable

#### 4.3 Nsite Support
- **Static Website Hosting**: Full support for NIP-1538 Static Website Specification
- **Content Discovery**: Built-in nsite browser and search functionality
- **Offline Caching**: Local storage of frequently accessed nsites
- **Version Control**: Support for nsite versioning and updates

#### 4.4 Local Relay
- **Built-in Relay Server**: Local relay for caching and performance
- **Event Storage**: Persistent storage of relevant Nostr events
- **Sync Capabilities**: Synchronization with external relays
- **Privacy Controls**: Options to limit data sharing with external relays

#### 4.5 Blossom Server Integration
- **Media Caching**: Local Blossom server for media content
- **CDN Functionality**: Serve cached content to improve performance
- **Storage Management**: Configurable cache size and retention policies
- **Content Verification**: Hash-based content integrity checking

### 5. User Stories

#### As a Nostr User
- I want to browse Nostr content natively without external clients
- I want my keys securely stored and easily accessible
- I want to seamlessly switch between traditional websites and nsites

#### As a Developer
- I want to build web applications that leverage NIP-07 without requiring extensions
- I want to deploy nsites that users can access directly
- I want access to local relay APIs for enhanced performance

#### As a Privacy-Conscious User
- I want control over which relays receive my data
- I want local caching to reduce external dependencies
- I want to browse both traditional and Nostr content anonymously

### 6. Success Metrics

- **Adoption Rate**: Number of active users within 6 months
- **Nsite Traffic**: Volume of nostr:// protocol usage
- **Developer Adoption**: Number of applications leveraging native NIP-07
- **Performance**: Page load times compared to traditional browsers
- **Security**: Zero critical security incidents related to key management

### 7. Constraints and Assumptions

#### Technical Constraints
- Must maintain Chromium compatibility for web standards
- Performance overhead must be minimal (<5% vs base Thorium)
- Security model must not compromise browser sandbox

#### Assumptions
- Users are willing to manage their own keys
- Nostr ecosystem will continue to grow
- Nsites will gain adoption as a content distribution method

### 8. Dependencies

- Thorium browser codebase
- Nostr protocol specifications (NIPs)
- nsyte tool for nsite development
- External relay infrastructure
- Blossom protocol specification

### 9. Development Phases

- **Phase 1**: Core NIP-07 implementation
- **Phase 2**: Nostr:// protocol handler and basic nsite support
- **Phase 3**: Local relay and Blossom server
- **Phase 4**: Polish, optimization, and beta release

### 10. Risks and Mitigation

- **Key Security**: Implement hardware security module support
- **Protocol Changes**: Maintain flexibility in implementation
- **Performance Impact**: Extensive profiling and optimization
- **User Adoption**: Strong documentation and onboarding experience