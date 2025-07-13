// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_LOCAL_RELAY_EVENT_STORAGE_H_
#define CHROME_BROWSER_NOSTR_LOCAL_RELAY_EVENT_STORAGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace nostr {

// Forward declarations
struct NostrEvent;
struct NostrFilter;

namespace local_relay {

// Forward declarations
class NostrDatabase;

// Options for querying events
struct QueryOptions {
  // Maximum number of events to return
  int limit = 1000;
  
  // Whether to return results in reverse chronological order
  bool reverse_order = false;
  
  // Whether to include deleted events
  bool include_deleted = false;
  
  // Timeout for the query
  base::TimeDelta timeout = base::Seconds(5);
};

// Statistics about stored events
struct StorageStats {
  int64_t total_events = 0;
  int64_t total_size_bytes = 0;
  int64_t events_last_hour = 0;
  int64_t events_last_day = 0;
  base::Time oldest_event;
  base::Time newest_event;
};

// Manages event storage and retrieval for the local relay
class EventStorage {
 public:
  using StoreCallback = base::OnceCallback<void(bool success, const std::string& error)>;
  using QueryCallback = base::OnceCallback<void(std::vector<std::unique_ptr<NostrEvent>> events)>;
  using StreamCallback = base::RepeatingCallback<void(std::unique_ptr<NostrEvent> event)>;
  using StatsCallback = base::OnceCallback<void(const StorageStats& stats)>;

  explicit EventStorage(NostrDatabase* database);
  ~EventStorage();

  // Store a new event
  // Returns false if event is invalid or duplicate
  void StoreEvent(std::unique_ptr<NostrEvent> event, StoreCallback callback);
  
  // Store multiple events in a batch
  void StoreEvents(std::vector<std::unique_ptr<NostrEvent>> events,
                   base::OnceCallback<void(int stored_count)> callback);
  
  // Query events with filters
  void QueryEvents(const std::vector<NostrFilter>& filters,
                   const QueryOptions& options,
                   QueryCallback callback);
  
  // Query events with streaming results (for large result sets)
  void QueryEventsStreaming(const std::vector<NostrFilter>& filters,
                           const QueryOptions& options,
                           StreamCallback stream_callback,
                           base::OnceClosure done_callback);
  
  // Get a single event by ID
  void GetEvent(const std::string& event_id,
                base::OnceCallback<void(std::unique_ptr<NostrEvent>)> callback);
  
  // Delete an event (soft delete)
  void DeleteEvent(const std::string& event_id,
                   base::OnceCallback<void(bool success)> callback);
  
  // Delete events older than a certain time
  void DeleteEventsOlderThan(base::Time cutoff,
                            base::OnceCallback<void(int deleted_count)> callback);
  
  // Process a NIP-09 deletion event
  void ProcessDeletionEvent(const NostrEvent& deletion_event,
                           base::OnceCallback<void(int deleted_count)> callback);
  
  // Get storage statistics
  void GetStorageStats(StatsCallback callback);
  
  // Check if an event exists
  void EventExists(const std::string& event_id,
                   base::OnceCallback<void(bool exists)> callback);
  
  // Optimize database (VACUUM, ANALYZE, etc.)
  void OptimizeStorage(base::OnceClosure callback);

 private:
  // Validate event before storage
  bool ValidateEvent(const NostrEvent& event, std::string& error);
  
  // Check event signature
  bool VerifyEventSignature(const NostrEvent& event);
  
  // Calculate event ID and verify it matches
  bool VerifyEventId(const NostrEvent& event);
  
  // Check if event should replace an existing one
  void CheckReplaceableEvent(const NostrEvent& event,
                            base::OnceCallback<void(bool should_store)> callback);
  
  // Process a single event for storage
  void ProcessEventForStorage(std::unique_ptr<NostrEvent> event,
                             StoreCallback callback);
  
  // Handle special event kinds (replaceable, ephemeral, etc.)
  void HandleSpecialEventKind(const NostrEvent& event);
  
  // Database instance (not owned)
  NostrDatabase* database_;
  
  // Thread checker
  SEQUENCE_CHECKER(sequence_checker_);
  
  // Weak pointer factory
  base::WeakPtrFactory<EventStorage> weak_factory_{this};
};

}  // namespace local_relay
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_LOCAL_RELAY_EVENT_STORAGE_H_