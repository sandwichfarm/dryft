// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOSSOM_BLOSSOM_STORAGE_H_
#define COMPONENTS_BLOSSOM_BLOSSOM_STORAGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace blossom {

// Metadata for a stored blob
struct BlobMetadata {
  std::string hash;           // SHA256 hash (64 hex chars)
  int64_t size;               // Size in bytes
  std::string mime_type;      // MIME type (if known)
  base::Time created_at;      // When blob was stored
  base::Time last_accessed;   // Last time blob was accessed
  int access_count;           // Number of times accessed
};

// Configuration for Blossom storage
struct StorageConfig {
  base::FilePath root_path;   // Root directory for storage
  int64_t max_total_size;     // Maximum total storage size in bytes
  int64_t max_blob_size;      // Maximum individual blob size in bytes
  int shard_depth;            // Directory sharding depth (1-4)
  bool enable_lru_eviction;   // Enable LRU eviction when full
};

// Manages content-addressed file storage for Blossom protocol
class BlossomStorage : public base::RefCounted<BlossomStorage> {
 public:
  using StoreCallback = base::OnceCallback<void(bool success, const std::string& error)>;
  using GetCallback = base::OnceCallback<void(bool success, 
                                             const std::string& data,
                                             const std::string& mime_type)>;
  using DeleteCallback = base::OnceCallback<void(bool success)>;
  using MetadataCallback = base::OnceCallback<void(std::unique_ptr<BlobMetadata>)>;
  using StatsCallback = base::OnceCallback<void(int64_t total_size, 
                                               int64_t blob_count)>;

  explicit BlossomStorage(const StorageConfig& config);

  // Initialize storage (create directories, load metadata)
  void Initialize(base::OnceCallback<void(bool success)> callback);

  // Store content with SHA256 verification
  void StoreContent(const std::string& hash,
                   const std::string& data,
                   const std::string& mime_type,
                   StoreCallback callback);

  // Store content from file path (for large files)
  void StoreContentFromFile(const std::string& hash,
                           const base::FilePath& source_path,
                           const std::string& mime_type,
                           StoreCallback callback);

  // Get content by hash
  void GetContent(const std::string& hash, GetCallback callback);

  // Get content file path (for direct serving)
  void GetContentPath(const std::string& hash,
                     base::OnceCallback<void(const base::FilePath&)> callback);

  // Check if content exists
  void HasContent(const std::string& hash,
                 base::OnceCallback<void(bool exists)> callback);

  // Delete content by hash
  void DeleteContent(const std::string& hash, DeleteCallback callback);

  // Get blob metadata
  void GetMetadata(const std::string& hash, MetadataCallback callback);

  // Get storage statistics
  void GetStats(StatsCallback callback);

  // Run storage cleanup (LRU eviction, orphan removal)
  void RunCleanup(base::OnceClosure callback);

  // Verify integrity of stored content
  void VerifyContent(const std::string& hash,
                    base::OnceCallback<void(bool valid)> callback);

 private:
  friend class base::RefCounted<BlossomStorage>;
  ~BlossomStorage();

  // Internal methods run on file task runner
  void InitializeOnFileThread(base::OnceCallback<void(bool)> callback);
  void StoreContentOnFileThread(const std::string& hash,
                               const std::string& data,
                               const std::string& mime_type,
                               StoreCallback callback);
  void GetContentOnFileThread(const std::string& hash, GetCallback callback);
  void DeleteContentOnFileThread(const std::string& hash, DeleteCallback callback);
  
  // Helper methods
  base::FilePath GetShardedPath(const std::string& hash) const;
  std::string CalculateSHA256(const std::string& data) const;
  bool VerifySHA256(const std::string& data, const std::string& expected_hash) const;
  std::string DetectMimeType(const std::string& data) const;
  
  // LRU eviction
  void EvictOldestBlobs(int64_t bytes_needed);
  
  // Update access time for blob
  void UpdateAccessTime(const std::string& hash);
  
  // Configuration
  StorageConfig config_;
  
  // Task runner for file operations
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  
  // Current storage statistics (cached)
  int64_t total_size_;
  int64_t blob_count_;
  
  // Thread checker for main thread
  SEQUENCE_CHECKER(sequence_checker_);
  
  // Weak pointer factory
  base::WeakPtrFactory<BlossomStorage> weak_factory_{this};
};

}  // namespace blossom

#endif  // COMPONENTS_BLOSSOM_BLOSSOM_STORAGE_H_