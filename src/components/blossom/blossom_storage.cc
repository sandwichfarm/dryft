// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blossom/blossom_storage.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "crypto/sha2.h"
#include "net/base/mime_sniffer.h"

namespace blossom {

namespace {

// Maximum bytes to read for MIME type detection
constexpr size_t kMimeSniffSize = 1024;

// Minimum free space to maintain (100MB)
constexpr int64_t kMinFreeSpace = 100 * 1024 * 1024;

// Metadata file extension
constexpr char kMetadataExtension[] = ".meta";

// Validate SHA256 hash format (64 hex characters)
bool IsValidSHA256(const std::string& hash) {
  if (hash.length() != 64) {
    return false;
  }
  
  for (char c : hash) {
    if (!base::IsHexDigit(c)) {
      return false;
    }
  }
  
  return true;
}

// Serialize metadata to string
std::string SerializeMetadata(const BlobMetadata& metadata) {
  base::Value::Dict dict;
  dict.Set("hash", metadata.hash);
  dict.Set("size", static_cast<double>(metadata.size));
  dict.Set("mime_type", metadata.mime_type);
  dict.Set("created_at", metadata.created_at.ToJsTimeIgnoringNull());
  dict.Set("last_accessed", metadata.last_accessed.ToJsTimeIgnoringNull());
  dict.Set("access_count", metadata.access_count);
  
  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

// Deserialize metadata from string
std::unique_ptr<BlobMetadata> DeserializeMetadata(const std::string& json) {
  auto value = base::JSONReader::Read(json);
  if (!value || !value->is_dict()) {
    return nullptr;
  }
  
  const auto& dict = value->GetDict();
  auto metadata = std::make_unique<BlobMetadata>();
  
  const auto* hash = dict.FindString("hash");
  if (!hash) return nullptr;
  metadata->hash = *hash;
  
  const auto size = dict.FindDouble("size");
  if (!size) return nullptr;
  metadata->size = static_cast<int64_t>(*size);
  
  const auto* mime_type = dict.FindString("mime_type");
  if (mime_type) {
    metadata->mime_type = *mime_type;
  }
  
  const auto created_at = dict.FindDouble("created_at");
  if (created_at) {
    metadata->created_at = base::Time::FromJsTime(*created_at);
  }
  
  const auto last_accessed = dict.FindDouble("last_accessed");
  if (last_accessed) {
    metadata->last_accessed = base::Time::FromJsTime(*last_accessed);
  }
  
  const auto access_count = dict.FindInt("access_count");
  if (access_count) {
    metadata->access_count = *access_count;
  }
  
  return metadata;
}

}  // namespace

BlossomStorage::BlossomStorage(const StorageConfig& config)
    : config_(config),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      total_size_(0),
      blob_count_(0) {
  DCHECK(config_.shard_depth >= 1 && config_.shard_depth <= 4);
}

BlossomStorage::~BlossomStorage() = default;

void BlossomStorage::Initialize(base::OnceCallback<void(bool success)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BlossomStorage::InitializeOnFileThread,
                     weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void BlossomStorage::StoreContent(const std::string& hash,
                                 const std::string& data,
                                 const std::string& mime_type,
                                 StoreCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsValidSHA256(hash)) {
    std::move(callback).Run(false, "Invalid SHA256 hash format");
    return;
  }
  
  if (data.size() > static_cast<size_t>(config_.max_blob_size)) {
    std::move(callback).Run(false, "Blob exceeds maximum size");
    return;
  }
  
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BlossomStorage::StoreContentOnFileThread,
                     weak_factory_.GetWeakPtr(),
                     hash, data, mime_type,
                     std::move(callback)));
}

void BlossomStorage::StoreContentFromFile(const std::string& hash,
                                         const base::FilePath& source_path,
                                         const std::string& mime_type,
                                         StoreCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsValidSHA256(hash)) {
    std::move(callback).Run(false, "Invalid SHA256 hash format");
    return;
  }
  
  // Read file and store
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<BlossomStorage> storage,
             std::string hash,
             base::FilePath source_path,
             std::string mime_type,
             StoreCallback callback) {
            if (!storage) {
              std::move(callback).Run(false, "Storage destroyed");
              return;
            }
            
            std::string data;
            if (!base::ReadFileToString(source_path, &data)) {
              std::move(callback).Run(false, "Failed to read source file");
              return;
            }
            
            storage->StoreContentOnFileThread(hash, data, mime_type,
                                            std::move(callback));
          },
          weak_factory_.GetWeakPtr(),
          hash, source_path, mime_type,
          std::move(callback)));
}

void BlossomStorage::GetContent(const std::string& hash, GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsValidSHA256(hash)) {
    std::move(callback).Run(false, "", "");
    return;
  }
  
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BlossomStorage::GetContentOnFileThread,
                     weak_factory_.GetWeakPtr(),
                     hash,
                     std::move(callback)));
}

void BlossomStorage::GetContentPath(const std::string& hash,
                                   base::OnceCallback<void(const base::FilePath&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsValidSHA256(hash)) {
    std::move(callback).Run(base::FilePath());
    return;
  }
  
  base::FilePath path = GetShardedPath(hash);
  std::move(callback).Run(path);
}

