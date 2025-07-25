// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_LOCAL_RELAY_NOSTR_DATABASE_SCHEMA_H_
#define CHROME_BROWSER_NOSTR_LOCAL_RELAY_NOSTR_DATABASE_SCHEMA_H_

#include <string>

namespace nostr {
namespace local_relay {

// SQLite database schema for Nostr local relay storage
// Following NIP-01 event structure and optimized for common query patterns
class NostrDatabaseSchema {
 public:
  // Current schema version for migration tracking
  static constexpr int kCurrentVersion = 1;

  // Main events table schema
  // Stores complete Nostr events with denormalized fields for fast queries
  static constexpr char kCreateEventsTable[] = R"(
    CREATE TABLE IF NOT EXISTS events (
      -- Event identification
      id TEXT PRIMARY KEY,              -- Event ID (32-byte SHA256 hex)
      pubkey TEXT NOT NULL,             -- Author's public key (32-byte hex)
      
      -- Event metadata
      created_at INTEGER NOT NULL,      -- Unix timestamp
      kind INTEGER NOT NULL,            -- Event kind number
      
      -- Event content
      content TEXT NOT NULL,            -- Event content (can be empty string)
      sig TEXT NOT NULL,                -- Schnorr signature (64-byte hex)
      
      -- Denormalized fields for queries
      deleted INTEGER DEFAULT 0,        -- Soft delete flag
      expires_at INTEGER,               -- Optional expiration timestamp
      
      -- Timestamps for local relay management
      received_at INTEGER NOT NULL,     -- When relay received the event
      updated_at INTEGER NOT NULL       -- Last modification time
    );
  )";

  // Tags table for efficient tag queries
  // Normalized storage of event tags for filtering
  static constexpr char kCreateTagsTable[] = R"(
    CREATE TABLE IF NOT EXISTS tags (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      event_id TEXT NOT NULL,           -- Foreign key to events.id
      tag_name TEXT NOT NULL,           -- Tag name (e.g., 'e', 'p', 'a')
      tag_value TEXT NOT NULL,          -- First value in tag array
      tag_full TEXT NOT NULL,           -- Full JSON array as text
      
      FOREIGN KEY (event_id) REFERENCES events(id) ON DELETE CASCADE
    );
  )";

