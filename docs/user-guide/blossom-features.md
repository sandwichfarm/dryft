# Blossom File Sharing

Learn how to use Tungsten's built-in Blossom server for content-addressed file storage and sharing.

## What is Blossom?

Blossom is a content-addressed file storage protocol for Nostr that provides:

- **Content Addressing**: Files identified by SHA256 hash
- **Decentralized Storage**: No single point of failure
- **Integrity Verification**: Content cannot be tampered with
- **Efficient Sharing**: Deduplication across the network

## Tungsten's Blossom Integration

Tungsten includes a built-in Blossom server and client:

- **Local Server**: Runs on `http://localhost:8080` 
- **File Storage**: Direct file system storage with sharding
- **Web API**: Complete `window.blossom` JavaScript API
- **Network Integration**: Sync with remote Blossom servers

## Basic File Operations

### Uploading Files

**Using the Web API**:
```javascript
// Upload a file
const fileInput = document.getElementById('file-input');
const file = fileInput.files[0];

try {
  const hash = await window.blossom.upload(file, {
    type: file.type,
    alt: 'Profile picture',
    caption: 'My new avatar'
  });
  
  console.log('File uploaded:', hash);
  console.log('Access URL:', `http://localhost:8080/files/${hash}`);
} catch (error) {
  console.error('Upload failed:', error);
}
```

**Upload from URL**:
```javascript
// Upload from URL
async function uploadFromURL(url) {
  const response = await fetch(url);
  const blob = await response.blob();
  
  const hash = await window.blossom.upload(blob, {
    type: response.headers.get('content-type'),
    source: url
  });
  
  return hash;
}
```

### Downloading Files

**Download by Hash**:
```javascript
// Download and display a file
async function displayFile(hash) {
  try {
    const blob = await window.blossom.get(hash);
    const url = URL.createObjectURL(blob);
    
    // Display image
    const img = document.createElement('img');
    img.src = url;
    document.body.appendChild(img);
    
    // Or create download link
    const a = document.createElement('a');
    a.href = url;
    a.download = `file-${hash.slice(0, 8)}.${getFileExtension(blob.type)}`;
    a.click();
  } catch (error) {
    console.error('Download failed:', error);
  }
}
```

**Direct HTTP Access**:
```javascript
// Access files directly via HTTP
const imageHash = 'a1b2c3d4e5f6789...';
const imageURL = `http://localhost:8080/files/${imageHash}`;

// Use in HTML
const img = document.createElement('img');
img.src = imageURL;
```

### File Management

**List Your Files**:
```javascript
// List recent uploads
const files = await window.blossom.list(50);
files.forEach(file => {
  console.log(`${file.hash}: ${file.size} bytes, ${file.type}`);
  console.log(`Uploaded: ${new Date(file.uploaded)}`);
  console.log(`Last accessed: ${new Date(file.accessed)}`);
});
```

**Delete Files**:
```javascript
// Delete a file
const deleted = await window.blossom.delete(hash);
if (deleted) {
  console.log('File deleted successfully');
} else {
  console.log('File not found or cannot be deleted');
}
```

## Advanced Features

### File Mirroring

**Mirror to Remote Servers**:
```javascript
// Mirror file to multiple servers for redundancy
const remoteServers = [
  'https://blossom.primal.net',
  'https://blossom.nostr.band',
  'https://blossom-cdn.com'
];

await window.blossom.mirror(hash, remoteServers);
console.log('File mirrored to remote servers');
```

**Automatic Mirroring**:
```javascript
// Configure automatic mirroring for important files
window.blossom.setAutoMirror({
  enabled: true,
  servers: remoteServers,
  fileTypes: ['image/*', 'video/*'], // Mirror media files
  minSize: 1024 * 1024, // Only files > 1MB
  maxSize: 100 * 1024 * 1024 // Up to 100MB
});
```

### Metadata and Tags

**Rich Metadata**:
```javascript
// Upload with comprehensive metadata
const hash = await window.blossom.upload(file, {
  type: file.type,
  alt: 'Description for screen readers',
  caption: 'Human-readable caption',
  tags: ['profile', 'avatar', 'nostr'],
  license: 'CC-BY-4.0',
  creator: 'npub1abc123...',
  created: Date.now(),
  location: 'San Francisco, CA',
  camera: 'iPhone 14 Pro',
  custom: {
    project: 'nsite-gallery',
    version: '1.0'
  }
});
```

**Search by Metadata**:
```javascript
// Find files by tags or metadata
const taggedFiles = await window.blossom.search({
  tags: ['profile'],
  type: 'image/*',
  creator: await window.nostr.getPublicKey(),
  limit: 20
});
```

### Content Types and Processing

**Supported File Types**:
```javascript
// Check supported formats
const supportedTypes = window.blossom.supportedTypes;
console.log('Supported types:', supportedTypes);

