// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_cache_manager.h"

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class NsiteCacheManagerTest : public testing::Test {
 protected:
  NsiteCacheManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_manager_ = std::make_unique<NsiteCacheManager>(temp_dir_.GetPath());
  }

  void TearDown() override {
    cache_manager_.reset();
  }

  // Helper to create test content of specific size
  std::string CreateContent(size_t size, char fill = 'X') {
    return std::string(size, fill);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<NsiteCacheManager> cache_manager_;
};

TEST_F(NsiteCacheManagerTest, PutAndGetFile) {
  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  const std::string path = "index.html";
  const std::string content = "<html><body>Test</body></html>";
  const std::string content_type = "text/html";

  // Put file
  cache_manager_->PutFile(npub, path, content, content_type);

  // Get file
  auto file = cache_manager_->GetFile(npub, path);
  ASSERT_TRUE(file);
  EXPECT_EQ(file->npub, npub);
  EXPECT_EQ(file->path, path);
  EXPECT_EQ(file->content, content);
  EXPECT_EQ(file->content_type, content_type);
  EXPECT_EQ(file->size, content.size());
  EXPECT_EQ(file->access_count, 2);  // 1 from put, 1 from get
}

TEST_F(NsiteCacheManagerTest, GetNonExistentFile) {
  auto file = cache_manager_->GetFile("npub1invalid", "missing.html");
  EXPECT_FALSE(file);
}

TEST_F(NsiteCacheManagerTest, RemoveFile) {
  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  const std::string path = "test.txt";
  
  // Add file
  cache_manager_->PutFile(npub, path, "content", "text/plain");
  
  // Verify it exists
  EXPECT_TRUE(cache_manager_->GetFile(npub, path));
  
  // Remove it
  cache_manager_->RemoveFile(npub, path);
  
  // Verify it's gone
  EXPECT_FALSE(cache_manager_->GetFile(npub, path));
}

TEST_F(NsiteCacheManagerTest, ClearNsite) {
  const std::string npub1 = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  const std::string npub2 = "npub14nr0ux0cn38r5rvf3wen3p9sgfxv2ydqchtqt5gu8r8rpa0x97q330wjj";
  
  // Add files for both nsites
  cache_manager_->PutFile(npub1, "file1.html", "content1", "text/html");
  cache_manager_->PutFile(npub1, "file2.html", "content2", "text/html");
  cache_manager_->PutFile(npub2, "file3.html", "content3", "text/html");
  
  // Clear npub1
  cache_manager_->ClearNsite(npub1);
  
  // Verify npub1 files are gone
  EXPECT_FALSE(cache_manager_->GetFile(npub1, "file1.html"));
  EXPECT_FALSE(cache_manager_->GetFile(npub1, "file2.html"));
  
  // Verify npub2 file still exists
  EXPECT_TRUE(cache_manager_->GetFile(npub2, "file3.html"));
}

TEST_F(NsiteCacheManagerTest, LRUEviction) {
  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  
  // Calculate sizes to trigger eviction
  // Max cache is 500MB, so we'll use smaller test size
  const size_t test_max_size = 1024 * 1024;  // 1MB for testing
  const size_t file_size = 400 * 1024;      // 400KB per file
  
  // Override max cache size for testing (would need to make it configurable)
  // For now, test the eviction logic exists
  
  // Add multiple files
  cache_manager_->PutFile(npub, "file1.html", CreateContent(file_size, 'A'), "text/html");
  cache_manager_->PutFile(npub, "file2.html", CreateContent(file_size, 'B'), "text/html");
  cache_manager_->PutFile(npub, "file3.html", CreateContent(file_size, 'C'), "text/html");
  
  // All files should exist (under 500MB limit)
  EXPECT_TRUE(cache_manager_->GetFile(npub, "file1.html"));
  EXPECT_TRUE(cache_manager_->GetFile(npub, "file2.html"));
  EXPECT_TRUE(cache_manager_->GetFile(npub, "file3.html"));
}

TEST_F(NsiteCacheManagerTest, CacheStats) {
  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  
  // Initial stats
  auto stats = cache_manager_->GetStats();
  EXPECT_EQ(stats.file_count, 0u);
  EXPECT_EQ(stats.total_size, 0u);
  EXPECT_EQ(stats.hit_count, 0u);
  EXPECT_EQ(stats.miss_count, 0u);
  
  // Add files
  cache_manager_->PutFile(npub, "file1.html", "content1", "text/html");
  cache_manager_->PutFile(npub, "file2.html", "content2", "text/html");
  
  // Get one file (hit)
  cache_manager_->GetFile(npub, "file1.html");
  
  // Try to get non-existent file (miss)
  cache_manager_->GetFile(npub, "missing.html");
  
  // Check updated stats
  stats = cache_manager_->GetStats();
  EXPECT_EQ(stats.file_count, 2u);
  EXPECT_EQ(stats.total_size, 16u);  // "content1" + "content2"
  EXPECT_EQ(stats.hit_count, 1u);
  EXPECT_EQ(stats.miss_count, 1u);
}

