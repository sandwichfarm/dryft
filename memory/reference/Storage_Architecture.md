# Storage Architecture
## dryft browser - Multi-Store Design for Different Components

### 1. Overview

Tungsten uses different storage solutions optimized for each component's specific needs:

```
┌─────────────────────────────────────────────────────────────┐
│                    Storage Architecture                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌────────────┐ │
│  │   Local Relay   │  │  Blossom Storage │  │ Key Store  │ │
│  │   (SQLite)      │  │  (File System)   │  │ (Platform) │ │
│  └─────────────────┘  └─────────────────┘  └────────────┘ │
│                                                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌────────────┐ │
│  │  Browser State  │  │  Nsite Cache    │  │   Prefs    │ │
│  │  (LMDB)         │  │  (Memory+Disk)  │  │   (JSON)   │ │
│  └─────────────────┘  └─────────────────┘  └────────────┘ │
│                                                             │
└─────────────────────────────────────────────────────────────┘

All storage is file-based, no separate processes required:
- SQLite: Single file database with WAL mode
- LMDB: Memory-mapped single file database  
- File System: Direct file storage with sharding
- Platform Key Store: OS-provided secure storage
- JSON: Simple text files for preferences
```

### 2. Local Relay Storage (SQLite)

**Purpose**: High-performance event storage with complex querying needs

```cpp
// Why SQLite for Local Relay:
// - Complex queries with multiple filters
// - Need for indexes on multiple fields
// - Full-text search capabilities
// - Efficient range queries
// - ACID compliance for data integrity
// - Better performance for large datasets

class LocalRelayDatabase {
 public:
  LocalRelayDatabase(const base::FilePath& db_path) {
    sqlite3_open_v2(
        db_path.value().c_str(),
        &db_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    
    // Enable Write-Ahead Logging for better concurrency
    ExecuteSQL("PRAGMA journal_mode=WAL");
    ExecuteSQL("PRAGMA synchronous=NORMAL");
    
    // Create tables with proper indexes
    CreateSchema();
  }
  
 private:
  void CreateSchema() {
    // Events table optimized for Nostr queries
    ExecuteSQL(R"(
      CREATE TABLE IF NOT EXISTS events (
        id TEXT PRIMARY KEY,
        pubkey TEXT NOT NULL,
        created_at INTEGER NOT NULL,
        kind INTEGER NOT NULL,
        content TEXT,
        sig TEXT NOT NULL,
        tags_json TEXT,
        UNIQUE(id)
      );
      
      CREATE INDEX idx_events_pubkey ON events(pubkey);
      CREATE INDEX idx_events_created_at ON events(created_at DESC);
      CREATE INDEX idx_events_kind ON events(kind);
      CREATE INDEX idx_events_pubkey_kind ON events(pubkey, kind);
    )");
    
    // Separate tags table for efficient tag queries
    ExecuteSQL(R"(
      CREATE TABLE IF NOT EXISTS tags (
        event_id TEXT NOT NULL,
        tag_name TEXT NOT NULL,
        tag_value TEXT,
        tag_order INTEGER,
        FOREIGN KEY(event_id) REFERENCES events(id) ON DELETE CASCADE
      );
      
      CREATE INDEX idx_tags_event_id ON tags(event_id);
      CREATE INDEX idx_tags_name_value ON tags(tag_name, tag_value);
    )");
  }
};
```

### 3. Blossom Storage (File System)

**Purpose**: Content-addressed blob storage without database overhead

