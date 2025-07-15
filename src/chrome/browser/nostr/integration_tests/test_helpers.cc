// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/integration_tests/test_helpers.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "crypto/random.h"
#include "net/base/escape.h"
#include "third_party/boringssl/src/include/openssl/sha.h"
#include "url/gurl.h"

namespace nostr {
namespace test {

namespace {

std::string BytesToHex(const std::vector<uint8_t>& bytes) {
  return base::HexEncode(bytes.data(), bytes.size());
}

std::string GenerateRandomHex(size_t bytes) {
  std::vector<uint8_t> random_bytes(bytes);
  crypto::RandBytes(random_bytes.data(), random_bytes.size());
  return BytesToHex(random_bytes);
}

}  // namespace

std::string CreateTestEvent(int kind,
                           const std::string& content,
                           const std::vector<std::vector<std::string>>& tags) {
  base::Value::Dict event;
  event.Set("id", GenerateRandomHex(32));
  event.Set("pubkey", GenerateRandomHex(32));
  event.Set("created_at", static_cast<int>(base::Time::Now().ToDoubleT()));
  event.Set("kind", kind);
  event.Set("content", content);
  
  base::Value::List tags_list;
  for (const auto& tag : tags) {
    base::Value::List tag_list;
    for (const auto& value : tag) {
      tag_list.Append(value);
    }
    tags_list.Append(std::move(tag_list));
  }
  event.Set("tags", std::move(tags_list));
  event.Set("sig", GenerateRandomHex(64));
  
  std::string json;
  base::JSONWriter::Write(event, &json);
  return json;
}

std::string CreateTestFilter(const std::vector<int>& kinds,
                            const std::vector<std::string>& authors,
                            const std::vector<std::string>& ids,
                            int64_t since,
                            int64_t until,
                            int limit) {
  base::Value::Dict filter;
  
  if (!kinds.empty()) {
    base::Value::List kinds_list;
    for (int kind : kinds) {
      kinds_list.Append(kind);
    }
    filter.Set("kinds", std::move(kinds_list));
  }
  
  if (!authors.empty()) {
    base::Value::List authors_list;
    for (const auto& author : authors) {
      authors_list.Append(author);
    }
    filter.Set("authors", std::move(authors_list));
  }
  
  if (!ids.empty()) {
    base::Value::List ids_list;
    for (const auto& id : ids) {
      ids_list.Append(id);
    }
    filter.Set("ids", std::move(ids_list));
  }
  
  if (since > 0) {
    filter.Set("since", static_cast<int>(since));
  }
  
  if (until > 0) {
    filter.Set("until", static_cast<int>(until));
  }
  
  if (limit > 0) {
    filter.Set("limit", limit);
  }
  
  std::string json;
  base::JSONWriter::Write(filter, &json);
  return json;
}

TestKeyPair GenerateTestKeyPair() {
  TestKeyPair key_pair;
  // Generate 32 bytes for private key
  key_pair.private_key = GenerateRandomHex(32);
  // For testing, just use a different random value for public key
  // In real implementation, this would be derived from private key
  key_pair.public_key = GenerateRandomHex(32);
  return key_pair;
}

std::string SignEvent(const std::string& event_json,
                     const std::string& private_key) {
  // For testing purposes, just return the event with a mock signature
  // Real implementation would compute actual signature
  return event_json;
}

bool VerifyEventSignature(const std::string& event_json) {
  // For testing purposes, always return true
  // Real implementation would verify the signature
  return true;
}

bool WaitForJavaScriptCondition(content::WebContents* web_contents,
                               const std::string& condition,
                               int timeout_ms) {
  const std::string script = base::StringPrintf(
      "new Promise((resolve) => {"
      "  const checkCondition = () => {"
      "    if (%s) {"
      "      resolve(true);"
      "    } else {"
      "      setTimeout(checkCondition, 100);"
      "    }"
      "  };"
      "  checkCondition();"
      "  setTimeout(() => resolve(false), %d);"
      "});",
      condition.c_str(), timeout_ms);
  
  return content::EvalJs(web_contents, script).ExtractBool();
}

bool ExecuteScriptAndExtractValue(content::WebContents* web_contents,
                                 const std::string& script,
                                 base::Value* result) {
  content::EvalJsResult eval_result = content::EvalJs(web_contents, script);
  if (!eval_result.error.empty()) {
    return false;
  }
  *result = eval_result.value.Clone();
  return true;
}

bool CheckNostrAPIAvailable(content::WebContents* web_contents) {
  const std::string script = R"(
    (function() {
      if (typeof window.nostr !== 'object') return false;
      const requiredMethods = [
        'getPublicKey', 'signEvent', 'getRelays',
        'nip04', 'nip44', 'libs', 'relay'
      ];
      return requiredMethods.every(method => 
        method in window.nostr
      );
    })();
  )";
  
