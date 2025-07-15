// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_EXTENSION_MIGRATION_SERVICE_H_
#define CHROME_BROWSER_NOSTR_EXTENSION_MIGRATION_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class Value;
class FilePath;
}  // namespace base

namespace nostr {

// Represents a detected Nostr extension
struct DetectedExtension {
  enum Type {
    UNKNOWN = 0,
    ALBY = 1,
    NOS2X = 2,
    NOSTR_CONNECT = 3,
    FLAMINGO = 4,
  };
  
  Type type;
  std::string id;
  std::string name;
  std::string version;
  base::FilePath storage_path;
  bool is_enabled;
};

// Migration data structure
struct MigrationData {
  // Keys to import
  struct KeyData {
    std::string name;
    std::string private_key_hex;
    bool is_default;
  };
  std::vector<KeyData> keys;
  
  // Relay list
  std::vector<std::string> relay_urls;
  
  // Permissions
  struct PermissionData {
    std::string origin;
    std::vector<std::string> allowed_methods;
  };
  std::vector<PermissionData> permissions;
  
  // Whether data was successfully read
  bool success = false;
  std::string error_message;
};

// Progress callback for migration
using MigrationProgressCallback = base::RepeatingCallback<void(
    int items_completed, int total_items, const std::string& current_item)>;

// Result callback for migration
using MigrationResultCallback = base::OnceCallback<void(
    bool success, const std::string& message)>;

// Service for migrating data from other Nostr extensions
class ExtensionMigrationService : public KeyedService {
 public:
  explicit ExtensionMigrationService(Profile* profile);
  ~ExtensionMigrationService() override;
  
  // Detect installed Nostr extensions
  std::vector<DetectedExtension> DetectInstalledExtensions();
  
  // Read migration data from an extension
  MigrationData ReadExtensionData(const DetectedExtension& extension);
  
  // Perform migration
  void MigrateFromExtension(const DetectedExtension& extension,
                           const MigrationData& data,
                           MigrationProgressCallback progress_callback,
                           MigrationResultCallback result_callback);
  
  // Check if an extension should be disabled after migration
  bool ShouldDisableExtension(const DetectedExtension& extension) const;
  
  // Disable an extension after successful migration
  void DisableExtension(const DetectedExtension& extension);
  
  // KeyedService implementation
  void Shutdown() override;
  
 private:
  // Extension-specific readers
  MigrationData ReadAlbyData(const base::FilePath& storage_path);
  MigrationData ReadNos2xData(const base::FilePath& storage_path);
  MigrationData ReadNostrConnectData(const base::FilePath& storage_path);
  MigrationData ReadFlamingoData(const base::FilePath& storage_path);
  
  // Import helpers
  void ImportKeys(const std::vector<MigrationData::KeyData>& keys,
                 MigrationProgressCallback progress_callback);
  void ImportRelays(const std::vector<std::string>& relay_urls);
  void ImportPermissions(const std::vector<MigrationData::PermissionData>& permissions);
  
  // LevelDB reader for extension storage
  std::unique_ptr<base::Value> ReadExtensionLevelDB(
      const base::FilePath& storage_path,
      const std::string& key);
  
  // Profile for context
  Profile* profile_;
  
  // Weak pointer factory
  base::WeakPtrFactory<ExtensionMigrationService> weak_factory_{this};
  
  DISALLOW_COPY_AND_ASSIGN(ExtensionMigrationService);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_EXTENSION_MIGRATION_SERVICE_H_