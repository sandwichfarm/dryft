// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_cache_manager.h"

#include <algorithm>
#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "crypto/sha2.h"

namespace nostr {

namespace {

constexpr char kMetadataFilename[] = "cache_metadata.json";
constexpr char kDataSubdir[] = "data";

// Convert bytes to human readable format
std::string FormatBytes(size_t bytes) {
  if (bytes < 1024) {
    return base::NumberToString(bytes) + " B";
  } else if (bytes < 1024 * 1024) {
    return base::NumberToString(bytes / 1024) + " KB";
  } else {
    return base::NumberToString(bytes / (1024 * 1024)) + " MB";
  }
}

}  // namespace

NsiteCacheManager::NsiteCacheManager(const base::FilePath& cache_dir)
    : cache_dir_(cache_dir) {
  DCHECK(!cache_dir_.empty());
  
  // Create cache directory if needed
  if (!base::DirectoryExists(cache_dir_)) {
    base::CreateDirectory(cache_dir_);
  }
  
  // Create data subdirectory
  base::FilePath data_dir = cache_dir_.Append(kDataSubdir);
  if (!base::DirectoryExists(data_dir)) {
    base::CreateDirectory(data_dir);
  }
  
  // Load existing metadata
  LoadMetadata();
}

NsiteCacheManager::~NsiteCacheManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // Save metadata on destruction
  SaveMetadata();
}

void NsiteCacheManager::PutFile(const std::string& npub,
                                const std::string& path,
                                const std::string& content,
                                const std::string& content_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  std::string cache_key = MakeCacheKey(npub, path);
  size_t content_size = content.size();
  
  // Check if we need to evict files
  {
    base::AutoLock lock(lock_);
    
    // Remove existing file if present
    auto existing = cache_.find(cache_key);
    if (existing != cache_.end()) {
      total_size_ -= existing->second->size;
      cache_.erase(existing);
    }
    
    // Check if we need to make room
    if (total_size_ + content_size > kMaxCacheSize) {
      EvictToMakeRoom(content_size);
    }
    
    // Create new cached file
    auto file = std::make_unique<CachedFile>();
    file->npub = npub;
    file->path = path;
    file->content = content;
    file->content_type = content_type;
    file->hash = CalculateHash(content);
    file->size = content_size;
    file->created_at = base::Time::Now();
    file->last_accessed = file->created_at;
    file->access_count = 1;
    
    // Update indices
    total_size_ += content_size;
    lru_index_.emplace(file->last_accessed, cache_key);
    
    // Write to disk
    WriteFileToDisk(cache_key, *file);
    
    // Store in memory
    cache_[cache_key] = std::move(file);
    
    LOG(INFO) << "Cached file: " << npub << "/" << path 
              << " (" << FormatBytes(content_size) << ")";
  }
}

std::unique_ptr<NsiteCacheManager::CachedFile> NsiteCacheManager::GetFile(
    const std::string& npub,
    const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  std::string cache_key = MakeCacheKey(npub, path);
  
  base::AutoLock lock(lock_);
  
  auto it = cache_.find(cache_key);
  if (it == cache_.end()) {
    // Try loading from disk
    auto file = ReadFileFromDisk(cache_key);
    if (!file) {
      stats_.miss_count++;
      return nullptr;
    }
    
    // Add to memory cache
    UpdateAccessInfo(file.get());
    cache_[cache_key] = std::move(file);
    it = cache_.find(cache_key);
  }
  
  // Update access info
  UpdateAccessInfo(it->second.get());
  stats_.hit_count++;
  
  // Return a copy
  auto result = std::make_unique<CachedFile>(*it->second);
  return result;
}

void NsiteCacheManager::RemoveFile(const std::string& npub,
                                  const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  std::string cache_key = MakeCacheKey(npub, path);
  
  base::AutoLock lock(lock_);
  
  auto it = cache_.find(cache_key);
  if (it != cache_.end()) {
    total_size_ -= it->second->size;
    
    // Remove from LRU index
    auto range = lru_index_.equal_range(it->second->last_accessed);
    for (auto lru_it = range.first; lru_it != range.second; ++lru_it) {
      if (lru_it->second == cache_key) {
        lru_index_.erase(lru_it);
        break;
      }
    }
    
    cache_.erase(it);
    DeleteFileFromDisk(cache_key);
    
    LOG(INFO) << "Removed cached file: " << npub << "/" << path;
  }
}

