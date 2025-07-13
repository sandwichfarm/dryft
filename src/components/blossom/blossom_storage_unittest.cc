// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blossom/blossom_storage.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blossom {

class BlossomStorageTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    
    StorageConfig config;
    config.root_path = temp_dir_.GetPath();
    config.max_total_size = 10 * 1024 * 1024;  // 10MB
    config.max_blob_size = 1 * 1024 * 1024;    // 1MB
    config.shard_depth = 2;
    config.enable_lru_eviction = true;
    
    storage_ = base::MakeRefCounted<BlossomStorage>(config);
    
    // Initialize storage
    base::RunLoop run_loop;
    storage_->Initialize(base::BindLambdaForTesting([&](bool success) {
      EXPECT_TRUE(success);
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  std::string CalculateHash(const std::string& data) {
    std::string hash = crypto::SHA256HashString(data);
    return base::HexEncode(hash.data(), hash.size());
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<BlossomStorage> storage_;
};

TEST_F(BlossomStorageTest, StoreAndRetrieveContent) {
  std::string content = "Hello, Blossom!";
  std::string hash = CalculateHash(content);
  
  // Store content
  base::RunLoop store_loop;
  storage_->StoreContent(
      hash, content, "text/plain",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_TRUE(success) << error;
        store_loop.Quit();
      }));
  store_loop.Run();
  
  // Retrieve content
  base::RunLoop get_loop;
  storage_->GetContent(
      hash,
      base::BindLambdaForTesting([&](bool success, 
                                   const std::string& data,
                                   const std::string& mime_type) {
        EXPECT_TRUE(success);
        EXPECT_EQ(content, data);
        EXPECT_EQ("text/plain", mime_type);
        get_loop.Quit();
      }));
  get_loop.Run();
}

TEST_F(BlossomStorageTest, StoreWithHashVerification) {
  std::string content = "Test content";
  std::string correct_hash = CalculateHash(content);
  std::string wrong_hash = "0000000000000000000000000000000000000000000000000000000000000000";
  
  // Try to store with wrong hash
  base::RunLoop wrong_loop;
  storage_->StoreContent(
      wrong_hash, content, "",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_FALSE(success);
        EXPECT_EQ("SHA256 hash mismatch", error);
        wrong_loop.Quit();
      }));
  wrong_loop.Run();
  
  // Store with correct hash
  base::RunLoop correct_loop;
  storage_->StoreContent(
      correct_hash, content, "",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_TRUE(success) << error;
        correct_loop.Quit();
      }));
  correct_loop.Run();
}

TEST_F(BlossomStorageTest, InvalidHashFormat) {
  std::string content = "Test";
  
  // Invalid hash length
  base::RunLoop short_loop;
  storage_->StoreContent(
      "abc123", content, "",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_FALSE(success);
        EXPECT_EQ("Invalid SHA256 hash format", error);
        short_loop.Quit();
      }));
  short_loop.Run();
  
  // Invalid characters
  base::RunLoop invalid_loop;
  storage_->StoreContent(
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
      content, "",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_FALSE(success);
        EXPECT_EQ("Invalid SHA256 hash format", error);
        invalid_loop.Quit();
      }));
  invalid_loop.Run();
}

TEST_F(BlossomStorageTest, ExceedMaxBlobSize) {
  // Create content larger than max blob size
  std::string large_content(2 * 1024 * 1024, 'x');  // 2MB
  std::string hash = CalculateHash(large_content);
  
  base::RunLoop run_loop;
  storage_->StoreContent(
      hash, large_content, "",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_FALSE(success);
        EXPECT_EQ("Blob exceeds maximum size", error);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BlossomStorageTest, MimeTypeDetection) {
  // Test with PNG image magic bytes
  std::string png_data = "\x89PNG\r\n\x1a\n";
  std::string hash = CalculateHash(png_data);
  
  base::RunLoop store_loop;
  storage_->StoreContent(
      hash, png_data, "",  // Empty mime type to trigger detection
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_TRUE(success) << error;
        store_loop.Quit();
      }));
  store_loop.Run();
  
  base::RunLoop get_loop;
  storage_->GetContent(
      hash,
      base::BindLambdaForTesting([&](bool success,
                                   const std::string& data,
                                   const std::string& mime_type) {
        EXPECT_TRUE(success);
        EXPECT_EQ("image/png", mime_type);
        get_loop.Quit();
      }));
  get_loop.Run();
}

