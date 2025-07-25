# dryft Nostr Integration Tests

This directory contains comprehensive integration tests for Tungsten's Nostr protocol support, including NIP-07 implementation, local relay, Blossom server, protocol handlers, and Nsite functionality.

## Test Structure

### Base Framework
- `nostr_integration_test_base.h/cc` - Base class providing common test utilities
- `test_helpers.h/cc` - Helper functions for creating test data and common operations
- `mock_nostr_service.h/cc` - Mock implementation of NostrService for testing
- `mock_local_relay_service.h/cc` - Mock implementation of LocalRelayService

### Integration Tests
- `nostr_api_integration_test.cc` - Tests for window.nostr JavaScript API
- `nostr_ui_to_backend_integration_test.cc` - Tests for UI to backend communication
- `nostr_ipc_integration_test.cc` - Tests for IPC message handling
- `nostr_protocol_handler_integration_test.cc` - Tests for nostr:// URL handling

### Browser Tests
- `nostr_e2e_browsertest.cc` - End-to-end workflow tests
- `nostr_permission_flow_browsertest.cc` - Permission system tests
- `nostr_key_management_browsertest.cc` - Key creation and management tests
- `nsite_navigation_browsertest.cc` - Nsite rendering and navigation tests
- `blossom_integration_browsertest.cc` - Blossom file storage tests

## Running Tests

### Quick Start
```bash
# Run all Nostr tests
./run_tests.sh

# Run specific test pattern
./run_tests.sh --filter "*KeyManagement*"

# Run only browser tests
./run_tests.sh --browser-only

# Run with verbose output
./run_tests.sh --verbose
```

### Manual Test Execution
```bash
# Run unit tests
out/Release/unit_tests --gtest_filter="Nostr*"

# Run browser tests
out/Release/browser_tests --gtest_filter="Nostr*BrowserTest*"

# Run specific test
out/Release/browser_tests --gtest_filter="NostrE2EBrowserTest.CompleteNostrWorkflow"
```

### Build Requirements
Before running tests, ensure Tungsten is built with Nostr support:
```bash
gn gen out/Release --args="is_official_build=true enable_nostr=true enable_local_relay=true enable_blossom_server=true"
autoninja -C out/Release chrome
```

## Test Categories

### 1. API Tests (`nostr_api_integration_test.cc`)
- window.nostr injection and availability
- NIP-07 method presence and functionality
- Library loading (NDK, nostr-tools, etc.)
- Read-only property enforcement
- Error handling for invalid inputs

### 2. Permission Tests (`nostr_permission_flow_browsertest.cc`)
- Initial permission requests
- Per-origin permission isolation
- Permission persistence across navigations
- Permission revocation
- No-key and locked-key scenarios

### 3. Key Management Tests (`nostr_key_management_browsertest.cc`)
- Multiple key creation and storage
- Key import/export
- Active key switching
- Key locking/unlocking
- Key deletion and rotation

### 4. Protocol Handler Tests (`nostr_protocol_handler_integration_test.cc`)
- nostr:npub navigation
- nostr:nevent handling
- nostr:naddr support
- nostr:nsite loading
- Invalid URL handling

### 5. Nsite Tests (`nsite_navigation_browsertest.cc`)
- Basic Nsite rendering
- Multi-page Nsite navigation
- Theme application
- Resource loading (images, styles)
- Content security policies
- Version updates

### 6. Blossom Tests (`blossom_integration_browsertest.cc`)
- File upload/download
- Image handling
- Authentication (BUD-05)
- File deletion
- Storage limits
- Nsite integration

### 7. End-to-End Tests (`nostr_e2e_browsertest.cc`)
- Complete workflows from key creation to event publishing
- Multi-tab interactions
- Real-time subscriptions
- Performance benchmarks

## Performance Targets

Tests verify these performance requirements:
- NIP-07 operations: < 20ms
- Local relay queries: < 10ms
- Startup overhead: < 50ms
- Memory overhead: < 50MB base
- CPU usage idle: < 0.1%

## Writing New Tests

### 1. Unit/Integration Test
```cpp
TEST_F(NostrIntegrationTestBase, MyNewTest) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("test-key", "password");
  
  // Test logic
  EXPECT_TRUE(/* your assertion */);
}
```

### 2. Browser Test
```cpp
IN_PROC_BROWSER_TEST_F(NostrE2EBrowserTest, MyBrowserTest) {
  // Navigate to test page
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Execute JavaScript
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey().then(pk => "
      "window.domAutomationController.send(pk));",
      &result));
}
```

### 3. Using Test Helpers
```cpp
// Create test event
std::string event = test::CreateTestEvent(1, "content", {{"p", "pubkey"}});

// Wait for condition
bool ready = test::WaitForLocalRelayConnection(web_contents());

// Measure performance
auto metrics = test::MeasureNostrOperation(
    web_contents(), 
    "window.nostr.signEvent({...})"
);
```

## Debugging Tests

### Enable verbose output
```bash
./run_tests.sh --verbose
```

### Run single test with logging
```bash
out/Release/browser_tests \
  --gtest_filter="NostrE2EBrowserTest.CompleteNostrWorkflow" \
  --single-process-tests \
  --enable-logging=stderr \
  --v=1
```

### Use test helpers for debugging
```javascript
// In DevTools console during test
chrome.storage.local.set({'dryft.dev.debug_logging': true});
console.log(window.nostr.relay);
```

## Common Issues

### Tests fail with "window.nostr undefined"
- Ensure `enable_nostr=true` in build args
- Check that test navigates to http/https URL (not chrome://)

### Permission tests fail
- Verify NostrPermissionManager is properly initialized
- Check that test grants permission before API calls

### Local relay connection timeouts
- Ensure `enable_local_relay=true` in build args
- Check that WaitForLocalRelayReady() is called

### Blossom tests fail
- Ensure `enable_blossom_server=true` in build args
- Verify WaitForBlossomServerReady() is called

## Contributing

When adding new tests:
1. Follow existing patterns and naming conventions
2. Add appropriate comments explaining test purpose
3. Include both positive and negative test cases
4. Verify performance requirements are met
5. Update this README if adding new test categories