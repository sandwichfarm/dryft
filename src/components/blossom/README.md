# Blossom Storage Implementation

This directory contains the core storage implementation for the Blossom protocol in Tungsten browser.

## Overview

Blossom is a content-addressed storage protocol where files are identified by their SHA256 hash. This implementation provides:

- Content-addressed file storage with SHA256 verification
- Sharded directory structure for scalability
- Atomic writes using temporary files
- Metadata tracking (access time, count, MIME type)
- LRU eviction policy (when enabled)
- Storage quota management

## Architecture

### BlossomStorage Class

The main storage class that handles:
- Storing and retrieving content by SHA256 hash
- Hash verification on store and retrieve
- MIME type detection
- Metadata management
- Storage statistics

### Storage Structure

Files are stored in a sharded directory structure to avoid filesystem limitations:

```
/blossom/
  /ca/
    /fe/
      /cafebabe123... (content file)
      /cafebabe123....meta (metadata file)
```

The sharding depth is configurable (1-4 levels).

### Configuration

```cpp
StorageConfig config;
config.root_path = base::FilePath("/path/to/storage");
config.max_total_size = 1024 * 1024 * 1024;  // 1GB
config.max_blob_size = 10 * 1024 * 1024;     // 10MB
config.shard_depth = 2;                       // Use 2 levels of sharding
config.enable_lru_eviction = true;            // Enable automatic cleanup
```

## Usage

```cpp
// Create storage instance
auto storage = base::MakeRefCounted<BlossomStorage>(config);

// Initialize
storage->Initialize(base::BindOnce([](bool success) {
  LOG(INFO) << "Storage initialized: " << success;
}));

// Store content
std::string data = "Hello, Blossom!";
std::string hash = CalculateSHA256(data);
storage->StoreContent(hash, data, "text/plain", 
    base::BindOnce([](bool success, const std::string& error) {
      if (!success) {
        LOG(ERROR) << "Store failed: " << error;
      }
    }));

// Retrieve content
storage->GetContent(hash,
    base::BindOnce([](bool success, 
                     const std::string& data,
                     const std::string& mime_type) {
      if (success) {
        LOG(INFO) << "Retrieved " << data.size() << " bytes";
      }
    }));
```

## Features

### Hash Verification
- All content is verified against its SHA256 hash on store
- Content integrity is checked on retrieval
- Invalid hashes are rejected immediately

### Atomic Writes
- Content is first written to a temporary file
- Then atomically moved to the final location
- Prevents corruption from partial writes

### Metadata Tracking
Each blob has associated metadata:
- Creation time
- Last access time
- Access count
- MIME type
- File size

### LRU Eviction
When storage is full:
- Least recently used blobs are evicted
- Based on access time and frequency
- Configurable minimum free space

## Testing

Unit tests are provided in `blossom_storage_unittest.cc` covering:
- Basic store/retrieve operations
- Hash verification
- Invalid inputs
- Size limits
- MIME type detection
- Metadata operations
- Statistics tracking

## Future Enhancements

1. **Compression**: Add optional compression for stored blobs
2. **Encryption**: Support for encrypted storage
3. **Remote Sync**: Sync with external Blossom servers
4. **Performance**: Add caching layer for frequently accessed blobs
5. **Monitoring**: Add metrics and performance tracking

## Integration

This storage component will be used by:
- `BlossomServerManager`: HTTP server for Blossom protocol
- `BlossomContentHandler`: Handles content requests
- `BlossomUIHandler`: Management UI for stored content

See the parent directory's design documents for full integration details.