TEST_F(BlossomStorageTest, HasContent) {
  std::string content = "Check existence";
  std::string hash = CalculateHash(content);
  
  // Check non-existent
  base::RunLoop not_exists_loop;
  storage_->HasContent(
      hash,
      base::BindLambdaForTesting([&](bool exists) {
        EXPECT_FALSE(exists);
        not_exists_loop.Quit();
      }));
  not_exists_loop.Run();
  
  // Store content
  base::RunLoop store_loop;
  storage_->StoreContent(
      hash, content, "",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_TRUE(success) << error;
        store_loop.Quit();
      }));
  store_loop.Run();
  
  // Check exists
  base::RunLoop exists_loop;
  storage_->HasContent(
      hash,
      base::BindLambdaForTesting([&](bool exists) {
        EXPECT_TRUE(exists);
        exists_loop.Quit();
      }));
  exists_loop.Run();
}

TEST_F(BlossomStorageTest, DeleteContent) {
  std::string content = "To be deleted";
  std::string hash = CalculateHash(content);
  
  // Store content
  base::RunLoop store_loop;
  storage_->StoreContent(
      hash, content, "",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_TRUE(success) << error;
        store_loop.Quit();
      }));
  store_loop.Run();
  
  // Delete content
  base::RunLoop delete_loop;
  storage_->DeleteContent(
      hash,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        delete_loop.Quit();
      }));
  delete_loop.Run();
  
  // Verify deleted
  base::RunLoop verify_loop;
  storage_->HasContent(
      hash,
      base::BindLambdaForTesting([&](bool exists) {
        EXPECT_FALSE(exists);
        verify_loop.Quit();
      }));
  verify_loop.Run();
}

TEST_F(BlossomStorageTest, GetMetadata) {
  std::string content = "Metadata test";
  std::string hash = CalculateHash(content);
  
  // Store content
  base::RunLoop store_loop;
  storage_->StoreContent(
      hash, content, "text/plain",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_TRUE(success) << error;
        store_loop.Quit();
      }));
  store_loop.Run();
  
  // Get metadata
  base::RunLoop metadata_loop;
  storage_->GetMetadata(
      hash,
      base::BindLambdaForTesting([&](std::unique_ptr<BlobMetadata> metadata) {
        ASSERT_TRUE(metadata);
        EXPECT_EQ(hash, metadata->hash);
        EXPECT_EQ(static_cast<int64_t>(content.size()), metadata->size);
        EXPECT_EQ("text/plain", metadata->mime_type);
        EXPECT_EQ(0, metadata->access_count);
        metadata_loop.Quit();
      }));
  metadata_loop.Run();
}

TEST_F(BlossomStorageTest, GetStats) {
  // Store multiple blobs
  std::vector<std::pair<std::string, std::string>> test_data = {
      {"First blob", CalculateHash("First blob")},
      {"Second blob", CalculateHash("Second blob")},
      {"Third blob", CalculateHash("Third blob")}
  };
  
  for (const auto& [content, hash] : test_data) {
    base::RunLoop loop;
    storage_->StoreContent(
        hash, content, "",
        base::BindLambdaForTesting([&](bool success, const std::string& error) {
          EXPECT_TRUE(success) << error;
          loop.Quit();
        }));
    loop.Run();
  }
  
  // Get statistics
  base::RunLoop stats_loop;
  storage_->GetStats(
      base::BindLambdaForTesting([&](int64_t total_size, int64_t blob_count) {
        EXPECT_EQ(3, blob_count);
        int64_t expected_size = 0;
        for (const auto& [content, _] : test_data) {
          expected_size += content.size();
        }
        EXPECT_EQ(expected_size, total_size);
        stats_loop.Quit();
      }));
  stats_loop.Run();
}