```cpp
// Why File System for Blossom:
// - Simple content-addressed storage (hash -> file)
// - No complex queries needed
// - Direct file serving capability
// - Efficient for large binary data
// - Easy cache eviction (just delete files)

class BlossomFileStorage {
 public:
  BlossomFileStorage(const base::FilePath& storage_root)
      : storage_root_(storage_root) {
    // Create directory structure
    // /blossom/
    //   /ca/fe/cafebabe123... (sharded by first 2 bytes)
    //   /de/ad/deadbeef456...
    base::CreateDirectory(storage_root_);
  }
  
  base::FilePath GetPathForHash(const std::string& hash) {
    // Shard by first 2 bytes to avoid too many files in one directory
    std::string first_byte = hash.substr(0, 2);
    std::string second_byte = hash.substr(2, 2);
    
    base::FilePath shard_dir = storage_root_
        .Append(first_byte)
        .Append(second_byte);
    
    base::CreateDirectory(shard_dir);
    return shard_dir.Append(hash);
  }
  
  bool Store(const std::string& hash, const std::vector<uint8_t>& data) {
    base::FilePath file_path = GetPathForHash(hash);
    
    // Verify hash before storing
    if (!VerifyHash(data, hash)) {
      return false;
    }
    
    // Atomic write with temp file
    base::FilePath temp_path = file_path.AddExtension(".tmp");
    if (!base::WriteFile(temp_path, data)) {
      return false;
    }
    
    return base::Move(temp_path, file_path);
  }
  
  std::unique_ptr<std::vector<uint8_t>> Retrieve(const std::string& hash) {
    base::FilePath file_path = GetPathForHash(hash);
    
    std::string contents;
    if (!base::ReadFileToString(file_path, &contents)) {
      return nullptr;
    }
    
    auto data = std::make_unique<std::vector<uint8_t>>(
        contents.begin(), contents.end());
    
    // Verify integrity
    if (!VerifyHash(*data, hash)) {
      base::DeleteFile(file_path);  // Remove corrupted file
      return nullptr;
    }
    
    return data;
  }
  
 private:
  base::FilePath storage_root_;
  
  // Simple LRU eviction when storage exceeds limit
  void EnforceStorageLimit() {
    struct FileInfo {
      base::FilePath path;
      base::Time last_accessed;
      int64_t size;
    };
    
    std::vector<FileInfo> files;
    int64_t total_size = 0;
    
    // Collect all files
    base::FileEnumerator enumerator(
        storage_root_, true, base::FileEnumerator::FILES);
    
    for (base::FilePath path = enumerator.Next(); 
         !path.empty(); 
         path = enumerator.Next()) {
      base::File::Info info;
      if (base::GetFileInfo(path, &info)) {
        files.push_back({path, info.last_accessed, info.size});
        total_size += info.size;
      }
    }
    
    // Remove oldest files if over limit
    if (total_size > max_storage_size_) {
      std::sort(files.begin(), files.end(), 
                [](const FileInfo& a, const FileInfo& b) {
                  return a.last_accessed < b.last_accessed;
                });
      
      for (const auto& file : files) {
        if (total_size <= max_storage_size_) break;
        
        base::DeleteFile(file.path);
        total_size -= file.size;
      }
    }
  }
};
```

### 4. Browser State Storage (LMDB)

**Purpose**: Fast, file-based storage for browser UI state and caches

