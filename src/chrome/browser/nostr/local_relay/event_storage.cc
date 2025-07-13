// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/event_storage.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/nostr/local_relay/nostr_database.h"
#include "components/nostr/nostr_event.h"
#include "components/nostr/nostr_filter.h"
#include "crypto/sha2.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace nostr {
namespace local_relay {

namespace {

// Maximum event size in bytes
constexpr size_t kMaxEventSize = 256 * 1024;  // 256KB

// Maximum number of tags per event
constexpr size_t kMaxTagsPerEvent = 1000;

// Maximum tag value length
constexpr size_t kMaxTagValueLength = 1024;

// Convert hex string to bytes
std::vector<uint8_t> HexToBytes(const std::string& hex) {
  std::vector<uint8_t> bytes;
  if (hex.length() % 2 != 0) {
    return bytes;
  }
  
  bytes.reserve(hex.length() / 2);
  for (size_t i = 0; i < hex.length(); i += 2) {
    uint8_t byte;
    if (!base::HexStringToUInt8(hex.substr(i, 2), &byte)) {
      return std::vector<uint8_t>();
    }
    bytes.push_back(byte);
  }
  return bytes;
}

// Serialize event for ID calculation
std::string SerializeEventForId(const NostrEvent& event) {
  // Format: [0,pubkey,created_at,kind,tags,content]
  base::Value::List event_data;
  event_data.Append(0);
  event_data.Append(event.pubkey);
  event_data.Append(static_cast<double>(event.created_at));
  event_data.Append(event.kind);
  event_data.Append(event.tags.Clone());
  event_data.Append(event.content);
  
  std::string json;
  if (!base::JSONWriter::Write(event_data, &json)) {
    return std::string();
  }
  
  return json;
}

}  // namespace

EventStorage::EventStorage(NostrDatabase* database) : database_(database) {
  DCHECK(database_);
}

EventStorage::~EventStorage() = default;

void EventStorage::StoreEvent(std::unique_ptr<NostrEvent> event,
                             StoreCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!event) {
    std::move(callback).Run(false, "null event");
    return;
  }
  
  ProcessEventForStorage(std::move(event), std::move(callback));
}

void EventStorage::StoreEvents(std::vector<std::unique_ptr<NostrEvent>> events,
                              base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (events.empty()) {
    std::move(callback).Run(0);
    return;
  }
  
  // Process events in sequence
  // In a real implementation, this could be optimized with batch operations
  auto* stored_count = new int(0);
  auto remaining = std::make_unique<int>(events.size());
  
  for (auto& event : events) {
    StoreEvent(
        std::move(event),
        base::BindOnce(
            [](int* count, int* remaining, 
               base::OnceCallback<void(int)> callback,
               bool success, const std::string& error) {
              if (success) {
                (*count)++;
              }
              (*remaining)--;
              
              if (*remaining == 0) {
                int final_count = *count;
                delete count;
                delete remaining;
                std::move(callback).Run(final_count);
              }
            },
            stored_count, remaining.get(), 
            remaining.get() == &events.back() ? std::move(callback) : 
                base::OnceCallback<void(int)>()));
  }
  
  remaining.release();  // Ownership transferred to callbacks
}

void EventStorage::QueryEvents(const std::vector<NostrFilter>& filters,
                              const QueryOptions& options,
                              QueryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // For now, use database directly but apply options
  // TODO: Integrate QueryEngine for full optimization
  database_->QueryEvents(
      filters,
      options.limit,
      base::BindOnce(
          [](QueryCallback callback,
             const QueryOptions& options,
             std::vector<std::unique_ptr<NostrEvent>> events) {
            // Apply reverse order if requested
            if (options.reverse_order) {
              std::reverse(events.begin(), events.end());
            }
            
            // Note: include_deleted is handled by database query
            // which always excludes deleted unless specifically included
            
            std::move(callback).Run(std::move(events));
          },
          std::move(callback),
          options));
}

