# Nsite Streaming Server - Milestones and Issues

## Overview
Implementation of a local HTTP streaming server that serves Nsite content with dynamic port allocation, intelligent caching, and seamless browser integration.

## Milestones

### M1: Core Server Infrastructure
**Goal**: Establish the basic HTTP server with dynamic port allocation
**Duration**: 1 week

### M2: Request Routing and Header Processing
**Goal**: Implement header-based nsite identification and request routing
**Duration**: 1 week

### M3: Cache Management System
**Goal**: Build the caching layer for nsite content
**Duration**: 1 week

### M4: Browser Integration
**Goal**: Implement browser-side header injection and navigation context
**Duration**: 2 weeks

### M5: Update Monitoring and Notifications
**Goal**: Add background update checking and user notifications
**Duration**: 1 week

### M6: Testing and Polish
**Goal**: Comprehensive testing, error handling, and performance optimization
**Duration**: 1 week

---

## Issues by Milestone

### M1: Core Server Infrastructure

#### Issue S-1: Dynamic Port Allocation System
**Dependencies**: None
**Priority**: Critical
**Description**: Implement dynamic port allocation in the ephemeral range (49152-65535)

**Acceptance Criteria**:
- [ ] Server tries random ports first, then sequential scan
- [ ] Avoids common development ports (3000, 8080, etc.)
- [ ] Handles bind failures gracefully
- [ ] Persists chosen port for session

**Technical Notes**:
- Use `net::ServerSocket` for binding attempts
- Implement in `NsiteStreamingServer::TryBindToPort()`
- Store avoided ports in static set

---

#### Issue S-2: NsiteService Port Management
**Dependencies**: S-1
**Priority**: Critical
**Description**: Create service to track and communicate server port to browser components

**Acceptance Criteria**:
- [ ] Singleton service accessible throughout browser
- [ ] Thread-safe port getter/setter
- [ ] Observer pattern for port change notifications
- [ ] Auto-start server when first accessed

**Technical Notes**:
- Implement as `KeyedService`
- Use `base::Lock` for thread safety
- Add to `BrowserContextKeyedServiceFactory`

---

#### Issue S-3: Basic HTTP Server Lifecycle
**Dependencies**: S-1, S-2
**Priority**: High
**Description**: Implement server start/stop with proper cleanup

**Acceptance Criteria**:
- [ ] Server starts on browser launch
- [ ] Graceful shutdown on browser exit
- [ ] Restart capability if server crashes
- [ ] Logging for debugging

**Technical Notes**:
- Hook into `BrowserMainParts::PostMainMessageLoopRun()`
- Implement crash detection and auto-restart

---

### M2: Request Routing and Header Processing

#### Issue S-4: Request Context Parser
**Dependencies**: S-3
**Priority**: Critical
**Description**: Parse incoming requests to extract npub from X-Nsite-Pubkey header

**Acceptance Criteria**:
- [ ] Extract npub from X-Nsite-Pubkey header
- [ ] Handle missing/invalid headers gracefully
- [ ] Support case-insensitive header names
- [ ] Validate npub format

**Technical Notes**:
- Implement in `ParseNsiteRequest()`
- Return structured `RequestContext`
- Log all requests for debugging

---

#### Issue S-5: Path-based Request Router
**Dependencies**: S-4
**Priority**: High
**Description**: Route requests to appropriate handlers based on resource path

**Acceptance Criteria**:
- [ ] Serve files from root path as nsites expect
- [ ] Default empty path to /index.html
- [ ] Handle 404s with fallback to /404.html
- [ ] Prevent path traversal attacks

**Technical Notes**:
- Sanitize all paths
- Implement `IsPathSafe()` validation
- Use cache manager for file retrieval

---

#### Issue S-6: Session-based Fallback
**Dependencies**: S-4
**Priority**: Medium
**Description**: Implement session tracking as fallback when headers fail

**Acceptance Criteria**:
- [ ] Map session IDs to npubs
- [ ] Clean up stale sessions
- [ ] Use cookies or URL parameters
- [ ] Seamless fallback from header method

**Technical Notes**:
- Consider using referrer header
- Implement session timeout (30 minutes)
- Store in memory with size limits

---

### M3: Cache Management System

#### Issue S-7: NsiteCacheManager Implementation
**Dependencies**: S-5
**Priority**: Critical
**Description**: Build the core caching system for nsite files

**Acceptance Criteria**:
- [ ] Store files by npub + path
- [ ] LRU eviction when size limit reached
- [ ] Track access times and hit rates
- [ ] Thread-safe operations

**Technical Notes**:
- Default cache size: 500MB
- Use `base::FilePath` for persistence
- Implement `CachedFile` structure

---

#### Issue S-8: Cache Persistence
**Dependencies**: S-7
**Priority**: High
**Description**: Save and restore cache to/from disk

**Acceptance Criteria**:
- [ ] Save cache metadata on shutdown
- [ ] Restore cache on startup
- [ ] Verify file integrity (hashes)
- [ ] Handle corrupted cache gracefully

**Technical Notes**:
- Store in profile directory
- Use protobuf or JSON for metadata
- Background save every 5 minutes

---

#### Issue S-9: Content Type Detection
**Dependencies**: S-7
**Priority**: Medium
**Description**: Detect and set correct MIME types for responses

**Acceptance Criteria**:
- [ ] Detect common web file types
- [ ] Support custom MIME mappings
- [ ] Default to application/octet-stream
- [ ] Handle text encoding properly

**Technical Notes**:
- Use `net::GetMimeTypeFromFile()`
- Special handling for .js as text/javascript
- UTF-8 for text files

---

### M4: Browser Integration

#### Issue S-10: WebRequest Header Injector
**Dependencies**: S-2
**Priority**: Critical
**Description**: Intercept requests to inject X-Nsite-Pubkey header