void NsiteCacheManager::ClearNsite(const std::string& npub) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  base::AutoLock lock(lock_);
  
  std::vector<std::string> keys_to_remove;
  
  // Find all files for this npub
  for (const auto& entry : cache_) {
    if (entry.second->npub == npub) {
      keys_to_remove.push_back(entry.first);
    }
  }
  
  // Remove them
  for (const auto& key : keys_to_remove) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      total_size_ -= it->second->size;
      
      // Remove from LRU index
      auto range = lru_index_.equal_range(it->second->last_accessed);
      for (auto lru_it = range.first; lru_it != range.second; ++lru_it) {
        if (lru_it->second == key) {
          lru_index_.erase(lru_it);
          break;
        }
      }
      
      cache_.erase(it);
      DeleteFileFromDisk(key);
    }
  }
  
  LOG(INFO) << "Cleared " << keys_to_remove.size() << " files for nsite: " << npub;
}

void NsiteCacheManager::ClearAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  base::AutoLock lock(lock_);
  
  // Delete all files from disk
  for (const auto& entry : cache_) {
    DeleteFileFromDisk(entry.first);
  }
  
  cache_.clear();
  lru_index_.clear();
  total_size_ = 0;
  stats_ = CacheStats();
  
  LOG(INFO) << "Cleared entire cache";
}

NsiteCacheManager::CacheStats NsiteCacheManager::GetStats() const {
  base::AutoLock lock(lock_);
  
  stats_.total_size = total_size_;
  stats_.file_count = cache_.size();
  
  if (!lru_index_.empty()) {
    stats_.oldest_access = lru_index_.begin()->first;
    stats_.newest_access = lru_index_.rbegin()->first;
  }
  
  return stats_;
}

void NsiteCacheManager::SaveMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  base::Value::Dict metadata;
  base::Value::List files;
  
  {
    base::AutoLock lock(lock_);
    
    for (const auto& entry : cache_) {
      base::Value::Dict file_info;
      file_info.Set("key", entry.first);
      file_info.Set("npub", entry.second->npub);
      file_info.Set("path", entry.second->path);
      file_info.Set("content_type", entry.second->content_type);
      file_info.Set("hash", entry.second->hash);
      file_info.Set("size", static_cast<int>(entry.second->size));
      file_info.Set("created_at", entry.second->created_at.ToDoubleT());
      file_info.Set("last_accessed", entry.second->last_accessed.ToDoubleT());
      file_info.Set("access_count", entry.second->access_count);
      files.Append(std::move(file_info));
    }
  }
  
  metadata.Set("version", 1);
  metadata.Set("files", std::move(files));
  
  std::string json;
  base::JSONWriter::Write(metadata, &json);
  
  base::FilePath metadata_path = cache_dir_.Append(kMetadataFilename);
  base::WriteFile(metadata_path, json);
}

void NsiteCacheManager::LoadMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  base::FilePath metadata_path = cache_dir_.Append(kMetadataFilename);
  if (!base::PathExists(metadata_path)) {
    return;
  }
  
  std::string json;
  if (!base::ReadFileToString(metadata_path, &json)) {
    LOG(WARNING) << "Failed to read cache metadata";
    return;
  }
  
  auto value = base::JSONReader::Read(json);
  if (!value || !value->is_dict()) {
    LOG(WARNING) << "Invalid cache metadata format";
    return;
  }
  
  const base::Value::Dict& metadata = value->GetDict();
  const base::Value::List* files = metadata.FindList("files");
  if (!files) {
    return;
  }
  
  base::AutoLock lock(lock_);
  
  for (const auto& file_value : *files) {
    if (!file_value.is_dict()) {
      continue;
    }
    
    const base::Value::Dict& file_info = file_value.GetDict();
    
    const std::string* key = file_info.FindString("key");
    const std::string* npub = file_info.FindString("npub");
    const std::string* path = file_info.FindString("path");
    const std::string* content_type = file_info.FindString("content_type");
    const std::string* hash = file_info.FindString("hash");
    
    if (!key || !npub || !path || !content_type || !hash) {
      continue;
    }
    
    auto file = std::make_unique<CachedFile>();
    file->npub = *npub;
    file->path = *path;
    file->content_type = *content_type;
    file->hash = *hash;
    
    if (auto size = file_info.FindInt("size")) {
      file->size = *size;
    }
    
    if (auto created = file_info.FindDouble("created_at")) {
      file->created_at = base::Time::FromDoubleT(*created);
    }
    
    if (auto accessed = file_info.FindDouble("last_accessed")) {
      file->last_accessed = base::Time::FromDoubleT(*accessed);
    }
    
    if (auto count = file_info.FindInt("access_count")) {
      file->access_count = *count;
    }
    
    // Don't load content yet (lazy loading)
    total_size_ += file->size;
    lru_index_.emplace(file->last_accessed, *key);
    cache_[*key] = std::move(file);
  }
  
  LOG(INFO) << "Loaded cache metadata: " << cache_.size() << " files, "
            << FormatBytes(total_size_);
}