void EventStorage::QueryEventsStreaming(const std::vector<NostrFilter>& filters,
                                       const QueryOptions& options,
                                       StreamCallback stream_callback,
                                       base::OnceClosure done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // For now, use non-streaming query and stream the results
  // TODO: Implement true streaming from database
  QueryEvents(
      filters,
      options,
      base::BindOnce(
          [](StreamCallback stream_callback,
             base::OnceClosure done_callback,
             std::vector<std::unique_ptr<NostrEvent>> events) {
            for (auto& event : events) {
              stream_callback.Run(std::move(event));
            }
            std::move(done_callback).Run();
          },
          stream_callback,
          std::move(done_callback)));
}

void EventStorage::GetEvent(const std::string& event_id,
                           base::OnceCallback<void(std::unique_ptr<NostrEvent>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  database_->GetEvent(event_id, std::move(callback));
}

void EventStorage::DeleteEvent(const std::string& event_id,
                              base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  database_->DeleteEvent(event_id, std::move(callback));
}

void EventStorage::DeleteEventsOlderThan(base::Time cutoff,
                                        base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  database_->RemoveExpiredEvents(
      base::BindOnce(
          [](base::Time cutoff,
             base::OnceCallback<void(int)> callback,
             bool success) {
            // TODO: Return actual count from database
            std::move(callback).Run(success ? 1 : 0);
          },
          cutoff,
          std::move(callback)));
}

void EventStorage::ProcessDeletionEvent(const NostrEvent& deletion_event,
                                       base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (deletion_event.kind != 5) {
    std::move(callback).Run(0);
    return;
  }
  
  database_->ProcessDeletionEvent(
      deletion_event,
      base::BindOnce(
          [](base::OnceCallback<void(int)> callback, bool success) {
            // TODO: Return actual count from database
            std::move(callback).Run(success ? 1 : 0);
          },
          std::move(callback)));
}

void EventStorage::GetStorageStats(StatsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  database_->GetDatabaseStats(
      base::BindOnce(
          [](StatsCallback callback, base::Value::Dict stats) {
            StorageStats storage_stats;
            
            if (auto* total = stats.FindDouble("total_events")) {
              storage_stats.total_events = static_cast<int64_t>(*total);
            }
            if (auto* size = stats.FindDouble("database_size_bytes")) {
              storage_stats.total_size_bytes = static_cast<int64_t>(*size);
            }
            
            std::move(callback).Run(storage_stats);
          },
          std::move(callback)));
}

void EventStorage::EventExists(const std::string& event_id,
                              base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  GetEvent(event_id,
           base::BindOnce(
               [](base::OnceCallback<void(bool)> callback,
                  std::unique_ptr<NostrEvent> event) {
                 std::move(callback).Run(event != nullptr);
               },
               std::move(callback)));
}

void EventStorage::OptimizeStorage(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  database_->OptimizeDatabase(std::move(callback));
}

bool EventStorage::ValidateEvent(const NostrEvent& event, std::string& error) {
  // Check required fields
  if (event.id.empty() || event.id.length() != 64) {
    error = "invalid event id";
    return false;
  }
  
  if (event.pubkey.empty() || event.pubkey.length() != 64) {
    error = "invalid pubkey";
    return false;
  }
  
  if (event.sig.empty() || event.sig.length() != 128) {
    error = "invalid signature";
    return false;
  }
  
  if (event.created_at <= 0) {
    error = "invalid timestamp";
    return false;
  }
  
  // Check event size
  std::string event_json = base::WriteJson(event.ToValue()).value_or("");
  if (event_json.size() > kMaxEventSize) {
    error = "event too large";
    return false;
  }
  
  // Check tag limits
  if (event.tags.size() > kMaxTagsPerEvent) {
    error = "too many tags";
    return false;
  }
  
  // Validate tag structure
  for (const auto& tag : event.tags) {
    const auto* tag_array = tag.GetIfList();
    if (!tag_array || tag_array->empty()) {
      error = "invalid tag structure";
      return false;
    }
    
    // Check tag values
    for (const auto& value : *tag_array) {
      const auto* str = value.GetIfString();
      if (str && str->length() > kMaxTagValueLength) {
        error = "tag value too long";
        return false;
      }
    }
  }
  
  // Verify event ID
  if (!VerifyEventId(event)) {
    error = "event id does not match content";
    return false;
  }
  
  // Verify signature
  if (!VerifyEventSignature(event)) {
    error = "invalid event signature";
    return false;
  }
  
  return true;
}

bool EventStorage::VerifyEventSignature(const NostrEvent& event) {
  // Simplified signature verification
  // TODO: Implement proper secp256k1 schnorr signature verification
  
  // For now, just check format
  auto sig_bytes = HexToBytes(event.sig);
  auto pubkey_bytes = HexToBytes(event.pubkey);
  
  return sig_bytes.size() == 64 && pubkey_bytes.size() == 32;
}

bool EventStorage::VerifyEventId(const NostrEvent& event) {
  // Calculate expected event ID
  std::string serialized = SerializeEventForId(event);
  if (serialized.empty()) {
    return false;
  }
  
  // Calculate SHA256
  std::string hash = crypto::SHA256HashString(serialized);
  std::string calculated_id = base::HexEncode(hash.data(), hash.size());
  base::ToLowerASCII(&calculated_id);
  
  return calculated_id == event.id;
}

void EventStorage::CheckReplaceableEvent(
    const NostrEvent& event,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!event.IsReplaceable() && !event.IsParameterizedReplaceable()) {
    std::move(callback).Run(true);  // Not replaceable, store it
    return;
  }
  
  if (event.IsParameterizedReplaceable()) {
    // Check for existing parameterized replaceable
    std::string d_tag = event.GetDTagValue();
    database_->GetReplaceableEvent(
        event.pubkey,
        event.kind,
        d_tag,
        base::BindOnce(
            [](const NostrEvent& new_event,
               base::OnceCallback<void(bool)> callback,
               std::unique_ptr<NostrEvent> existing) {
              if (!existing) {
                std::move(callback).Run(true);  // No existing, store it
                return;
              }
              
              // Only store if newer
              std::move(callback).Run(new_event.created_at > existing->created_at);
            },
            std::ref(event),
            std::move(callback)));
  } else {
    // Regular replaceable event - the database handles this
    std::move(callback).Run(true);
  }
}