void BlossomStorage::HasContent(const std::string& hash,
                               base::OnceCallback<void(bool exists)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsValidSHA256(hash)) {
    std::move(callback).Run(false);
    return;
  }
  
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&base::PathExists, GetShardedPath(hash)),
      std::move(callback));
}

void BlossomStorage::DeleteContent(const std::string& hash, DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsValidSHA256(hash)) {
    std::move(callback).Run(false);
    return;
  }
  
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BlossomStorage::DeleteContentOnFileThread,
                     weak_factory_.GetWeakPtr(),
                     hash,
                     std::move(callback)));
}

void BlossomStorage::GetMetadata(const std::string& hash, MetadataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsValidSHA256(hash)) {
    std::move(callback).Run(nullptr);
    return;
  }
  
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath metadata_path, MetadataCallback callback) {
            std::string json;
            if (!base::ReadFileToString(metadata_path, &json)) {
              std::move(callback).Run(nullptr);
              return;
            }
            
            auto metadata = DeserializeMetadata(json);
            std::move(callback).Run(std::move(metadata));
          },
          GetShardedPath(hash).AddExtension(kMetadataExtension),
          std::move(callback)));
}

void BlossomStorage::GetStats(StatsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<BlossomStorage> storage,
             base::FilePath root_path,
             StatsCallback callback) {
            if (!storage) {
              std::move(callback).Run(0, 0);
              return;
            }
            
            int64_t total_size = 0;
            int64_t blob_count = 0;
            
            base::FileEnumerator enumerator(
                root_path, true, base::FileEnumerator::FILES);
            
            for (base::FilePath path = enumerator.Next();
                 !path.empty();
                 path = enumerator.Next()) {
              // Skip metadata files
              if (path.Extension() == kMetadataExtension) {
                continue;
              }
              
              const auto& info = enumerator.GetInfo();
              total_size += info.GetSize();
              blob_count++;
            }
            
            storage->total_size_ = total_size;
            storage->blob_count_ = blob_count;
            
            std::move(callback).Run(total_size, blob_count);
          },
          weak_factory_.GetWeakPtr(),
          config_.root_path,
          std::move(callback)));
}

void BlossomStorage::RunCleanup(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<BlossomStorage> storage,
             base::OnceClosure callback) {
            if (!storage) {
              std::move(callback).Run();
              return;
            }
            
            // TODO: Implement LRU eviction and orphan cleanup
            LOG(INFO) << "Running Blossom storage cleanup";
            
            std::move(callback).Run();
          },
          weak_factory_.GetWeakPtr(),
          std::move(callback)));
}

void BlossomStorage::VerifyContent(const std::string& hash,
                                  base::OnceCallback<void(bool valid)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsValidSHA256(hash)) {
    std::move(callback).Run(false);
    return;
  }
  
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<BlossomStorage> storage,
             std::string hash,
             base::FilePath path,
             base::OnceCallback<void(bool)> callback) {
            if (!storage) {
              std::move(callback).Run(false);
              return;
            }
            
            std::string data;
            if (!base::ReadFileToString(path, &data)) {
              std::move(callback).Run(false);
              return;
            }
            
            bool valid = storage->VerifySHA256(data, hash);
            std::move(callback).Run(valid);
          },
          weak_factory_.GetWeakPtr(),
          hash,
          GetShardedPath(hash),
          std::move(callback)));
}

void BlossomStorage::InitializeOnFileThread(base::OnceCallback<void(bool)> callback) {
  // Create root directory
  if (!base::CreateDirectory(config_.root_path)) {
    LOG(ERROR) << "Failed to create Blossom storage directory: " 
               << config_.root_path;
    std::move(callback).Run(false);
    return;
  }
  
  // Calculate initial statistics
  GetStats(base::BindOnce(
      [](base::OnceCallback<void(bool)> callback,
         int64_t total_size, int64_t blob_count) {
        LOG(INFO) << "Blossom storage initialized with " << blob_count 
                  << " blobs, total size: " << total_size;
        std::move(callback).Run(true);
      },
      std::move(callback)));
}