  return content::EvalJs(web_contents, script).ExtractBool();
}

bool CheckNostrLibraryAvailable(content::WebContents* web_contents,
                               const std::string& library_name) {
  const std::string script = base::StringPrintf(
      "typeof window.nostr?.libs?.%s === 'string'",
      library_name.c_str());
  
  return content::EvalJs(web_contents, script).ExtractBool();
}

void SimulatePermissionPromptResponse(content::WebContents* web_contents,
                                     bool allow) {
  // This would interact with the permission UI
  // For now, we'll use the test API to set permissions directly
  const std::string script = base::StringPrintf(
      "window.__tungsten_test_permission_response = %s;",
      allow ? "true" : "false");
  
  content::ExecuteScript(web_contents, script);
}

std::string CreateNostrTestHTML(const std::string& body_content) {
  return base::StringPrintf(R"(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Nostr Test Page</title>
    </head>
    <body>
      %s
      <script>
        // Test helper functions
        window.waitForNostr = () => {
          return new Promise((resolve) => {
            if (window.nostr) {
              resolve(true);
            } else {
              const observer = new MutationObserver(() => {
                if (window.nostr) {
                  observer.disconnect();
                  resolve(true);
                }
              });
              observer.observe(window, { attributes: true });
              setTimeout(() => {
                observer.disconnect();
                resolve(false);
              }, 5000);
            }
          });
        };
      </script>
    </body>
    </html>
  )", body_content.c_str());
}

bool WaitForLocalRelayConnection(content::WebContents* web_contents,
                                int timeout_ms) {
  return WaitForJavaScriptCondition(
      web_contents,
      "window.nostr?.relay?.connected === true",
      timeout_ms);
}

bool WaitForBlossomServerReady(content::WebContents* web_contents,
                              int timeout_ms) {
  return WaitForJavaScriptCondition(
      web_contents,
      "window.blossom?.ready === true",
      timeout_ms);
}

PerformanceMetrics MeasureNostrOperation(content::WebContents* web_contents,
                                        const std::string& operation_script) {
  const std::string measurement_script = base::StringPrintf(R"(
    (async function() {
      const startTime = performance.now();
      const startMemory = performance.memory ? performance.memory.usedJSHeapSize : 0;
      
      try {
        await (%s);
      } catch (e) {
        console.error('Operation failed:', e);
      }
      
      const endTime = performance.now();
      const endMemory = performance.memory ? performance.memory.usedJSHeapSize : 0;
      
      return {
        operation_time_ms: endTime - startTime,
        memory_used_bytes: endMemory - startMemory,
        cpu_usage_percent: 0  // Would need native measurement
      };
    })();
  )", operation_script.c_str());
  
  content::EvalJsResult result = content::EvalJs(web_contents, measurement_script);
  
  PerformanceMetrics metrics;
  if (result.value.is_dict()) {
    const base::Value::Dict& dict = result.value.GetDict();
    metrics.operation_time_ms = dict.FindDouble("operation_time_ms").value_or(0);
    metrics.memory_used_bytes = dict.FindDouble("memory_used_bytes").value_or(0);
    metrics.cpu_usage_percent = dict.FindDouble("cpu_usage_percent").value_or(0);
  }
  
  return metrics;
}

std::string CreateTestNsiteContent(const std::string& title,
                                  const std::string& content,
                                  const std::string& theme) {
  base::Value::Dict nsite;
  nsite.Set("kind", 34128);  // Correct Nsite kind
  nsite.Set("title", title);
  nsite.Set("summary", "Test Nsite");
  nsite.Set("content", content);
  nsite.Set("theme", theme);
  
  base::Value::List tags;
  base::Value::List title_tag;
  title_tag.Append("title");
  title_tag.Append(title);
  tags.Append(std::move(title_tag));
  
  base::Value::List theme_tag;
  theme_tag.Append("theme");
  theme_tag.Append(theme);
  tags.Append(std::move(theme_tag));
  
  nsite.Set("tags", std::move(tags));
  
  std::string json;
  base::JSONWriter::Write(nsite, &json);
  return json;
}

GURL CreateNostrURL(const std::string& type,
                   const std::string& identifier) {
  return GURL(base::StringPrintf("nostr:%s:%s", type.c_str(), identifier.c_str()));
}

}  // namespace test
}  // namespace nostr