  // Deletions table for NIP-09 event deletion
  // Tracks deletion events separately for efficient handling
  static constexpr char kCreateDeletionsTable[] = R"(
    CREATE TABLE IF NOT EXISTS deletions (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      deletion_event_id TEXT NOT NULL,  -- The kind:5 deletion event ID
      deleted_event_id TEXT NOT NULL,   -- The event being deleted
      deleted_at INTEGER NOT NULL,      -- Timestamp of deletion
      
      UNIQUE(deletion_event_id, deleted_event_id)
    );
  )";

  // Replaceable events table for NIP-16
  // Tracks the latest version of replaceable events (kinds 0, 3, 10000-19999)
  static constexpr char kCreateReplaceableEventsTable[] = R"(
    CREATE TABLE IF NOT EXISTS replaceable_events (
      pubkey TEXT NOT NULL,
      kind INTEGER NOT NULL,
      d_tag TEXT NOT NULL DEFAULT '',   -- 'd' tag value for parameterized replaceable
      current_event_id TEXT NOT NULL,   -- Current active event ID
      
      PRIMARY KEY (pubkey, kind, d_tag),
      FOREIGN KEY (current_event_id) REFERENCES events(id) ON DELETE CASCADE
    );
  )";

  // Subscriptions table for active REQ subscriptions
  // Tracks client subscriptions for real-time updates
  static constexpr char kCreateSubscriptionsTable[] = R"(
    CREATE TABLE IF NOT EXISTS subscriptions (
      id TEXT PRIMARY KEY,              -- Subscription ID from client
      connection_id TEXT NOT NULL,      -- WebSocket connection identifier
      filters TEXT NOT NULL,            -- JSON array of filters
      created_at INTEGER NOT NULL,      -- Subscription creation time
      active INTEGER DEFAULT 1          -- Active flag
    );
  )";

  // Indexes for common query patterns
  
  // Primary query indexes
  static constexpr char kCreateIndexEventsPubkey[] = 
      "CREATE INDEX IF NOT EXISTS idx_events_pubkey ON events(pubkey);";
  
  static constexpr char kCreateIndexEventsKind[] = 
      "CREATE INDEX IF NOT EXISTS idx_events_kind ON events(kind);";
  
  static constexpr char kCreateIndexEventsCreatedAt[] = 
      "CREATE INDEX IF NOT EXISTS idx_events_created_at ON events(created_at DESC);";
  
  // Composite indexes for common filter combinations
  static constexpr char kCreateIndexEventsKindCreatedAt[] = 
      "CREATE INDEX IF NOT EXISTS idx_events_kind_created_at ON events(kind, created_at DESC);";
  
  static constexpr char kCreateIndexEventsPubkeyKind[] = 
      "CREATE INDEX IF NOT EXISTS idx_events_pubkey_kind ON events(pubkey, kind);";
  
  static constexpr char kCreateIndexEventsPubkeyCreatedAt[] = 
      "CREATE INDEX IF NOT EXISTS idx_events_pubkey_created_at ON events(pubkey, created_at DESC);";
  
  // Tag query indexes
  static constexpr char kCreateIndexTagsEventId[] = 
      "CREATE INDEX IF NOT EXISTS idx_tags_event_id ON tags(event_id);";
  
  static constexpr char kCreateIndexTagsNameValue[] = 
      "CREATE INDEX IF NOT EXISTS idx_tags_name_value ON tags(tag_name, tag_value);";
  
  // Deletion tracking index
  static constexpr char kCreateIndexDeletionsDeletedEventId[] = 
      "CREATE INDEX IF NOT EXISTS idx_deletions_deleted_event_id ON deletions(deleted_event_id);";
  
  // Replaceable events index
  static constexpr char kCreateIndexReplaceableCurrentEventId[] = 
      "CREATE INDEX IF NOT EXISTS idx_replaceable_current_event_id ON replaceable_events(current_event_id);";

  // Database metadata table for version tracking and stats
  static constexpr char kCreateMetadataTable[] = R"(
    CREATE TABLE IF NOT EXISTS metadata (
      key TEXT PRIMARY KEY,
      value TEXT NOT NULL
    );
  )";

  // Initial metadata values
  static constexpr char kInsertSchemaVersion[] = 
      "INSERT OR REPLACE INTO metadata (key, value) VALUES ('schema_version', '1');";
  
  static constexpr char kInsertCreatedAt[] = 
      "INSERT OR REPLACE INTO metadata (key, value) VALUES ('created_at', strftime('%s', 'now'));";

  // Helper queries

  // Check if event exists
  static constexpr char kCheckEventExists[] = 
      "SELECT 1 FROM events WHERE id = ? LIMIT 1;";

  // Get event by ID
  static constexpr char kGetEventById[] = 
      "SELECT * FROM events WHERE id = ? AND deleted = 0;";

  // Mark event as deleted
  static constexpr char kMarkEventDeleted[] = 
      "UPDATE events SET deleted = 1, updated_at = ? WHERE id = ?;";

  // Get latest replaceable event
  static constexpr char kGetLatestReplaceableEvent[] = R"(
    SELECT e.* FROM events e
    INNER JOIN replaceable_events r ON e.id = r.current_event_id
    WHERE r.pubkey = ? AND r.kind = ? AND r.d_tag = ?
    AND e.deleted = 0;
  )";

  // Count total events
  static constexpr char kCountTotalEvents[] = 
      "SELECT COUNT(*) FROM events WHERE deleted = 0;";

  // Get database size
  static constexpr char kGetDatabaseSize[] = 
      "SELECT page_count * page_size FROM pragma_page_count(), pragma_page_size();";

  // Vacuum database (maintenance)
  static constexpr char kVacuumDatabase[] = "VACUUM;";

  // Analyze database (optimize query planner)
  static constexpr char kAnalyzeDatabase[] = "ANALYZE;";
};

}  // namespace local_relay
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_LOCAL_RELAY_NOSTR_DATABASE_SCHEMA_H_