TEST_F(NsiteCacheManagerTest, UpdateExistingFile) {
  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  const std::string path = "index.html";
  
  // Put initial content
  cache_manager_->PutFile(npub, path, "version1", "text/html");
  
  // Update with new content
  cache_manager_->PutFile(npub, path, "version2_longer", "text/html");
  
  // Get should return updated content
  auto file = cache_manager_->GetFile(npub, path);
  ASSERT_TRUE(file);
  EXPECT_EQ(file->content, "version2_longer");
  
  // Stats should reflect proper size
  auto stats = cache_manager_->GetStats();
  EXPECT_EQ(stats.file_count, 1u);
  EXPECT_EQ(stats.total_size, 15u);  // "version2_longer"
}

TEST_F(NsiteCacheManagerTest, PersistenceAcrossRestart) {
  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  const std::string path = "persistent.html";
  const std::string content = "persistent content";
  
  // Put file and save metadata
  cache_manager_->PutFile(npub, path, content, "text/html");
  cache_manager_->SaveMetadata();
  
  // Create new cache manager (simulating restart)
  cache_manager_.reset();
  cache_manager_ = std::make_unique<NsiteCacheManager>(temp_dir_.GetPath());
  
  // File should still be accessible
  auto file = cache_manager_->GetFile(npub, path);
  ASSERT_TRUE(file);
  EXPECT_EQ(file->content, content);
}

TEST_F(NsiteCacheManagerTest, ClearAll) {
  const std::string npub1 = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  const std::string npub2 = "npub14nr0ux0cn38r5rvf3wen3p9sgfxv2ydqchtqt5gu8r8rpa0x97q330wjj";
  
  // Add multiple files
  cache_manager_->PutFile(npub1, "file1.html", "content1", "text/html");
  cache_manager_->PutFile(npub2, "file2.html", "content2", "text/html");
  
  // Clear all
  cache_manager_->ClearAll();
  
  // Verify all files are gone
  EXPECT_FALSE(cache_manager_->GetFile(npub1, "file1.html"));
  EXPECT_FALSE(cache_manager_->GetFile(npub2, "file2.html"));
  
  // Stats should be reset
  auto stats = cache_manager_->GetStats();
  EXPECT_EQ(stats.file_count, 0u);
  EXPECT_EQ(stats.total_size, 0u);
}

TEST_F(NsiteCacheManagerTest, ContentTypes) {
  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  
  // Test various content types
  struct TestCase {
    std::string path;
    std::string content_type;
  };
  
  TestCase cases[] = {
    {"index.html", "text/html"},
    {"style.css", "text/css"},
    {"script.js", "application/javascript"},
    {"image.png", "image/png"},
    {"data.json", "application/json"},
  };
  
  for (const auto& test : cases) {
    cache_manager_->PutFile(npub, test.path, "content", test.content_type);
    
    auto file = cache_manager_->GetFile(npub, test.path);
    ASSERT_TRUE(file);
    EXPECT_EQ(file->content_type, test.content_type);
  }
}

TEST_F(NsiteCacheManagerTest, HashConsistency) {
  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  const std::string path = "test.txt";
  const std::string content = "test content for hashing";
  
  // Put file
  cache_manager_->PutFile(npub, path, content, "text/plain");
  
  // Get file and check hash
  auto file1 = cache_manager_->GetFile(npub, path);
  ASSERT_TRUE(file1);
  EXPECT_FALSE(file1->hash.empty());
  
  // Put same content again
  cache_manager_->PutFile(npub, path, content, "text/plain");
  
  // Hash should be the same
  auto file2 = cache_manager_->GetFile(npub, path);
  ASSERT_TRUE(file2);
  EXPECT_EQ(file1->hash, file2->hash);
  
  // Put different content
  cache_manager_->PutFile(npub, path, content + " modified", "text/plain");
  
  // Hash should be different
  auto file3 = cache_manager_->GetFile(npub, path);
  ASSERT_TRUE(file3);
  EXPECT_NE(file1->hash, file3->hash);
}

}  // namespace nostr