```cpp
// Why LMDB for Browser State:
// - Memory-mapped files (no separate process needed)
// - ACID transactions with MVCC
// - Zero-copy reads
// - Crash-resistant
// - Excellent performance for read-heavy workloads
// - Single file storage (easy backup/restore)

class BrowserStateStore {
 public:
  BrowserStateStore(const base::FilePath& db_path)
      : db_path_(db_path) {
    // Create LMDB environment
    mdb_env_create(&env_);
    
    // Set max DB size (1GB should be plenty for browser state)
    mdb_env_set_mapsize(env_, 1ULL * 1024 * 1024 * 1024);
    
    // Open environment
    mdb_env_open(env_, db_path_.value().c_str(), 
                 MDB_NOSUBDIR | MDB_NOTLS, 0644);
    
    // Create named databases
    MDB_txn* txn;
    mdb_txn_begin(env_, nullptr, 0, &txn);
    
    mdb_dbi_open(txn, "ui_state", MDB_CREATE, &ui_state_db_);
    mdb_dbi_open(txn, "permissions", MDB_CREATE, &permissions_db_);
    mdb_dbi_open(txn, "relay_stats", MDB_CREATE, &relay_stats_db_);
    mdb_dbi_open(txn, "nip07_permissions", MDB_CREATE, &nip07_db_);
    
    mdb_txn_commit(txn);
  }
  
  bool SaveUIState(const std::string& key, const base::Value& value) {
    std::string json;
    base::JSONWriter::Write(value, &json);
    
    MDB_txn* txn;
    mdb_txn_begin(env_, nullptr, 0, &txn);
    
    MDB_val mkey = {key.size(), const_cast<char*>(key.data())};
    MDB_val mval = {json.size(), const_cast<char*>(json.data())};
    
    int rc = mdb_put(txn, ui_state_db_, &mkey, &mval, 0);
    
    if (rc == 0) {
      mdb_txn_commit(txn);
      return true;
    } else {
      mdb_txn_abort(txn);
      return false;
    }
  }
  
  std::unique_ptr<base::Value> GetUIState(const std::string& key) {
    MDB_txn* txn;
    mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
    
    MDB_val mkey = {key.size(), const_cast<char*>(key.data())};
    MDB_val mval;
    
    int rc = mdb_get(txn, ui_state_db_, &mkey, &mval);
    
    std::unique_ptr<base::Value> result;
    if (rc == 0) {
      std::string json(static_cast<char*>(mval.mv_data), mval.mv_size);
      result = base::JSONReader::Read(json);
    }
    
    mdb_txn_abort(txn);
    return result;
  }
  
  // Store NIP-07 permissions
  void SaveNIP07Permission(const url::Origin& origin,
                          const NIP07Permission& permission) {
    MDB_txn* txn;
    mdb_txn_begin(env_, nullptr, 0, &txn);
    
    std::string key = origin.Serialize();
    std::string value = SerializePermission(permission);
    
    MDB_val mkey = {key.size(), const_cast<char*>(key.data())};
    MDB_val mval = {value.size(), const_cast<char*>(value.data())};
    
    mdb_put(txn, nip07_db_, &mkey, &mval, 0);
    mdb_txn_commit(txn);
  }
  
 private:
  base::FilePath db_path_;
  MDB_env* env_;
  MDB_dbi ui_state_db_;
  MDB_dbi permissions_db_;
  MDB_dbi relay_stats_db_;
  MDB_dbi nip07_db_;
};
```

### 5. Key Storage (Platform-Specific)

**Purpose**: Secure storage of cryptographic keys

```cpp
// Platform-specific secure storage
// - macOS: Keychain Services
// - Windows: Credential Manager  
// - Linux: Secret Service API

// Defined in LLDD_Key_Management.md
```

### 6. Nsite Cache (Memory + Disk)

**Purpose**: Fast access to static website files with memory caching

```cpp
class NsiteCache {
 public:
  NsiteCache(const base::FilePath& cache_dir)
      : disk_cache_(cache_dir),
        memory_cache_(kMaxMemoryCacheEntries) {}
  
  void Put(const std::string& key, const FileContent& content) {
    // Store in memory cache (LRU)
    memory_cache_.Put(key, content);
    
    // Store on disk for persistence
    disk_cache_.Store(key, content);
  }
  
  std::unique_ptr<FileContent> Get(const std::string& key) {
    // Check memory cache first
    auto memory_result = memory_cache_.Get(key);
    if (memory_result) {
      return memory_result;
    }
    
    // Fall back to disk cache
    auto disk_result = disk_cache_.Retrieve(key);
    if (disk_result) {
      // Promote to memory cache
      memory_cache_.Put(key, *disk_result);
    }
    
    return disk_result;
  }
  
 private:
  static constexpr size_t kMaxMemoryCacheEntries = 1000;
  
  base::LRUCache<std::string, FileContent> memory_cache_;
  NsiteDiskCache disk_cache_;
};
```