// Common supported formats:
// - Images: JPEG, PNG, GIF, WebP, SVG, HEIC
// - Video: MP4, WebM, MOV, AVI
// - Audio: MP3, WAV, OGG, M4A
// - Documents: PDF, TXT, JSON, CSV
// - Archives: ZIP, TAR, GZ
// - Any binary file
```

**Image Processing**:
```javascript
// Upload with automatic processing
const hash = await window.blossom.upload(imageFile, {
  type: 'image/jpeg',
  processing: {
    generateThumbnail: true,
    maxWidth: 2048,
    maxHeight: 2048,
    quality: 0.85,
    formats: ['webp', 'jpeg'] // Generate multiple formats
  }
});

// Access processed versions
const thumbnail = await window.blossom.get(`${hash}-thumb`);
const webp = await window.blossom.get(`${hash}.webp`);
```

## Integration with Nostr

### Profile Pictures and Media

**Upload Profile Picture**:
```javascript
async function updateProfilePicture(imageFile) {
  // Upload to Blossom
  const hash = await window.blossom.upload(imageFile, {
    type: imageFile.type,
    alt: 'Profile picture',
    tags: ['profile', 'avatar']
  });
  
  // Create Blossom URL
  const imageURL = `blossom://${hash}`;
  
  // Update Nostr profile
  const profileEvent = {
    kind: 0,
    created_at: Math.floor(Date.now() / 1000),
    tags: [],
    content: JSON.stringify({
      name: 'Your Name',
      about: 'Your bio',
      picture: imageURL, // Use Blossom URL
      banner: await getPreviousBanner() // Keep existing banner
    })
  };
  
  await window.nostr.signEvent(profileEvent);
  // Publish to relays...
}
```

**Content in Notes**:
```javascript
// Include media in Nostr notes
async function postNoteWithImage(text, imageFile) {
  // Upload image
  const hash = await window.blossom.upload(imageFile, {
    type: imageFile.type,
    alt: 'Attached image'
  });
  
  // Create note with image reference
  const noteEvent = {
    kind: 1,
    created_at: Math.floor(Date.now() / 1000),
    tags: [
      ['imeta', `url blossom://${hash}`, `x ${hash}`, `m ${imageFile.type}`]
    ],
    content: `${text}\n\nblossom://${hash}`
  };
  
  await window.nostr.signEvent(noteEvent);
}
```

### Nsite Assets

**Static Site Assets**:
```javascript
// Upload website assets for Nsite
async function uploadSiteAssets(files) {
  const hashes = {};
  
  for (const [path, file] of Object.entries(files)) {
    const hash = await window.blossom.upload(file, {
      type: file.type,
      tags: ['nsite', 'asset'],
      path: path // Original file path
    });
    
    hashes[path] = hash;
  }
  
  return hashes;
}

// Use in Nsite HTML
const assetHashes = await uploadSiteAssets({
  'style.css': cssFile,
  'script.js': jsFile,
  'logo.png': logoFile
});

const html = `
<!DOCTYPE html>
<html>
<head>
  <link rel="stylesheet" href="blossom://${assetHashes['style.css']}">
</head>
<body>
  <img src="blossom://${assetHashes['logo.png']}" alt="Logo">
  <script src="blossom://${assetHashes['script.js']}"></script>
</body>
</html>
`;
```

## Storage Management

### Local Storage Configuration

**Check Storage Usage**:
```javascript
// Monitor storage usage
const stats = await window.blossom.getStats();
console.log('Storage used:', stats.storageUsed, 'bytes');
console.log('Files stored:', stats.fileCount);
console.log('Available space:', stats.availableSpace, 'bytes');
```

**Storage Limits**:
```javascript
// Configure storage limits
await window.blossom.setLimits({
  maxStorage: 10 * 1024 * 1024 * 1024, // 10GB
  maxFileSize: 100 * 1024 * 1024,      // 100MB per file
  maxFiles: 10000,                      // Maximum number of files
  cleanupPolicy: 'lru'                  // Least recently used cleanup
});
```

### Cleanup and Optimization

**Manual Cleanup**:
```javascript
// Remove old or unused files
await window.blossom.cleanup({
  olderThan: 30 * 24 * 60 * 60 * 1000, // 30 days
  unusedFor: 7 * 24 * 60 * 60 * 1000,  // Not accessed for 7 days
  minFreeSpace: 1024 * 1024 * 1024      // Keep 1GB free
});
```

**Automatic Optimization**:
```javascript
// Enable automatic cleanup
window.blossom.setAutoCleanup({
  enabled: true,
  schedule: 'daily',
  keepDays: 90,
  minFreeSpace: 2 * 1024 * 1024 * 1024 // 2GB
});
```

## Security and Privacy

### Access Control

**File Permissions**:
```javascript
// Upload private file (only accessible locally)
const hash = await window.blossom.upload(file, {
  type: file.type,
  visibility: 'private', // Not shared with remote servers
  encryption: 'local'     // Encrypted with local key
});

// Upload public file (can be mirrored)
const publicHash = await window.blossom.upload(file, {
  type: file.type,
  visibility: 'public',
  allowMirroring: true
});
```

**Content Verification**:
```javascript
// Verify file integrity
const isValid = await window.blossom.verify(hash);
if (!isValid) {
  console.warn('File integrity check failed!');
}

