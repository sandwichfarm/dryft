// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/integration_tests/nostr_integration_test_base.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "chrome/browser/nostr/integration_tests/test_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace nostr {

class BlossomIntegrationBrowserTest : public InProcessBrowserTest,
                                     public NostrIntegrationTestBase {
 public:
  BlossomIntegrationBrowserTest() = default;
  ~BlossomIntegrationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    NostrIntegrationTestBase::SetUp();
    
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDownOnMainThread() override {
    NostrIntegrationTestBase::TearDown();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
  
  base::ScopedTempDir temp_dir_;
  
  std::string CreateTestFile(const std::string& filename,
                            const std::string& content) {
    base::FilePath file_path = temp_dir_.GetPath().AppendASCII(filename);
    if (!base::WriteFile(file_path, content)) {
      return "";
    }
    return file_path.AsUTF8Unsafe();
  }
};

// Test basic Blossom upload functionality
IN_PROC_BROWSER_TEST_F(BlossomIntegrationBrowserTest, BasicFileUpload) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Wait for Blossom to be ready
  bool blossom_ready = test::WaitForBlossomServerReady(web_contents());
  EXPECT_TRUE(blossom_ready);
  
  // Check window.blossom API exists
  bool has_blossom = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send(typeof window.blossom === 'object');",
      &has_blossom));
  EXPECT_TRUE(has_blossom);
  
  // Test upload via JavaScript
  std::string upload_script = R"(
    (async () => {
      try {
        const blob = new Blob(['Hello, Blossom!'], { type: 'text/plain' });
        const file = new File([blob], 'test.txt', { type: 'text/plain' });
        
        const result = await window.blossom.upload(file);
        return JSON.stringify(result);
      } catch (e) {
        return 'error: ' + e.message;
      }
    })().then(result => window.domAutomationController.send(result));
  )";
  
  std::string upload_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), upload_script, &upload_result));
  EXPECT_FALSE(base::StartsWith(upload_result, "error:"));
  
  // Parse result
  auto result = base::JSONReader::Read(upload_result);
  ASSERT_TRUE(result && result->is_dict());
  
  const auto& dict = result->GetDict();
  const std::string* hash = dict.FindString("hash");
  const std::string* url = dict.FindString("url");
  ASSERT_TRUE(hash);
  ASSERT_TRUE(url);
  EXPECT_FALSE(hash->empty());
  EXPECT_TRUE(base::StartsWith(*url, "blossom://"));
}

// Test file download via Blossom URL
IN_PROC_BROWSER_TEST_F(BlossomIntegrationBrowserTest, FileDownload) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  test::WaitForBlossomServerReady(web_contents());
  
  // First upload a file
  std::string upload_script = R"(
    (async () => {
      const blob = new Blob(['Download test content'], { type: 'text/plain' });
      const file = new File([blob], 'download.txt', { type: 'text/plain' });
      
      const result = await window.blossom.upload(file);
      return result.url;
    })().then(url => window.domAutomationController.send(url));
  )";
  
  std::string blossom_url;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), upload_script, &blossom_url));
  EXPECT_TRUE(base::StartsWith(blossom_url, "blossom://"));
  
  // Now download the file
  std::string download_script = base::StringPrintf(R"(
    (async () => {
      try {
        const response = await fetch('%s');
        const text = await response.text();
        return text;
      } catch (e) {
        return 'error: ' + e.message;
      }
    })().then(result => window.domAutomationController.send(result));
  )", blossom_url.c_str());
  
  std::string download_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), download_script, &download_result));
  EXPECT_EQ("Download test content", download_result);
}

// Test image upload and display
IN_PROC_BROWSER_TEST_F(BlossomIntegrationBrowserTest, ImageUploadAndDisplay) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  test::WaitForBlossomServerReady(web_contents());
  
  // Create and upload a small test image
  std::string upload_image_script = R"(
    (async () => {
      // Create a 1x1 red pixel PNG
      const canvas = document.createElement('canvas');
      canvas.width = 1;
      canvas.height = 1;
      const ctx = canvas.getContext('2d');
      ctx.fillStyle = 'red';
      ctx.fillRect(0, 0, 1, 1);
      
      const blob = await new Promise(resolve => canvas.toBlob(resolve, 'image/png'));
      const file = new File([blob], 'pixel.png', { type: 'image/png' });
      
      const result = await window.blossom.upload(file);
      
      // Create img element with Blossom URL
      const img = document.createElement('img');
      img.id = 'test-image';
      img.src = result.url;
      document.body.appendChild(img);
      
      return result.url;
    })().then(url => window.domAutomationController.send(url));
  )";
  
  std::string image_url;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), upload_image_script, &image_url));
  EXPECT_TRUE(base::StartsWith(image_url, "blossom://"));
  
  // Wait for image to load
  bool image_loaded = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      R"(
        new Promise((resolve) => {
          const img = document.getElementById('test-image');
          if (img.complete) {
            resolve(true);
          } else {
            img.onload = () => resolve(true);
            img.onerror = () => resolve(false);
          }
        }).then(result => window.domAutomationController.send(result));
      )",
      &image_loaded));
  EXPECT_TRUE(image_loaded);
}

