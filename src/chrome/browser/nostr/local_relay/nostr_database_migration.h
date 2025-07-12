// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_LOCAL_RELAY_NOSTR_DATABASE_MIGRATION_H_
#define CHROME_BROWSER_NOSTR_LOCAL_RELAY_NOSTR_DATABASE_MIGRATION_H_

#include <map>
#include <functional>

#include "sql/database.h"

namespace nostr {
namespace local_relay {

// Manages database schema migrations for the Nostr local relay database
class NostrDatabaseMigration {
 public:
  // Migration function type
  using MigrationFunction = std::function<bool(sql::Database*)>;
  
  // Register all migrations
  static void RegisterMigrations();
  
  // Run migrations from current version to target version
  static bool RunMigrations(sql::Database* db, 
                          int current_version, 
                          int target_version);

 private:
  // Individual migration functions
  
  // Future migrations would be added here as static methods
  // Example:
  // static bool MigrateV1ToV2(sql::Database* db);
  // static bool MigrateV2ToV3(sql::Database* db);
  
  // Map of version transitions to migration functions
  static std::map<std::pair<int, int>, MigrationFunction> migrations_;
};

}  // namespace local_relay
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_LOCAL_RELAY_NOSTR_DATABASE_MIGRATION_H_