void EventStorage::ProcessEventForStorage(std::unique_ptr<NostrEvent> event,
                                         StoreCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // Validate event
  std::string error;
  if (!ValidateEvent(*event, error)) {
    VLOG(1) << "Event validation failed: " << error;
    std::move(callback).Run(false, error);
    return;
  }
  
  // Check if it's a replaceable event
  CheckReplaceableEvent(
      *event,
      base::BindOnce(
          [](base::WeakPtr<EventStorage> storage,
             std::unique_ptr<NostrEvent> event,
             StoreCallback callback,
             bool should_store) {
            if (!storage) {
              std::move(callback).Run(false, "storage destroyed");
              return;
            }
            
            if (!should_store) {
              std::move(callback).Run(false, "older replaceable event");
              return;
            }
            
            // Handle special event kinds
            storage->HandleSpecialEventKind(*event);
            
            // Store in database
            storage->database_->StoreEvent(
                std::move(event),
                base::BindOnce(
                    [](StoreCallback callback, bool success) {
                      std::move(callback).Run(
                          success, 
                          success ? "" : "database error");
                    },
                    std::move(callback)));
          },
          weak_factory_.GetWeakPtr(),
          std::move(event),
          std::move(callback)));
}

void EventStorage::HandleSpecialEventKind(const NostrEvent& event) {
  // Handle ephemeral events (20000-29999)
  if (event.IsEphemeral()) {
    // Ephemeral events are not stored permanently
    // They're only broadcast to current subscribers
    VLOG(2) << "Ephemeral event kind " << event.kind << " will not be persisted";
    return;
  }
  
  // Handle deletion events (kind 5)
  if (event.kind == 5) {
    // Process deletions after storing the deletion event itself
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&EventStorage::ProcessDeletionEvent,
                      weak_factory_.GetWeakPtr(),
                      event,
                      base::BindOnce([](int count) {
                        VLOG(2) << "Processed deletion event, deleted " 
                                << count << " events";
                      })));
  }
}

}  // namespace local_relay
}  // namespace nostr