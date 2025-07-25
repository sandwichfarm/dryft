# API Reference

Complete reference for dryft browser's enhanced Nostr and Blossom APIs.

## window.nostr API

dryft implements the complete NIP-07 specification plus dryft-specific extensions.

### Standard NIP-07 Methods

#### getPublicKey()

Returns the public key of the current user.

```typescript
getPublicKey(): Promise<string>
```

**Returns**: Promise that resolves to a hex-encoded public key (64 characters)

**Example**:
```javascript
const pubkey = await window.nostr.getPublicKey();
console.log('User pubkey:', pubkey); // "a1b2c3d4e5f6..."
```

**Errors**:
- `UserDeniedError`: User rejected the permission request
- `NoAccountError`: No account is currently active

---

#### signEvent()

Signs a Nostr event with the current user's private key.

```typescript
signEvent(event: UnsignedEvent): Promise<SignedEvent>
```

**Parameters**:
- `event`: Unsigned Nostr event object

**Returns**: Promise that resolves to a signed event with `id`, `pubkey`, and `sig` fields added

**Example**:
```javascript
const unsignedEvent = {
  kind: 1,
  created_at: Math.floor(Date.now() / 1000),
  tags: [['t', 'hello']],
  content: 'Hello Nostr!'
};

const signedEvent = await window.nostr.signEvent(unsignedEvent);
console.log('Signed event:', signedEvent);
// Output: { kind: 1, created_at: 1672531200, tags: [['t', 'hello']], 
//          content: 'Hello Nostr!', id: '...', pubkey: '...', sig: '...' }
```

**Errors**:
- `UserDeniedError`: User rejected the signing request
- `InvalidEventError`: Event format is invalid
- `RateLimitError`: Too many signing requests

---

#### getRelays()

Returns the user's relay configuration.

```typescript
getRelays(): Promise<{[url: string]: {read: boolean, write: boolean}}>
```

**Returns**: Promise that resolves to relay configuration object

**Example**:
```javascript
const relays = await window.nostr.getRelays();
console.log('User relays:', relays);
// Output: {
//   'wss://relay.damus.io': { read: true, write: true },
//   'wss://nos.lol': { read: true, write: false }
// }
```

---

#### nip04 (Legacy Encryption)

Legacy encryption methods for backward compatibility.

```typescript
nip04: {
  encrypt(pubkey: string, plaintext: string): Promise<string>;
  decrypt(pubkey: string, ciphertext: string): Promise<string>;
}
```

**encrypt()**:
- `pubkey`: Recipient's hex public key
- `plaintext`: Message to encrypt
- Returns: Base64-encoded encrypted message

**decrypt()**:
- `pubkey`: Sender's hex public key  
- `ciphertext`: Base64-encoded encrypted message
- Returns: Decrypted plaintext message

**Example**:
```javascript
// Encrypt message
const encrypted = await window.nostr.nip04.encrypt(
  'a1b2c3d4e5f6...', // recipient pubkey
  'Hello secret message!'
);

// Decrypt message
const decrypted = await window.nostr.nip04.decrypt(
  'a1b2c3d4e5f6...', // sender pubkey
  encrypted
);
```

### dryft Extensions

#### relay

Local relay integration for enhanced performance.

```typescript
relay: {
  readonly url: string;
  readonly connected: boolean;
  readonly eventCount: number;
  readonly storageUsed: number;
  query(filter: NostrFilter): Promise<NostrEvent[]>;
  count(filter: NostrFilter): Promise<number>;
  deleteEvents(filter: NostrFilter): Promise<number>;
}
```

**Properties**:
- `url`: WebSocket URL of local relay (e.g., "ws://localhost:8081")
- `connected`: Boolean indicating connection status
- `eventCount`: Total number of events stored locally
- `storageUsed`: Storage space used in bytes

**query()**:
Query events from the local relay.

```javascript
const filter = {
  kinds: [1],
  authors: ['a1b2c3d4...'],
  limit: 20
};

const events = await window.nostr.relay.query(filter);
console.log(`Found ${events.length} events`);
```

**count()**:
Count events matching a filter without fetching them.

