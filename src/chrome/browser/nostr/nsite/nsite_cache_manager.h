// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NSITE_NSITE_CACHE_MANAGER_H_
#define CHROME_BROWSER_NOSTR_NSITE_NSITE_CACHE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"

namespace nostr {

// Manages cached nsite files with LRU eviction and persistence
class NsiteCacheManager {
 public:
  static constexpr size_t kMaxCacheSize = 500 * 1024 * 1024;  // 500MB
  
  // Information about a cached file
  struct CachedFile {
    std::string npub;
    std::string path;
    std::string content;
    std::string content_type;
    std::string hash;  // SHA256 of content
    size_t size = 0;
    base::Time created_at;
    base::Time last_accessed;
    int access_count = 0;
  };

  // Cache statistics
  struct CacheStats {
    size_t total_size = 0;
    size_t file_count = 0;
    size_t hit_count = 0;
    size_t miss_count = 0;
    base::Time oldest_access;
    base::Time newest_access;
  };

  explicit NsiteCacheManager(const base::FilePath& cache_dir);
  NsiteCacheManager(const base::FilePath& cache_dir, size_t max_cache_size);
  ~NsiteCacheManager();

  // Cache operations
  void PutFile(const std::string& npub,
               const std::string& path,
               const std::string& content,
               const std::string& content_type);

  // Returns nullptr if not found
  std::unique_ptr<CachedFile> GetFile(const std::string& npub,
                                      const std::string& path);

  // Remove a specific file
  void RemoveFile(const std::string& npub, const std::string& path);

  // Clear all files for a specific nsite
  void ClearNsite(const std::string& npub);

  // Clear entire cache
  void ClearAll();

  // Get cache statistics
  CacheStats GetStats() const;

  // Persist cache metadata to disk
  void SaveMetadata();

  // Load cache metadata from disk
  void LoadMetadata();

 private:
  // Generate cache key from npub and path
  std::string MakeCacheKey(const std::string& npub, 
                          const std::string& path) const;

  // Evict oldest files to make room
  void EvictToMakeRoom(size_t bytes_needed) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Update access time and count
  void UpdateAccessInfo(CachedFile* file) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Calculate content hash
  std::string CalculateHash(const std::string& content) const;

  // File operations
  base::FilePath GetFilePath(const std::string& cache_key) const;
  void WriteFileToDisk(const std::string& cache_key, 
                      const CachedFile& file);
  std::unique_ptr<CachedFile> ReadFileFromDisk(const std::string& cache_key);
  void DeleteFileFromDisk(const std::string& cache_key);

  base::FilePath cache_dir_;
  size_t max_cache_size_;
  
  mutable base::Lock lock_;
  
  // Main cache storage: cache_key -> file
  std::map<std::string, std::unique_ptr<CachedFile>> cache_ GUARDED_BY(lock_);
  
  // LRU tracking: access_time -> cache_key
  std::multimap<base::Time, std::string> lru_index_ GUARDED_BY(lock_);
  
  // Size tracking
  size_t total_size_ GUARDED_BY(lock_) = 0;
  
  // Statistics
  mutable CacheStats stats_ GUARDED_BY(lock_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<NsiteCacheManager> weak_factory_{this};
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NSITE_NSITE_CACHE_MANAGER_H_