void BlossomStorage::StoreContentOnFileThread(const std::string& hash,
                                             const std::string& data,
                                             const std::string& mime_type,
                                             StoreCallback callback) {
  // Verify hash
  if (!VerifySHA256(data, hash)) {
    std::move(callback).Run(false, "SHA256 hash mismatch");
    return;
  }
  
  // Check if we need to evict old content
  if (config_.enable_lru_eviction && 
      total_size_ + static_cast<int64_t>(data.size()) > config_.max_total_size) {
    EvictOldestBlobs(data.size() + kMinFreeSpace);
  }
  
  // Get target path
  base::FilePath target_path = GetShardedPath(hash);
  
  // Check if already exists
  if (base::PathExists(target_path)) {
    // Update access time
    UpdateAccessTime(hash);
    std::move(callback).Run(true, "");
    return;
  }
  
  // Create parent directories
  base::FilePath parent_dir = target_path.DirName();
  if (!base::CreateDirectory(parent_dir)) {
    std::move(callback).Run(false, "Failed to create directory");
    return;
  }
  
  // Write to temporary file first (atomic write)
  base::FilePath temp_path = target_path.AddExtension(".tmp");
  if (!base::WriteFile(temp_path, data)) {
    std::move(callback).Run(false, "Failed to write file");
    return;
  }
  
  // Move to final location
  if (!base::Move(temp_path, target_path)) {
    base::DeleteFile(temp_path);
    std::move(callback).Run(false, "Failed to move file");
    return;
  }
  
  // Write metadata
  BlobMetadata metadata;
  metadata.hash = hash;
  metadata.size = data.size();
  metadata.mime_type = mime_type.empty() ? DetectMimeType(data) : mime_type;
  metadata.created_at = base::Time::Now();
  metadata.last_accessed = base::Time::Now();
  metadata.access_count = 0;
  
  base::FilePath metadata_path = target_path.AddExtension(kMetadataExtension);
  std::string metadata_json = SerializeMetadata(metadata);
  if (!base::WriteFile(metadata_path, metadata_json)) {
    LOG(WARNING) << "Failed to write metadata for " << hash;
  }
  
  // Update statistics
  total_size_ += data.size();
  blob_count_++;
  
  std::move(callback).Run(true, "");
}

void BlossomStorage::GetContentOnFileThread(const std::string& hash, 
                                           GetCallback callback) {
  base::FilePath path = GetShardedPath(hash);
  
  std::string data;
  if (!base::ReadFileToString(path, &data)) {
    std::move(callback).Run(false, "", "");
    return;
  }
  
  // Verify hash
  if (!VerifySHA256(data, hash)) {
    LOG(ERROR) << "SHA256 mismatch for stored blob: " << hash;
    std::move(callback).Run(false, "", "");
    return;
  }
  
  // Get metadata for MIME type
  std::string mime_type;
  base::FilePath metadata_path = path.AddExtension(kMetadataExtension);
  std::string metadata_json;
  if (base::ReadFileToString(metadata_path, &metadata_json)) {
    auto metadata = DeserializeMetadata(metadata_json);
    if (metadata) {
      mime_type = metadata->mime_type;
    }
  }
  
  // Update access time
  UpdateAccessTime(hash);
  
  std::move(callback).Run(true, data, mime_type);
}

void BlossomStorage::DeleteContentOnFileThread(const std::string& hash,
                                              DeleteCallback callback) {
  base::FilePath path = GetShardedPath(hash);
  base::FilePath metadata_path = path.AddExtension(kMetadataExtension);
  
  // Get file size for statistics update
  int64_t file_size = 0;
  base::GetFileSize(path, &file_size);
  
  // Delete content file
  bool success = base::DeleteFile(path);
  
  // Delete metadata file
  base::DeleteFile(metadata_path);
  
  if (success) {
    // Update statistics
    total_size_ -= file_size;
    blob_count_--;
  }
  
  std::move(callback).Run(success);
}

base::FilePath BlossomStorage::GetShardedPath(const std::string& hash) const {
  DCHECK_EQ(hash.length(), 64u);
  
  base::FilePath path = config_.root_path;
  
  // Add sharding directories based on hash prefix
  for (int i = 0; i < config_.shard_depth && i < 4; ++i) {
    path = path.Append(hash.substr(i * 2, 2));
  }
  
  // Add full hash as filename
  return path.Append(hash);
}

std::string BlossomStorage::CalculateSHA256(const std::string& data) const {
  std::string hash = crypto::SHA256HashString(data);
  return base::HexEncode(hash.data(), hash.size());
}

bool BlossomStorage::VerifySHA256(const std::string& data,
                                 const std::string& expected_hash) const {
  std::string calculated_hash = CalculateSHA256(data);
  return base::EqualsCaseInsensitiveASCII(calculated_hash, expected_hash);
}

std::string BlossomStorage::DetectMimeType(const std::string& data) const {
  std::string mime_type;
  net::SniffMimeType(data.substr(0, kMimeSniffSize), GURL(), "", 
                     net::ForceSniffFileUrlsForHtml::kDisabled, &mime_type);
  return mime_type;
}

void BlossomStorage::EvictOldestBlobs(int64_t bytes_needed) {
  // TODO: Implement LRU eviction based on access time and frequency
  LOG(WARNING) << "LRU eviction not yet implemented, need " << bytes_needed 
               << " bytes";
}

void BlossomStorage::UpdateAccessTime(const std::string& hash) {
  base::FilePath metadata_path = GetShardedPath(hash).AddExtension(kMetadataExtension);
  
  // Read existing metadata
  std::string json;
  if (!base::ReadFileToString(metadata_path, &json)) {
    return;
  }
  
  auto metadata = DeserializeMetadata(json);
  if (!metadata) {
    return;
  }
  
  // Update access info
  metadata->last_accessed = base::Time::Now();
  metadata->access_count++;
  
  // Write back
  std::string updated_json = SerializeMetadata(*metadata);
  base::WriteFile(metadata_path, updated_json);
}

}  // namespace blossom