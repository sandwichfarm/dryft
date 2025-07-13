# Nsite Streaming Server Issues

Following Tungsten's issue creation template for Group D (Protocol Handlers).

---

## Issue Title: D-7: Implement Dynamic Port Allocation for Streaming Server

### Context
- **Group**: D (Protocol Handlers)
- **Dependencies**: D-4 (#24)
- **Milestone**: M4: Protocol Support

### Description
Implement dynamic port allocation for the Nsite streaming server in the ephemeral port range (49152-65535), avoiding common development ports. The server must communicate its port to browser components for proper request routing.

### Acceptance Criteria
- [ ] Server successfully binds to available port in range 49152-65535
- [ ] Avoids common development ports (3000, 8080, 5173, etc.)
- [ ] Falls back to sequential scan if random selection fails
- [ ] Port is accessible via NsiteService singleton
- [ ] Tests written for port allocation logic
- [ ] Documentation updated

### Technical Notes
- Key files to create: `nsite_streaming_server.h/cc`, `nsite_service.h/cc`
- Use `net::ServerSocket` for binding attempts
- Implement retry logic with both random and sequential strategies
- Thread-safe port storage in NsiteService

### References
- Related LLDDs: /memory/LLDD_Nsite_Streaming_Server.md
- Related specs: Dynamic port allocation (Section 10)

---

## Issue Title: D-8: Implement Header-Based Request Routing

### Context
- **Group**: D (Protocol Handlers)
- **Dependencies**: D-7
- **Milestone**: M4: Protocol Support

### Description
Implement request parsing and routing based on X-Nsite-Pubkey header. The server must extract the npub from the header to determine which nsite's files to serve, as different nsites will have conflicting paths like /index.html.

### Acceptance Criteria
- [ ] Parse X-Nsite-Pubkey header from incoming requests
- [ ] Validate npub format
- [ ] Route requests to correct nsite namespace
- [ ] Handle missing/invalid headers gracefully
- [ ] Support case-insensitive header matching
- [ ] Tests for various header scenarios
- [ ] Documentation updated

### Technical Notes
- Implement in `NsiteStreamingServer::ParseNsiteRequest()`
- Return structured RequestContext with npub and path
- Log all requests for debugging
- Consider session-based fallback for missing headers

### References
- Related LLDDs: /memory/LLDD_Nsite_Streaming_Server.md
- Header injection design (Section 4)

---

## Issue Title: D-9: Implement Nsite Cache Manager

### Context
- **Group**: D (Protocol Handlers)
- **Dependencies**: D-8
- **Milestone**: M4: Protocol Support

### Description
Build the caching system for nsite files with LRU eviction, persistence, and efficient lookup. Cache must handle multiple nsites with conflicting paths by namespacing with npub.

### Acceptance Criteria
- [ ] Store files indexed by npub + path
- [ ] Implement LRU eviction at 500MB limit
- [ ] Track cache hits/misses and access times
- [ ] Thread-safe cache operations
- [ ] Persist cache metadata to disk
- [ ] Restore cache on server startup
- [ ] Unit tests for cache operations
- [ ] Documentation updated

### Technical Notes
- Create `nsite_cache_manager.h/cc`
- Use profile directory for persistence
- Implement CachedFile structure with metadata
- Consider memory-mapped files for large content

### References
- Related LLDDs: /memory/LLDD_Nsite_Streaming_Server.md
- Cache implementation (Section 5)

---

## Issue Title: D-10: Implement Browser Header Injection System

### Context
- **Group**: D (Protocol Handlers)
- **Dependencies**: D-7, D-8
- **Milestone**: M4: Protocol Support

### Description
Implement WebRequest interceptor to inject X-Nsite-Pubkey header for all requests to the streaming server. Critical for maintaining nsite context across page navigations.

### Acceptance Criteria
- [ ] Intercept all requests to localhost:<dynamic-port>
- [ ] Inject header based on current tab's nsite context
- [ ] Handle subframes and resources correctly
- [ ] Maintain header across all navigations within same nsite
- [ ] Clear header when navigating to different nsite
- [ ] Integration tests for header persistence
- [ ] Documentation updated

### Technical Notes
- Create `nsite_header_injector.h/cc`
- Use content::WebRequestAPI
- Implement NsiteNavigationContext as WebContentsObserver
- Cache tab-to-npub mappings for performance

### References
- Related LLDDs: /memory/LLDD_Nsite_Streaming_Server.md
- Browser integration (Section 11)

---

## Issue Title: D-11: Update Protocol Handler for Streaming Server

### Context
- **Group**: D (Protocol Handlers)
- **Dependencies**: D-10
- **Milestone**: M4: Protocol Support

### Description
Update the nostr:// protocol handler to redirect nsite URLs to the streaming server with proper context setup. Must query dynamic port and establish navigation context before redirect.

### Acceptance Criteria
- [ ] Resolve nsite identifier to npub (support NIP-05)
- [ ] Query NsiteService for current server port
- [ ] Start server if not running
- [ ] Set up navigation context before redirect
- [ ] Redirect to localhost:<port>/<path>
- [ ] Handle resolution failures gracefully
- [ ] Integration tests for full flow
- [ ] Documentation updated

### Technical Notes
- Modify `nostr_protocol_handler.cc`
- Add async identifier resolution
- Show loading/error states
- Set referrer header as fallback

### References
- Related LLDDs: /memory/LLDD_Nsite_Streaming_Server.md
- Protocol handler updates (Section 12)

---

## Issue Title: D-12: Implement Background Update Monitor

### Context
- **Group**: D (Protocol Handlers)
- **Dependencies**: D-9
- **Milestone**: M4: Protocol Support

### Description
Implement background checking for nsite updates after serving cached content. Must not block request serving and should notify users when updates are available.

### Acceptance Criteria
- [ ] Check for updates after serving from cache
- [ ] Rate limit checks to minimum 5 minutes per nsite
- [ ] Compare event timestamps to detect updates
- [ ] Download changed files in background
- [ ] Don't interrupt active user session
- [ ] Tests for update detection logic
- [ ] Documentation updated

### Technical Notes
- Create `nsite_update_monitor.h/cc`
- Use base::ThreadPool for background work
- Track last check time per nsite
- Implement progressive download

### References
- Related LLDDs: /memory/LLDD_Nsite_Streaming_Server.md
- Update monitoring (Section 7)

---

## Issue Title: D-13: Implement Update Notification Banner

### Context
- **Group**: D (Protocol Handlers)
- **Dependencies**: D-12
- **Milestone**: M4: Protocol Support

### Description
Show non-intrusive banner to users when nsite updates are available, allowing them to reload when ready.

### Acceptance Criteria
- [ ] Inject banner via JavaScript into active tab
- [ ] Include reload and dismiss buttons
- [ ] Auto-hide after 30 seconds
- [ ] Styled to match browser theme
- [ ] Only show for visible tabs
- [ ] E2E tests for notification flow
- [ ] Documentation updated

### Technical Notes
- Use WebContents::ExecuteJavaScript()
- Store dismissed state per nsite
- Position fixed at top of viewport
- Ensure accessibility compliance

### References
- Related LLDDs: /memory/LLDD_Nsite_Streaming_Server.md
- Update notifications (Section 8)

---

## Issue Title: D-14: Implement Security Hardening

### Context
- **Group**: D (Protocol Handlers)
- **Dependencies**: D-7 through D-13
- **Milestone**: M4: Protocol Support

### Description
Security audit and hardening of the streaming server to prevent attacks and ensure safe operation.

### Acceptance Criteria
- [ ] Path traversal prevention implemented
- [ ] Input validation for all parameters
- [ ] Rate limiting for requests
- [ ] Secure session handling
- [ ] No information leakage in errors
- [ ] Security tests written
- [ ] Documentation updated

### Technical Notes
- Implement IsPathSafe() validation
- Sanitize all user inputs
- Use constant-time comparisons where needed
- Structured error responses

### References
- Related LLDDs: /memory/LLDD_Nsite_Streaming_Server.md
- Security considerations (Section 13)

---

## Issue Title: D-15: Performance Optimization and Monitoring

### Context
- **Group**: D (Protocol Handlers)
- **Dependencies**: D-7 through D-13
- **Milestone**: M4: Protocol Support

### Description
Optimize streaming server performance and add monitoring capabilities for debugging and metrics.

### Acceptance Criteria
- [ ] Cache hits under 100ms
- [ ] Server startup under 500ms
- [ ] Memory usage under 100MB for 10 nsites
- [ ] CPU usage near 0% when idle
- [ ] Metrics accessible via chrome://nsite-internals
- [ ] Performance tests written
- [ ] Documentation updated

### Technical Notes
- Profile with Chrome tracing
- Add UMA metrics for monitoring
- Implement chrome://nsite-internals page
- Consider memory mapping for large files

### References
- Related LLDDs: /memory/LLDD_Nsite_Streaming_Server.md
- Performance targets (Section 15)

---

## Implementation Priority

Based on dependencies:
1. D-7 (Dynamic Port) - Foundation
2. D-8 (Request Routing) - Core functionality
3. D-9 (Cache Manager) - Essential for performance
4. D-10 & D-11 (Browser Integration) - Can be done in parallel
5. D-12 & D-13 (Updates) - Enhancement
6. D-14 (Security) - Critical before release
7. D-15 (Performance) - Polish

## Testing Strategy

- Unit tests for each component
- Integration tests for full request flow
- E2E tests for user scenarios
- Security tests for vulnerabilities
- Performance benchmarks against targets