**Acceptance Criteria**:
- [ ] Intercept all requests to localhost:<port>
- [ ] Inject header based on tab context
- [ ] Handle frames and subresources
- [ ] Minimal performance impact

**Technical Notes**:
- Use `content::WebRequestAPI`
- Register in `OnBeforeSendHeaders`
- Cache tab->npub mappings

---

#### Issue S-11: Navigation Context Tracker
**Dependencies**: S-10
**Priority**: Critical
**Description**: Track current nsite context per tab

**Acceptance Criteria**:
- [ ] Detect navigation to new nsite
- [ ] Maintain context across page loads
- [ ] Clear context when leaving nsite
- [ ] Handle tab switches correctly

**Technical Notes**:
- Implement as `WebContentsObserver`
- Update on `DidStartNavigation`
- Store as `WebContentsUserData`

---

#### Issue S-12: Protocol Handler Updates
**Dependencies**: S-11
**Priority**: High
**Description**: Update nostr:// handler to use streaming server

**Acceptance Criteria**:
- [ ] Resolve nsite identifier to npub
- [ ] Query service for current port
- [ ] Set up navigation context
- [ ] Redirect to localhost URL

**Technical Notes**:
- Handle async NIP-05 resolution
- Show loading state during resolution
- Error page for resolution failures

---

#### Issue S-13: Browser UI Integration
**Dependencies**: S-12
**Priority**: Medium
**Description**: Add UI elements for nsite status

**Acceptance Criteria**:
- [ ] Show current nsite in omnibox
- [ ] Indicate cache status
- [ ] Server status in chrome://nsite-internals
- [ ] Debug information page

**Technical Notes**:
- Extend omnibox with nsite info
- Create new chrome:// page
- Show cache statistics

---

### M5: Update Monitoring and Notifications

#### Issue S-14: Background Update Checker
**Dependencies**: S-7
**Priority**: High
**Description**: Check for content updates without blocking requests

**Acceptance Criteria**:
- [ ] Check after serving cached content
- [ ] Rate limit checks (5 min minimum)
- [ ] Compare event timestamps
- [ ] Download only changed files

**Technical Notes**:
- Use `base::ThreadPool`
- Implement `NsiteUpdateMonitor`
- Track last check per nsite

---

#### Issue S-15: Update Notification Banner
**Dependencies**: S-14
**Priority**: High
**Description**: Show banner when updates are available

**Acceptance Criteria**:
- [ ] Inject banner via JavaScript
- [ ] Non-intrusive placement
- [ ] Reload and dismiss buttons
- [ ] Auto-hide after 30 seconds

**Technical Notes**:
- Use `ExecuteJavaScript()`
- Style to match browser theme
- Store dismissed state

---

#### Issue S-16: Progressive Update Downloads
**Dependencies**: S-14
**Priority**: Medium
**Description**: Download updates without interrupting current session

**Acceptance Criteria**:
- [ ] Download to temporary location
- [ ] Verify hashes before replacing
- [ ] Atomic file replacement
- [ ] Resume interrupted downloads

**Technical Notes**:
- Use `.tmp` extension during download
- Implement resumable downloads
- Clean up orphaned files

---

### M6: Testing and Polish

#### Issue S-17: Integration Test Suite
**Dependencies**: All previous
**Priority**: High
**Description**: Comprehensive integration tests

**Acceptance Criteria**:
- [ ] Test full navigation flow
- [ ] Simulate multiple concurrent nsites
- [ ] Test cache eviction
- [ ] Performance benchmarks

**Technical Notes**:
- Use `content::BrowserTest`
- Mock relay responses
- Measure request latencies

---

#### Issue S-18: Error Handling and Recovery
**Dependencies**: All previous
**Priority**: High
**Description**: Robust error handling throughout system

**Acceptance Criteria**:
- [ ] Graceful handling of all failures
- [ ] User-friendly error pages
- [ ] Automatic recovery attempts
- [ ] Detailed error logging

**Technical Notes**:
- Custom error pages per error type
- Exponential backoff for retries
- Structured logging

---

#### Issue S-19: Performance Optimization
**Dependencies**: S-17
**Priority**: Medium
**Description**: Optimize for speed and resource usage

**Acceptance Criteria**:
- [ ] Sub-100ms cache hits
- [ ] Efficient memory usage
- [ ] Minimal CPU when idle
- [ ] Fast server startup

**Technical Notes**:
- Profile with Chrome tracing
- Optimize hot paths
- Consider memory mapping for large files

---

#### Issue S-20: Security Hardening
**Dependencies**: All previous
**Priority**: High
**Description**: Security review and hardening

**Acceptance Criteria**:
- [ ] Prevent path traversal
- [ ] Validate all inputs
- [ ] Rate limiting
- [ ] Secure session handling

**Technical Notes**:
- Security audit checklist
- Fuzzing for edge cases
- Penetration testing

---

## Implementation Order

1. **Week 1**: S-1, S-2, S-3 (Core Infrastructure)
2. **Week 2**: S-4, S-5, S-6 (Request Routing)
3. **Week 3**: S-7, S-8, S-9 (Cache System)
4. **Week 4**: S-10, S-11, S-12 (Browser Integration Part 1)
5. **Week 5**: S-13, S-14, S-15 (Browser Integration Part 2 & Updates)
6. **Week 6**: S-16, S-17 (Updates & Testing)
7. **Week 7**: S-18, S-19, S-20 (Polish & Security)

## Success Metrics

- Server starts in < 500ms
- Cache hit rate > 90% for repeat visits
- Page load time < 200ms for cached content
- Memory usage < 100MB for 10 active nsites
- Zero security vulnerabilities
- 99.9% server uptime during browser session