// Check file source
const metadata = await window.blossom.getMetadata(hash);
console.log('Uploaded by:', metadata.uploader);
console.log('Original hash:', metadata.originalHash);
```

### Privacy Features

**Anonymous Uploads**:
```javascript
// Upload without associating with Nostr identity
const hash = await window.blossom.upload(file, {
  type: file.type,
  anonymous: true, // Don't include uploader info
  noMetrics: true  // Don't track access
});
```

**Temporary Files**:
```javascript
// Upload temporary file with auto-deletion
const hash = await window.blossom.upload(file, {
  type: file.type,
  ttl: 24 * 60 * 60 * 1000, // Delete after 24 hours
  maxAccess: 10              // Delete after 10 downloads
});
```

## Performance Optimization

### Efficient Uploads

**Chunked Uploads**:
```javascript
// Upload large files in chunks for better performance
async function uploadLargeFile(file) {
  const chunkSize = 1024 * 1024; // 1MB chunks
  const chunks = Math.ceil(file.size / chunkSize);
  
  const hashes = [];
  for (let i = 0; i < chunks; i++) {
    const start = i * chunkSize;
    const end = Math.min(start + chunkSize, file.size);
    const chunk = file.slice(start, end);
    
    const hash = await window.blossom.upload(chunk, {
      type: 'application/octet-stream',
      part: i,
      totalParts: chunks,
      parentFile: file.name
    });
    
    hashes.push(hash);
  }
  
  // Combine chunks on server
  return await window.blossom.combine(hashes, {
    type: file.type,
    name: file.name
  });
}
```

**Parallel Operations**:
```javascript
// Upload multiple files in parallel
async function uploadMultipleFiles(files) {
  const uploads = files.map(file => 
    window.blossom.upload(file, { type: file.type })
  );
  
  const hashes = await Promise.all(uploads);
  return hashes;
}
```

### Caching Strategies

**Smart Caching**:
```javascript
// Configure caching behavior
window.blossom.setCaching({
  enabled: true,
  strategy: 'adaptive',     // Adaptive based on usage
  maxCacheSize: 1024 * 1024 * 1024, // 1GB cache
  prefetchPopular: true,    // Prefetch frequently accessed files
  backgroundSync: true      // Sync in background
});
```

## Troubleshooting

### Common Issues

**Upload Failures**:
```javascript
// Debug upload issues
try {
  const hash = await window.blossom.upload(file);
} catch (error) {
  switch (error.code) {
    case 'FILE_TOO_LARGE':
      console.error('File exceeds size limit');
      break;
    case 'STORAGE_FULL':
      console.error('Local storage is full');
      break;
    case 'NETWORK_ERROR':
      console.error('Network connection failed');
      break;
    case 'PERMISSION_DENIED':
      console.error('Upload permission denied');
      break;
    default:
      console.error('Unknown upload error:', error);
  }
}
```

**Performance Issues**:
```javascript
// Monitor performance
const metrics = await window.blossom.getMetrics();
console.log('Average upload speed:', metrics.avgUploadSpeed, 'bytes/sec');
console.log('Average download speed:', metrics.avgDownloadSpeed, 'bytes/sec');
console.log('Cache hit rate:', metrics.cacheHitRate);
```

### Debug Information

**Server Status**:
```javascript
// Check Blossom server status
const status = await window.blossom.getServerStatus();
console.log('Server running:', status.running);
console.log('Server port:', status.port);
console.log('Storage path:', status.storagePath);
console.log('Available space:', status.availableSpace);
```

**Network Connectivity**:
```javascript
// Test connectivity to remote servers
const servers = ['https://blossom.primal.net', 'https://blossom.nostr.band'];
for (const server of servers) {
  try {
    const ping = await window.blossom.ping(server);
    console.log(`${server}: ${ping}ms`);
  } catch (error) {
    console.error(`${server}: unreachable`);
  }
}
```

## Best Practices

### File Organization

**Naming Conventions**:
```javascript
// Use descriptive metadata for better organization
const hash = await window.blossom.upload(file, {
  type: file.type,
  alt: 'Descriptive alt text',
  tags: ['category', 'subcategory', 'specific-tag'],
  project: 'my-website',
  version: '1.0',
  created: Date.now()
});
```

**Backup Strategy**:
```javascript
// Regular backup of important files
async function backupImportantFiles() {
  const importantFiles = await window.blossom.list({
    tags: ['important', 'backup'],
    limit: 1000
  });
  
  const backupServers = [
    'https://backup1.example.com',
    'https://backup2.example.com'
  ];
  
  for (const file of importantFiles) {
    await window.blossom.mirror(file.hash, backupServers);
  }
}
```

### Security Best Practices

1. **Verify File Sources**: Always check metadata for uploaded files
2. **Use Private Mode**: For sensitive files, use private visibility
3. **Regular Cleanup**: Remove old or unused files regularly
4. **Monitor Usage**: Keep track of storage usage and access patterns
5. **Backup Important Data**: Mirror critical files to multiple servers

For more advanced integration patterns, see the [Developer Documentation](../developer/README.md).