std::string NsiteCacheManager::MakeCacheKey(const std::string& npub,
                                           const std::string& path) const {
  // Use a delimiter that won't appear in npub or normal paths
  return npub + "|" + path;
}

void NsiteCacheManager::EvictToMakeRoom(size_t bytes_needed) {
  // Already holding lock_
  
  size_t freed = 0;
  std::vector<std::string> keys_to_remove;
  
  // Start with oldest files
  for (const auto& lru_entry : lru_index_) {
    if (total_size_ - freed + bytes_needed <= kMaxCacheSize) {
      break;
    }
    
    const std::string& cache_key = lru_entry.second;
    auto it = cache_.find(cache_key);
    if (it != cache_.end()) {
      freed += it->second->size;
      keys_to_remove.push_back(cache_key);
    }
  }
  
  // Remove the files
  for (const auto& key : keys_to_remove) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      total_size_ -= it->second->size;
      cache_.erase(it);
      DeleteFileFromDisk(key);
    }
  }
  
  // Remove from LRU index
  auto new_end = std::remove_if(
      lru_index_.begin(), lru_index_.end(),
      [&keys_to_remove](const auto& entry) {
        return std::find(keys_to_remove.begin(), keys_to_remove.end(),
                        entry.second) != keys_to_remove.end();
      });
  lru_index_.erase(new_end, lru_index_.end());
  
  LOG(INFO) << "Evicted " << keys_to_remove.size() << " files ("
            << FormatBytes(freed) << ") to make room";
}

void NsiteCacheManager::UpdateAccessInfo(CachedFile* file) {
  // Already holding lock_
  
  // Remove old LRU entry
  auto range = lru_index_.equal_range(file->last_accessed);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == MakeCacheKey(file->npub, file->path)) {
      lru_index_.erase(it);
      break;
    }
  }
  
  // Update access info
  file->last_accessed = base::Time::Now();
  file->access_count++;
  
  // Add new LRU entry
  lru_index_.emplace(file->last_accessed, MakeCacheKey(file->npub, file->path));
}

std::string NsiteCacheManager::CalculateHash(const std::string& content) const {
  std::string hash = crypto::SHA256HashString(content);
  return base::HexEncode(hash.data(), hash.size());
}

base::FilePath NsiteCacheManager::GetFilePath(
    const std::string& cache_key) const {
  // Use first 2 chars of hash for sharding
  std::string hash = crypto::SHA256HashString(cache_key);
  std::string hex = base::HexEncode(hash.data(), hash.size());
  
  return cache_dir_.Append(kDataSubdir)
                   .Append(hex.substr(0, 2))
                   .Append(hex);
}

void NsiteCacheManager::WriteFileToDisk(const std::string& cache_key,
                                        const CachedFile& file) {
  base::FilePath file_path = GetFilePath(cache_key);
  
  // Create parent directory if needed
  base::FilePath parent = file_path.DirName();
  if (!base::DirectoryExists(parent)) {
    base::CreateDirectory(parent);
  }
  
  // Write content
  base::WriteFile(file_path, file.content);
}

std::unique_ptr<NsiteCacheManager::CachedFile> 
NsiteCacheManager::ReadFileFromDisk(const std::string& cache_key) {
  base::FilePath file_path = GetFilePath(cache_key);
  
  if (!base::PathExists(file_path)) {
    return nullptr;
  }
  
  // Find metadata in cache
  auto it = cache_.find(cache_key);
  if (it == cache_.end()) {
    return nullptr;
  }
  
  // Load content from disk
  std::string content;
  if (!base::ReadFileToString(file_path, &content)) {
    return nullptr;
  }
  
  // Create copy with content
  auto file = std::make_unique<CachedFile>(*it->second);
  file->content = std::move(content);
  
  return file;
}

void NsiteCacheManager::DeleteFileFromDisk(const std::string& cache_key) {
  base::FilePath file_path = GetFilePath(cache_key);
  base::DeleteFile(file_path);
}

}  // namespace nostr