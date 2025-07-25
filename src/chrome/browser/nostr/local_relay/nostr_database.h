// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_LOCAL_RELAY_NOSTR_DATABASE_H_
#define CHROME_BROWSER_NOSTR_LOCAL_RELAY_NOSTR_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "sql/database.h"
#include "sql/meta_table.h"

namespace nostr {
namespace local_relay {

// Forward declarations
struct NostrEvent;
struct NostrFilter;

// Database for storing Nostr events in a local relay
// This class must be used on a single sequence (database thread)
class NostrDatabase {
 public:
  // Callback types
  using InitCallback = base::OnceCallback<void(bool success)>;
  using EventCallback = base::OnceCallback<void(std::unique_ptr<NostrEvent>)>;
  using EventListCallback = 
      base::OnceCallback<void(std::vector<std::unique_ptr<NostrEvent>>)>;
  using CountCallback = base::OnceCallback<void(int64_t count)>;
  using StatusCallback = base::OnceCallback<void(bool success)>;

  // Database configuration
  struct Config {
    // Maximum database size in bytes (0 = unlimited)
    int64_t max_size_bytes = 1024 * 1024 * 1024;  // 1GB default
    
    // Maximum number of events to store (0 = unlimited)
    int64_t max_event_count = 1000000;  // 1M events default
    
    // Event retention period in days (0 = no expiration)
    int retention_days = 0;
    
    // Whether to enable auto-vacuum
    bool auto_vacuum = true;
    
    // Page size for SQLite (must be power of 2, 512-65536)
    int page_size = 4096;
    
    // Cache size in pages (-1 = default)
    int cache_size = -1;
  };

  explicit NostrDatabase(const base::FilePath& db_path,
                        const Config& config = Config());
  ~NostrDatabase();

  // Initialize the database (create tables, indexes, etc.)
  // Must be called before any other operations
  void Initialize(InitCallback callback);

  // Close the database
  void Close();

  // Event storage operations
  
  // Store a new event
  void StoreEvent(std::unique_ptr<NostrEvent> event,
                 StatusCallback callback);
  
  // Retrieve an event by ID
  void GetEvent(const std::string& event_id,
               EventCallback callback);
  
  // Query events matching filters
  void QueryEvents(const std::vector<NostrFilter>& filters,
                  int limit,
                  EventListCallback callback);
  
  // Delete an event (soft delete)
  void DeleteEvent(const std::string& event_id,
                  StatusCallback callback);
  
  // Process a deletion event (NIP-09)
  void ProcessDeletionEvent(const NostrEvent& deletion_event,
                           StatusCallback callback);

  // Replaceable event operations (NIP-16)
  
  // Update or insert a replaceable event
  void StoreReplaceableEvent(std::unique_ptr<NostrEvent> event,
                            StatusCallback callback);
  
  // Get the current replaceable event
  void GetReplaceableEvent(const std::string& pubkey,
                          int kind,
                          const std::string& d_tag,
                          EventCallback callback);

  // Maintenance operations
  
  // Remove expired events
  void RemoveExpiredEvents(StatusCallback callback);
  
  // Vacuum the database to reclaim space
  void VacuumDatabase(StatusCallback callback);
  
  // Optimize database (ANALYZE)
  void OptimizeDatabase(StatusCallback callback);
  
  // Get database statistics
  void GetDatabaseStats(base::OnceCallback<void(base::Value::Dict)> callback);

  // Query operations
  
  // Count total events
  void CountEvents(CountCallback callback);
  
  // Count events by kind
  void CountEventsByKind(int kind, CountCallback callback);
  
  // Count events by author
  void CountEventsByAuthor(const std::string& pubkey,
                          CountCallback callback);
  
  // Get database size in bytes
  void GetDatabaseSize(CountCallback callback);

  // Migration support
  
  // Get current schema version
  int GetSchemaVersion() const;
  
  // Migrate database to latest schema
  bool MigrateToLatestSchema();

 private:
  // Database operations (run on database sequence)
  