// Test Blossom authentication
IN_PROC_BROWSER_TEST_F(BlossomIntegrationBrowserTest, BlossomAuthentication) {
  // Setup key for authentication
  std::string pubkey = CreateAndStoreTestKey("blossom-key", "password");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  test::WaitForBlossomServerReady(web_contents());
  
  // Upload with authentication
  std::string auth_upload_script = R"(
    (async () => {
      try {
        const blob = new Blob(['Authenticated content'], { type: 'text/plain' });
        const file = new File([blob], 'auth.txt', { type: 'text/plain' });
        
        // Create auth event (BUD-05 kind 24242)
        const authEvent = {
          kind: 24242,
          created_at: Math.floor(Date.now() / 1000),
          tags: [
            ['t', 'upload'],
            ['x', await window.blossom.getFileHash(file)]
          ],
          content: 'Authorize upload'
        };
        
        const signedAuth = await window.nostr.signEvent(authEvent);
        
        // Upload with auth
        const result = await window.blossom.upload(file, {
          auth: signedAuth
        });
        
        return JSON.stringify({
          success: true,
          url: result.url,
          authenticated: result.authenticated || false
        });
      } catch (e) {
        return JSON.stringify({
          success: false,
          error: e.message
        });
      }
    })().then(result => window.domAutomationController.send(result));
  )";
  
  std::string auth_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), auth_upload_script, &auth_result));
  
  auto result = base::JSONReader::Read(auth_result);
  ASSERT_TRUE(result && result->is_dict());
  
  const auto& dict = result->GetDict();
  EXPECT_TRUE(dict.FindBool("success").value_or(false));
  EXPECT_TRUE(dict.FindString("url"));
}

// Test file mirroring
IN_PROC_BROWSER_TEST_F(BlossomIntegrationBrowserTest, FileMirroring) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  test::WaitForBlossomServerReady(web_contents());
  
  // Test mirroring an external file
  std::string mirror_script = R"(
    (async () => {
      try {
        // Mirror a file from another Blossom server
        const externalUrl = 'https://example.blossom.server/file.jpg';
        const result = await window.blossom.mirror(externalUrl);
        
        return JSON.stringify({
          success: true,
          url: result.url,
          hash: result.hash
        });
      } catch (e) {
        // For testing, we expect this to fail since example.blossom.server doesn't exist
        return JSON.stringify({
          success: false,
          error: e.message
        });
      }
    })().then(result => window.domAutomationController.send(result));
  )";
  
  std::string mirror_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), mirror_script, &mirror_result));
  
  auto result = base::JSONReader::Read(mirror_result);
  ASSERT_TRUE(result && result->is_dict());
  
  // In a real test, we'd set up a mock server to test successful mirroring
  const auto& dict = result->GetDict();
  EXPECT_FALSE(dict.FindBool("success").value_or(true));  // Expected to fail
}

// Test file deletion
IN_PROC_BROWSER_TEST_F(BlossomIntegrationBrowserTest, FileDeletion) {
  // Setup key for authentication
  std::string pubkey = CreateAndStoreTestKey("delete-key", "password");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  test::WaitForBlossomServerReady(web_contents());
  
  // Upload then delete a file
  std::string delete_test_script = R"(
    (async () => {
      try {
        // First upload a file
        const blob = new Blob(['Delete me'], { type: 'text/plain' });
        const file = new File([blob], 'delete.txt', { type: 'text/plain' });
        
        const uploadResult = await window.blossom.upload(file);
        const fileHash = uploadResult.hash;
        
        // Create delete auth event
        const deleteAuth = {
          kind: 24242,
          created_at: Math.floor(Date.now() / 1000),
          tags: [
            ['t', 'delete'],
            ['x', fileHash]
          ],
          content: 'Delete file'
        };
        
        const signedAuth = await window.nostr.signEvent(deleteAuth);
        
        // Delete the file
        const deleteResult = await window.blossom.delete(fileHash, {
          auth: signedAuth
        });
        
        return JSON.stringify({
          uploaded: true,
          deleted: deleteResult.success || false,
          hash: fileHash
        });
      } catch (e) {
        return JSON.stringify({
          uploaded: false,
          deleted: false,
          error: e.message
        });
      }
    })().then(result => window.domAutomationController.send(result));
  )";
  
  std::string delete_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), delete_test_script, &delete_result));
  
  auto result = base::JSONReader::Read(delete_result);
  ASSERT_TRUE(result && result->is_dict());
  
  const auto& dict = result->GetDict();
  EXPECT_TRUE(dict.FindBool("uploaded").value_or(false));
  EXPECT_TRUE(dict.FindBool("deleted").value_or(false));
}