TEST_F(BlossomStorageTest, VerifyContent) {
  std::string content = "Verify me";
  std::string hash = CalculateHash(content);
  
  // Store content
  base::RunLoop store_loop;
  storage_->StoreContent(
      hash, content, "",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_TRUE(success) << error;
        store_loop.Quit();
      }));
  store_loop.Run();
  
  // Verify content
  base::RunLoop verify_loop;
  storage_->VerifyContent(
      hash,
      base::BindLambdaForTesting([&](bool valid) {
        EXPECT_TRUE(valid);
        verify_loop.Quit();
      }));
  verify_loop.Run();
}

TEST_F(BlossomStorageTest, ShardedPathGeneration) {
  // Test that files are stored in sharded directories
  std::string content = "Sharded storage test";
  std::string hash = CalculateHash(content);
  
  base::RunLoop store_loop;
  storage_->StoreContent(
      hash, content, "",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_TRUE(success) << error;
        store_loop.Quit();
      }));
  store_loop.Run();
  
  // Check file exists in sharded path
  base::FilePath expected_path = temp_dir_.GetPath()
      .Append(hash.substr(0, 2))
      .Append(hash.substr(2, 2))
      .Append(hash);
  
  EXPECT_TRUE(base::PathExists(expected_path));
}

TEST_F(BlossomStorageTest, StoreFromFile) {
  // Create a temporary file
  base::ScopedTempDir source_dir;
  ASSERT_TRUE(source_dir.CreateUniqueTempDir());
  base::FilePath source_file = source_dir.GetPath().Append("test.txt");
  
  std::string content = "File content";
  ASSERT_TRUE(base::WriteFile(source_file, content));
  
  std::string hash = CalculateHash(content);
  
  // Store from file
  base::RunLoop store_loop;
  storage_->StoreContentFromFile(
      hash, source_file, "text/plain",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_TRUE(success) << error;
        store_loop.Quit();
      }));
  store_loop.Run();
  
  // Verify content
  base::RunLoop get_loop;
  storage_->GetContent(
      hash,
      base::BindLambdaForTesting([&](bool success,
                                   const std::string& data,
                                   const std::string& mime_type) {
        EXPECT_TRUE(success);
        EXPECT_EQ(content, data);
        EXPECT_EQ("text/plain", mime_type);
        get_loop.Quit();
      }));
  get_loop.Run();
}

TEST_F(BlossomStorageTest, GetContentPath) {
  std::string content = "Path test";
  std::string hash = CalculateHash(content);
  
  // Get path before storing
  base::RunLoop path_loop1;
  storage_->GetContentPath(
      hash,
      base::BindLambdaForTesting([&](const base::FilePath& path) {
        // Should return valid path even if file doesn't exist yet
        EXPECT_FALSE(path.empty());
        // Check sharding structure (2 levels configured in SetUp)
        EXPECT_EQ(hash.substr(0, 2), path.DirName().BaseName().value());
        EXPECT_EQ(hash.substr(2, 2), path.DirName().DirName().BaseName().value());
        EXPECT_EQ(hash, path.BaseName().value());
        path_loop1.Quit();
      }));
  path_loop1.Run();
  
  // Store content
  base::RunLoop store_loop;
  storage_->StoreContent(
      hash, content, "",
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_TRUE(success) << error;
        store_loop.Quit();
      }));
  store_loop.Run();
  
  // Get path after storing
  base::RunLoop path_loop2;
  storage_->GetContentPath(
      hash,
      base::BindLambdaForTesting([&](const base::FilePath& path) {
        EXPECT_FALSE(path.empty());
        // Verify file exists at returned path
        EXPECT_TRUE(base::PathExists(path));
        // Verify content at path
        std::string file_content;
        EXPECT_TRUE(base::ReadFileToString(path, &file_content));
        EXPECT_EQ(content, file_content);
        path_loop2.Quit();
      }));
  path_loop2.Run();
  
  // Test with invalid hash
  base::RunLoop invalid_loop;
  storage_->GetContentPath(
      "invalid",
      base::BindLambdaForTesting([&](const base::FilePath& path) {
        EXPECT_TRUE(path.empty());
        invalid_loop.Quit();
      }));
  invalid_loop.Run();
}

}  // namespace blossom