### 7. Preferences Storage (JSON)

**Purpose**: Simple configuration storage

```cpp
// Standard Chromium preferences system
// - JSON file for persistence
// - Memory cache for fast access
// - Change notifications
// Defined in existing Chromium preference system
```

### 8. Storage Quotas and Management

```cpp
class StorageManager {
 public:
  struct StorageQuotas {
    int64_t local_relay_max = 1 * 1024 * 1024 * 1024;      // 1GB
    int64_t blossom_cache_max = 5 * 1024 * 1024 * 1024;    // 5GB
    int64_t nsite_cache_max = 500 * 1024 * 1024;           // 500MB
    int64_t total_max = 10 * 1024 * 1024 * 1024;           // 10GB
  };
  
  void EnforceQuotas() {
    int64_t total_usage = 0;
    
    // Check each component
    total_usage += GetLocalRelaySize();
    total_usage += GetBlossomCacheSize();
    total_usage += GetNsiteCacheSize();
    
    if (total_usage > quotas_.total_max) {
      // Evict in priority order
      EvictBlossomCache(total_usage - quotas_.total_max);
      EvictNsiteCache();
      // Never evict local relay data (user's own events)
    }
  }
  
 private:
  void EvictBlossomCache(int64_t bytes_needed) {
    // LRU eviction of Blossom files
    blossom_storage_->EvictLRU(bytes_needed);
  }
};
```

### 9. Migration from Other Browsers

```cpp
class StorageMigration {
 public:
  void MigrateFromExtension(ExtensionType type) {
    switch (type) {
      case ExtensionType::ALBY:
        MigrateAlbyStorage();
        break;
      case ExtensionType::NOS2X:
        MigrateNos2xStorage();
        break;
    }
  }
  
 private:
  void MigrateAlbyStorage() {
    // Alby uses extension storage API (LevelDB)
    base::FilePath alby_leveldb = GetExtensionStoragePath("alby-extension-id");
    
    // Open LevelDB
    leveldb::DB* db;
    leveldb::Status status = leveldb::DB::Open(
        leveldb::Options(), alby_leveldb.value(), &db);
    
    if (status.ok()) {
      // Read and migrate keys
      std::string value;
      status = db->Get(leveldb::ReadOptions(), "nostr_keys", &value);
      if (status.ok()) {
        ImportKeysFromJSON(value);
        
        // Also migrate permissions and relay lists
        std::string permissions;
        if (db->Get(leveldb::ReadOptions(), "permissions", &permissions).ok()) {
          MigratePermissionsToLMDB(permissions);
        }
        
        std::string relays;
        if (db->Get(leveldb::ReadOptions(), "relays", &relays).ok()) {
          MigrateRelaysToSQLite(relays);
        }
      }
      delete db;
    }
  }
};
```

### 10. Performance Characteristics

| Storage Type | Read Speed | Write Speed | Query Capability | Best For |
|-------------|------------|-------------|------------------|----------|
| SQLite (Relay) | Fast with indexes | Moderate | Complex queries | Event storage |
| File System (Blossom) | Very Fast | Fast | Hash lookup only | Binary blobs |
| LMDB (Browser) | Very Fast | Fast | Key-value lookups | UI state |
| Platform Key Store | Slow | Slow | Key lookup only | Secure keys |
| Memory Cache | Instant | Instant | Key lookup only | Hot data |
| JSON (Prefs) | Fast | Moderate | No queries | Settings |

### Key Design Decisions

1. **SQLite for Local Relay**: Chosen for complex Nostr filter queries
2. **File System for Blossom**: Simple and efficient for content-addressed storage
3. **No Database for Blossom**: Reduces complexity, file system is sufficient
4. **LMDB for Browser State**: Memory-mapped files, no separate process needed
5. **Platform Storage for Keys**: Maximum security for private keys
6. **Hybrid Caching**: Memory + disk for optimal performance
7. **All File-Based**: No separate database processes to manage