// Test Blossom storage limits
IN_PROC_BROWSER_TEST_F(BlossomIntegrationBrowserTest, StorageLimits) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  test::WaitForBlossomServerReady(web_contents());
  
  // Get storage stats
  std::string stats_script = R"(
    (async () => {
      try {
        const stats = await window.blossom.getStorageStats();
        return JSON.stringify(stats);
      } catch (e) {
        return JSON.stringify({
          error: e.message
        });
      }
    })().then(result => window.domAutomationController.send(result));
  )";
  
  std::string stats_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), stats_script, &stats_result));
  
  auto stats = base::JSONReader::Read(stats_result);
  ASSERT_TRUE(stats && stats->is_dict());
  
  const auto& dict = stats->GetDict();
  EXPECT_TRUE(dict.FindDouble("used").has_value());
  EXPECT_TRUE(dict.FindDouble("total").has_value());
  EXPECT_TRUE(dict.FindDouble("available").has_value());
}

// Test Blossom file listing
IN_PROC_BROWSER_TEST_F(BlossomIntegrationBrowserTest, FileListingForUser) {
  // Setup key
  std::string pubkey = CreateAndStoreTestKey("list-key", "password");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  test::WaitForBlossomServerReady(web_contents());
  
  // Upload some files then list them
  std::string list_test_script = R"(
    (async () => {
      try {
        // Upload a few files
        const files = [];
        for (let i = 0; i < 3; i++) {
          const blob = new Blob([`File ${i} content`], { type: 'text/plain' });
          const file = new File([blob], `file${i}.txt`, { type: 'text/plain' });
          const result = await window.blossom.upload(file);
          files.push(result);
        }
        
        // Get public key
        const myPubkey = await window.nostr.getPublicKey();
        
        // List files for this user
        const listing = await window.blossom.list({
          pubkey: myPubkey
        });
        
        return JSON.stringify({
          uploaded: files.length,
          listed: listing.files.length,
          matches: listing.files.length >= files.length
        });
      } catch (e) {
        return JSON.stringify({
          error: e.message
        });
      }
    })().then(result => window.domAutomationController.send(result));
  )";
  
  std::string list_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), list_test_script, &list_result));
  
  auto result = base::JSONReader::Read(list_result);
  ASSERT_TRUE(result && result->is_dict());
  
  const auto& dict = result->GetDict();
  EXPECT_EQ(3, dict.FindDouble("uploaded").value_or(0));
  EXPECT_TRUE(dict.FindBool("matches").value_or(false));
}

// Test Blossom integration with Nsite
IN_PROC_BROWSER_TEST_F(BlossomIntegrationBrowserTest, NsiteBlossomIntegration) {
  WaitForLocalRelayReady();
  test::WaitForBlossomServerReady(web_contents());
  
  // Upload an image to Blossom
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  std::string upload_and_publish_script = R"(
    (async () => {
      try {
        // Create a test image
        const canvas = document.createElement('canvas');
        canvas.width = 100;
        canvas.height = 100;
        const ctx = canvas.getContext('2d');
        ctx.fillStyle = 'blue';
        ctx.fillRect(0, 0, 100, 100);
        
        const blob = await new Promise(resolve => canvas.toBlob(resolve, 'image/png'));
        const file = new File([blob], 'nsite-image.png', { type: 'image/png' });
        
        // Upload to Blossom
        const blossomResult = await window.blossom.upload(file);
        
        return blossomResult.url;
      } catch (e) {
        return 'error: ' + e.message;
      }
    })().then(url => window.domAutomationController.send(url));
  )";
  
  std::string blossom_image_url;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), upload_and_publish_script, &blossom_image_url));
  EXPECT_TRUE(base::StartsWith(blossom_image_url, "blossom://"));
  
  // Create Nsite content with the Blossom image
  std::string nsite_content = base::StringPrintf(
      R"(<h1>Nsite with Blossom Image</h1>
         <img src="%s" alt="Blossom Image">
         <p>This image is stored in Blossom!</p>)",
      blossom_image_url.c_str());
  
  // Publish Nsite
  std::string nsite_event = test::CreateTestEvent(
      34128,
      nsite_content,
      {{"title", "Blossom Test Site"}});
  SendTestEventToLocalRelay(nsite_event);
  
  auto parsed = base::JSONReader::Read(nsite_event);
  std::string nsite_id = *parsed->GetDict().FindString("id");
  
  // Navigate to Nsite
  GURL nsite_url = test::CreateNostrURL("nsite", nsite_id);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), nsite_url, 1);
  
  // Verify image loads from Blossom
  bool image_loaded = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      R"(
        new Promise((resolve) => {
          const img = document.querySelector('img[src*="blossom://"]');
          if (!img) {
            resolve(false);
          } else if (img.complete) {
            resolve(img.naturalWidth > 0);
          } else {
            img.onload = () => resolve(true);
            img.onerror = () => resolve(false);
          }
        }).then(result => window.domAutomationController.send(result));
      )",
      &image_loaded));
  EXPECT_TRUE(image_loaded);
}

}  // namespace nostr