  bool InitializeInternal();
  bool CreateTables();
  bool CreateIndexes();
  bool CreateMetadata();
  
  bool StoreEventInternal(const NostrEvent& event);
  std::unique_ptr<NostrEvent> GetEventInternal(const std::string& event_id);
  std::vector<std::unique_ptr<NostrEvent>> QueryEventsInternal(
      const std::vector<NostrFilter>& filters, int limit);
  
  bool DeleteEventInternal(const std::string& event_id);
  bool ProcessDeletionEventInternal(const NostrEvent& deletion_event);
  
  bool StoreReplaceableEventInternal(const NostrEvent& event);
  std::unique_ptr<NostrEvent> GetReplaceableEventInternal(
      const std::string& pubkey, int kind, const std::string& d_tag);
  
  bool StoreTags(const std::string& event_id, const base::Value::List& tags);
  bool IsEventDeleted(const std::string& event_id);
  bool IsReplaceableKind(int kind);
  std::string GetDTagValue(const base::Value::List& tags);
  
  bool RemoveExpiredEventsInternal();
  bool VacuumDatabaseInternal();
  bool OptimizeDatabaseInternal();
  
  base::Value::Dict GetDatabaseStatsInternal();
  int64_t CountEventsInternal();
  int64_t CountEventsByKindInternal(int kind);
  int64_t CountEventsByAuthorInternal(const std::string& pubkey);
  int64_t GetDatabaseSizeInternal();
  
  // Apply database configuration
  bool ApplyConfiguration();
  
  // Enforce storage limits
  void EnforceStorageLimits();
  
  // Helper to run callback on the calling sequence
  template <typename Callback, typename... Args>
  void PostCallback(Callback callback, Args&&... args);

  // Database path
  base::FilePath db_path_;
  
  // Configuration
  Config config_;
  
  // SQLite database
  std::unique_ptr<sql::Database> db_;
  
  // Meta table for versioning
  std::unique_ptr<sql::MetaTable> meta_table_;
  
  // Task runner for database operations
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  
  // Task runner that created this object (for callbacks)
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;
  
  // Ensure database operations run on correct sequence
  SEQUENCE_CHECKER(sequence_checker_);
  
  // Weak pointer factory
  base::WeakPtrFactory<NostrDatabase> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NostrDatabase);
};

// Nostr event structure matching NIP-01
struct NostrEvent {
  std::string id;                    // 32-byte hex event ID
  std::string pubkey;                // 32-byte hex public key
  int64_t created_at;                // Unix timestamp
  int kind;                          // Event kind
  base::Value::List tags;            // Array of tag arrays
  std::string content;               // Event content
  std::string sig;                   // 64-byte hex signature
  
  // Local relay metadata
  base::Time received_at;            // When relay received the event
  bool deleted = false;              // Soft delete flag
  
  // Convert to JSON for wire protocol
  base::Value::Dict ToDict() const;
  
  // Parse from JSON
  static std::unique_ptr<NostrEvent> FromDict(const base::Value::Dict& dict);
  
  // Validate event structure and signature
  bool IsValid() const;
  
  // Compute event ID from content
  std::string ComputeId() const;
};

// Nostr filter structure for REQ subscriptions
struct NostrFilter {
  std::vector<std::string> ids;      // Event IDs to match
  std::vector<std::string> authors;  // Public keys to match
  std::vector<int> kinds;            // Event kinds to match
  
  // Tag filters (#e, #p, etc.)
  std::map<std::string, std::vector<std::string>> tags;
  
  // Time range filters
  std::optional<int64_t> since;      // Events created after this time
  std::optional<int64_t> until;      // Events created before this time
  
  // Result limit
  std::optional<int> limit;          // Maximum events to return
  
  // Convert to SQL WHERE clause
  std::string ToSqlWhereClause() const;
  
  // Parse from JSON
  static std::unique_ptr<NostrFilter> FromDict(const base::Value::Dict& dict);
};

}  // namespace local_relay
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_LOCAL_RELAY_NOSTR_DATABASE_H_