```javascript
const count = await window.nostr.relay.count({
  kinds: [1],
  since: Math.floor(Date.now() / 1000) - 86400 // last 24 hours
});
console.log(`${count} events in last 24 hours`);
```

**deleteEvents()**:
Delete events from local storage (admin operation).

```javascript
const deleted = await window.nostr.relay.deleteEvents({
  authors: ['spam-pubkey...']
});
console.log(`Deleted ${deleted} spam events`);
```

---

#### libs

Pre-loaded Nostr libraries for instant import.

```typescript
libs: {
  ndk: string;
  'nostr-tools': string;
  'applesauce-core': string;
  'applesauce-content': string;
  'applesauce-lists': string;
  'alby-sdk': string;
  versions: {
    ndk: string;
    'nostr-tools': string;
    'applesauce-core': string;
    'applesauce-content': string;
    'applesauce-lists': string;
    'alby-sdk': string;
  };
}
```

**Usage**:
```javascript
// Import NDK
const NDK = await import(window.nostr.libs.ndk);
const ndk = new NDK.default({
  relayUrls: ['wss://relay.damus.io']
});

// Import nostr-tools
const { SimplePool, nip19 } = await import(window.nostr.libs['nostr-tools']);

// Check versions
console.log('NDK version:', window.nostr.libs.versions.ndk);
console.log('Available libraries:', Object.keys(window.nostr.libs));
```

---

#### accounts

Multi-account management (dryft Pro feature).

```typescript
accounts: {
  current(): Promise<Account>;
  list(): Promise<Account[]>;
  switch(pubkey: string): Promise<void>;
}
```

**Account Interface**:
```typescript
interface Account {
  pubkey: string;
  displayName: string;
  avatar?: string;
  nip05?: string;
  active: boolean;
  created: number;
}
```

**current()**:
Get information about the currently active account.

```javascript
const account = await window.nostr.accounts.current();
console.log('Active account:', account.displayName);
```

**list()**:
Get all available accounts.

```javascript
const accounts = await window.nostr.accounts.list();
accounts.forEach(account => {
  console.log(`${account.displayName}: ${account.pubkey}`);
});
```

**switch()**:
Switch to a different account for the current website.

```javascript
await window.nostr.accounts.switch('a1b2c3d4e5f6...');
console.log('Switched to new account');
```

## window.blossom API

Content-addressed file storage and retrieval.

### upload()

Upload a file to Blossom storage.

```typescript
upload(file: Blob, metadata?: BlossomMetadata): Promise<string>
```

**Parameters**:
- `file`: File blob to upload
- `metadata`: Optional metadata object

**Returns**: Promise that resolves to SHA256 hash of the uploaded file

**Example**:
```javascript
const fileInput = document.getElementById('file-input');
const file = fileInput.files[0];

const hash = await window.blossom.upload(file, {
  type: 'image/jpeg',
  alt: 'Profile picture'
});

console.log('File uploaded with hash:', hash);
```

---

### get()

Retrieve a file by its hash.

```typescript
get(hash: string): Promise<Blob>
```

**Parameters**:
- `hash`: SHA256 hash of the file

**Returns**: Promise that resolves to file blob

**Example**:
```javascript
const hash = 'a1b2c3d4e5f6...';
const blob = await window.blossom.get(hash);

// Create download link
const url = URL.createObjectURL(blob);
const a = document.createElement('a');
a.href = url;
a.download = 'downloaded-file';
a.click();
```

---

### mirror()

Mirror a file to additional Blossom servers.

```typescript
mirror(hash: string, servers: string[]): Promise<void>
```

**Parameters**:
- `hash`: SHA256 hash of the file to mirror
- `servers`: Array of Blossom server URLs

**Example**:
```javascript
await window.blossom.mirror(
  'a1b2c3d4e5f6...',
  ['https://blossom1.com', 'https://blossom2.com']
);
console.log('File mirrored to additional servers');
```

---

### delete()

Delete a file from Blossom storage.

```typescript
delete(hash: string): Promise<boolean>
```

**Parameters**:
- `hash`: SHA256 hash of the file to delete

