// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_INTEGRATION_TESTS_TEST_HELPERS_H_
#define CHROME_BROWSER_NOSTR_INTEGRATION_TESTS_TEST_HELPERS_H_

#include <string>
#include <vector>

#include "base/values.h"

class GURL;

namespace content {
class WebContents;
}

namespace nostr {
namespace test {

// Helper functions for Nostr integration tests

// Creates a valid Nostr event JSON with the given parameters
std::string CreateTestEvent(int kind,
                           const std::string& content,
                           const std::vector<std::vector<std::string>>& tags = {});

// Creates a valid Nostr filter JSON
std::string CreateTestFilter(
    const std::vector<int>& kinds = {},
    const std::vector<std::string>& authors = {},
    const std::vector<std::string>& ids = {},
    int64_t since = 0,
    int64_t until = 0,
    int limit = 0);

// Generates a test keypair
struct TestKeyPair {
  std::string private_key;
  std::string public_key;
};
TestKeyPair GenerateTestKeyPair();

// Signs a Nostr event with the given private key
std::string SignEvent(const std::string& event_json,
                     const std::string& private_key);

// Verifies an event signature
bool VerifyEventSignature(const std::string& event_json);

// Helper to wait for a JavaScript condition to become true
bool WaitForJavaScriptCondition(content::WebContents* web_contents,
                               const std::string& condition,
                               int timeout_ms = 5000);

// Helper to execute JavaScript and extract various types
bool ExecuteScriptAndExtractValue(content::WebContents* web_contents,
                                 const std::string& script,
                                 base::Value* result);

// Helper to check if window.nostr is available and has expected methods
bool CheckNostrAPIAvailable(content::WebContents* web_contents);

// Helper to check if a specific Nostr library is available
bool CheckNostrLibraryAvailable(content::WebContents* web_contents,
                               const std::string& library_name);

// Helper to simulate user interaction for permission prompts
void SimulatePermissionPromptResponse(content::WebContents* web_contents,
                                     bool allow);

// Helper to create a test HTML page with Nostr functionality
std::string CreateNostrTestHTML(const std::string& body_content);

// Helper to wait for local relay connection
bool WaitForLocalRelayConnection(content::WebContents* web_contents,
                                int timeout_ms = 10000);

// Helper to wait for Blossom server ready
bool WaitForBlossomServerReady(content::WebContents* web_contents,
                              int timeout_ms = 10000);

// Performance measurement helpers
struct PerformanceMetrics {
  double operation_time_ms;
  double memory_used_bytes;
  double cpu_usage_percent;
};

// Measures the performance of a Nostr operation
PerformanceMetrics MeasureNostrOperation(
    content::WebContents* web_contents,
    const std::string& operation_script);

// Helper to create test Nsite content
std::string CreateTestNsiteContent(const std::string& title,
                                  const std::string& content,
                                  const std::string& theme = "default");

// Helper to create a valid nostr:// URL
GURL CreateNostrURL(const std::string& type,
                   const std::string& identifier);

}  // namespace test
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_INTEGRATION_TESTS_TEST_HELPERS_H_