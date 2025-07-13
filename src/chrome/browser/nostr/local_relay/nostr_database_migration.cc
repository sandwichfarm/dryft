// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/nostr_database_migration.h"

#include "base/logging.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace nostr {
namespace local_relay {

// Static member initialization
std::map<std::pair<int, int>, NostrDatabaseMigration::MigrationFunction> 
    NostrDatabaseMigration::migrations_;

// static
void NostrDatabaseMigration::RegisterMigrations() {
  // Register migrations here as the schema evolves
  // Example:
  // migrations_[{1, 2}] = &MigrateV1ToV2;
  // migrations_[{2, 3}] = &MigrateV2ToV3;
  
  // Currently we're at version 1, so no migrations yet
}

// static
bool NostrDatabaseMigration::RunMigrations(sql::Database* db,
                                         int current_version,
                                         int target_version) {
  if (current_version >= target_version) {
    return true;  // No migration needed
  }
  
  LOG(INFO) << "Migrating Nostr database from version " << current_version
            << " to version " << target_version;
  
  // Ensure migrations are registered
  if (migrations_.empty()) {
    RegisterMigrations();
  }
  
  // Run migrations in sequence
  for (int from = current_version; from < target_version; ++from) {
    int to = from + 1;
    
    auto it = migrations_.find({from, to});
    if (it == migrations_.end()) {
      LOG(ERROR) << "No migration path from version " << from 
                 << " to version " << to;
      return false;
    }
    
    sql::Transaction transaction(db);
    if (!transaction.Begin()) {
      LOG(ERROR) << "Failed to begin migration transaction";
      return false;
    }
    
    if (!it->second(db)) {
      LOG(ERROR) << "Migration from version " << from 
                 << " to version " << to << " failed";
      return false;
    }
    
    // Update version number
    sql::Statement update_version(db->GetUniqueStatement(
        "UPDATE metadata SET value = ? WHERE key = 'schema_version'"));
    update_version.BindString(0, base::NumberToString(to));
    
    if (!update_version.Run()) {
      LOG(ERROR) << "Failed to update schema version";
      return false;
    }
    
    if (!transaction.Commit()) {
      LOG(ERROR) << "Failed to commit migration transaction";
      return false;
    }
    
    LOG(INFO) << "Successfully migrated to version " << to;
  }
  
  return true;
}

// Example migration function for future use:
// 
// static
// bool NostrDatabaseMigration::MigrateV1ToV2(sql::Database* db) {
//   // Add new column example
//   if (!db->Execute("ALTER TABLE events ADD COLUMN new_field TEXT")) {
//     return false;
//   }
//   
//   // Create new index example
//   if (!db->Execute("CREATE INDEX idx_events_new_field ON events(new_field)")) {
//     return false;
//   }
//   
//   return true;
// }

}  // namespace local_relay
}  // namespace nostr