**Returns**: Promise that resolves to true if file was deleted

**Example**:
```javascript
const deleted = await window.blossom.delete('a1b2c3d4e5f6...');
if (deleted) {
  console.log('File deleted successfully');
}
```

---

### list()

List files in Blossom storage.

```typescript
list(limit?: number): Promise<BlossomFile[]>
```

**Parameters**:
- `limit`: Maximum number of files to return (default: 100)

**Returns**: Promise that resolves to array of file metadata

**BlossomFile Interface**:
```typescript
interface BlossomFile {
  hash: string;
  size: number;
  type: string;
  uploaded: number;
  accessed: number;
  metadata?: object;
}
```

**Example**:
```javascript
const files = await window.blossom.list(50);
files.forEach(file => {
  console.log(`${file.hash}: ${file.size} bytes, ${file.type}`);
});
```

## Error Handling

All API methods can throw these error types:

### Common Errors

```typescript
class UserDeniedError extends Error {
  constructor() {
    super('User denied the operation');
    this.name = 'UserDeniedError';
  }
}

class RateLimitError extends Error {
  constructor(retryAfter: number) {
    super(`Rate limit exceeded. Retry after ${retryAfter}ms`);
    this.name = 'RateLimitError';
    this.retryAfter = retryAfter;
  }
}

class InvalidEventError extends Error {
  constructor(reason: string) {
    super(`Invalid event: ${reason}`);
    this.name = 'InvalidEventError';
  }
}

class NetworkError extends Error {
  constructor(message: string) {
    super(`Network error: ${message}`);
    this.name = 'NetworkError';
  }
}
```

### Error Handling Example

```javascript
async function safeNostrOperation() {
  try {
    const pubkey = await window.nostr.getPublicKey();
    return pubkey;
  } catch (error) {
    switch (error.name) {
      case 'UserDeniedError':
        console.log('User denied permission');
        break;
      case 'RateLimitError':
        console.log(`Rate limited, retry after ${error.retryAfter}ms`);
        break;
      case 'NetworkError':
        console.log('Network connection failed');
        break;
      default:
        console.error('Unknown error:', error);
    }
    throw error;
  }
}
```

## Type Definitions

### NostrEvent

```typescript
interface NostrEvent {
  id: string;
  pubkey: string;
  created_at: number;
  kind: number;
  tags: string[][];
  content: string;
  sig: string;
}

interface UnsignedEvent {
  kind: number;
  created_at: number;
  tags: string[][];
  content: string;
}
```

### NostrFilter

```typescript
interface NostrFilter {
  ids?: string[];
  authors?: string[];
  kinds?: number[];
  since?: number;
  until?: number;
  limit?: number;
  search?: string;
  [key: `#${string}`]: string[];
}
```

### BlossomMetadata

```typescript
interface BlossomMetadata {
  type?: string;
  alt?: string;
  caption?: string;
  tags?: string[];
  [key: string]: any;
}
```

## Browser Compatibility

dryft APIs are only available in dryft browser. For cross-browser compatibility:

```javascript
// Feature detection
const hasNostr = typeof window.nostr !== 'undefined';
const hasTungstenFeatures = window.nostr?.relay?.url !== undefined;
const hasBlossom = typeof window.blossom !== 'undefined';

if (hasNostr) {
  console.log('Nostr support available');
  
  if (hasTungstenFeatures) {
    console.log('Running in dryft browser');
  } else {
    console.log('Running with Nostr extension');
  }
}

if (hasBlossom) {
  console.log('Blossom storage available');
}
```

## Migration from Extensions

Migrating from Nostr browser extensions:

```javascript
// Works with both extensions and dryft
async function getNostrProvider() {
  // Check for dryft first (more features)
  if (window.nostr?.relay?.url) {
    return {
      type: 'dryft',
      provider: window.nostr,
      features: ['local-relay', 'blossom', 'multi-account']
    };
  }
  
  // Fallback to extension
  if (window.nostr) {
    return {
      type: 'extension',
      provider: window.nostr,
      features: ['basic-nip07']
    };
  }
  
  throw new Error('No Nostr provider available');
}
```