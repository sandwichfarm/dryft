# dryft browser Developer Documentation

Build web applications that leverage Tungsten's native Nostr capabilities without requiring extensions.

## Quick Navigation

- [API Reference](api-reference.md) - Complete window.nostr and window.blossom API
- [Integration Guide](integration-guide.md) - Step-by-step integration examples
- [Code Examples](examples/) - Working examples and snippets
- [Best Practices](best-practices.md) - Security and performance recommendations
- [Testing](testing.md) - Testing your Nostr applications
- [Migration](migration.md) - Migrating from extension-based solutions

## Overview

dryft browser provides native Nostr protocol support through enhanced web APIs. Unlike browser extensions, these APIs are:

- **Always Available**: No installation or enabling required
- **Performant**: Direct browser integration, no extension overhead
- **Secure**: Integrated with browser security model
- **Enhanced**: Additional features beyond standard NIP-07

## Quick Start

### Detection

Check if your app is running in Tungsten:

```javascript
// Basic detection
if (window.nostr) {
  console.log('Nostr support available');
}

// dryft-specific detection
if (window.nostr?.relay?.url) {
  console.log('Running in dryft browser');
  console.log('Local relay:', window.nostr.relay.url);
}
```

### Basic Usage

```javascript
// Get user's public key
const pubkey = await window.nostr.getPublicKey();
console.log('User pubkey:', pubkey);

// Sign an event
const event = {
  kind: 1,
  created_at: Math.floor(Date.now() / 1000),
  tags: [],
  content: 'Hello from Tungsten!'
};

const signedEvent = await window.nostr.signEvent(event);
console.log('Signed event:', signedEvent);
```

### Using Pre-loaded Libraries

Tungsten includes popular Nostr libraries for instant use:

```javascript
// Import NDK
const NDK = await import(window.nostr.libs.ndk);
const ndk = new NDK.default();

// Import nostr-tools
const NostrTools = await import(window.nostr.libs['nostr-tools']);
const { SimplePool } = NostrTools;

// Check available libraries
console.log('Available libraries:', Object.keys(window.nostr.libs));
console.log('Library versions:', window.nostr.libs.versions);
```

## Core APIs

### Standard NIP-07 API

Tungsten implements the complete NIP-07 specification:

```typescript
interface WindowNostr {
  getPublicKey(): Promise<string>;
  signEvent(event: UnsignedEvent): Promise<SignedEvent>;
  getRelays(): Promise<{[url: string]: {read: boolean, write: boolean}}>;
  nip04: {
    encrypt(pubkey: string, plaintext: string): Promise<string>;
    decrypt(pubkey: string, ciphertext: string): Promise<string>;
  };
}
```

### Tungsten Extensions

Additional APIs exclusive to Tungsten:

```typescript
interface TungstenNostrExtensions {
  // Local relay integration
  relay: {
    readonly url: string;
    readonly connected: boolean;
    readonly eventCount: number;
    readonly storageUsed: number;
    query(filter: NostrFilter): Promise<NostrEvent[]>;
    count(filter: NostrFilter): Promise<number>;
    deleteEvents(filter: NostrFilter): Promise<number>;
  };
  
  // Pre-loaded libraries
  libs: {
    ndk: string;
    'nostr-tools': string;
    'applesauce-core': string;
    // ... more libraries
    versions: Record<string, string>;
  };
  
  // Account management
  accounts: {
    current(): Promise<Account>;
    list(): Promise<Account[]>;
    switch(pubkey: string): Promise<void>;
  };
}
```

### Blossom API

Content-addressed file storage:

```typescript
interface WindowBlossom {
  upload(file: Blob, metadata?: object): Promise<string>; // returns hash
  get(hash: string): Promise<Blob>;
  mirror(hash: string, servers: string[]): Promise<void>;
  delete(hash: string): Promise<boolean>;
  list(limit?: number): Promise<BlossomFile[]>;
}
```

## Development Patterns

### Graceful Degradation

Build apps that work with and without Tungsten:

```javascript
class NostrClient {
  constructor() {
    this.nostr = null;
    this.init();
  }
  
  async init() {
    if (window.nostr) {
      // Tungsten or extension available
      this.nostr = window.nostr;
      
      if (window.nostr.relay?.url) {
        // dryft-specific features
        console.log('Enhanced dryft features available');
        this.localRelay = window.nostr.relay;
      }
    } else {
      // Fallback for browsers without Nostr support
      this.showNostrInstallPrompt();
    }
  }
  
  async getPublicKey() {
    if (!this.nostr) throw new Error('Nostr not available');
    return await this.nostr.getPublicKey();
  }
  
  showNostrInstallPrompt() {
    // Guide users to install Tungsten or Nostr extension
  }
}
```

### Local Relay Integration

Leverage Tungsten's local relay for better performance:

```javascript
class TungstenNostrClient {
  constructor() {
    this.localRelay = window.nostr?.relay;
    this.externalRelays = ['wss://relay1.com', 'wss://relay2.com'];
  }
  
  async queryEvents(filter) {
    const events = [];
    
    // Check local relay first (fastest)
    if (this.localRelay?.connected) {
      const localEvents = await this.localRelay.query(filter);
      events.push(...localEvents);
    }
    
    // Query external relays for additional events
    const externalEvents = await this.queryExternalRelays(filter);
    events.push(...externalEvents);
    
    // Deduplicate by event ID
    return this.deduplicateEvents(events);
  }
  
  async queryExternalRelays(filter) {
    // Implementation for external relay queries
  }
  
  deduplicateEvents(events) {
    const seen = new Set();
    return events.filter(event => {
      if (seen.has(event.id)) return false;
      seen.add(event.id);
      return true;
    });
  }
}
```

### Error Handling

Robust error handling for Nostr operations:

```javascript
async function signAndPublish(event) {
  try {
    // Sign the event
    const signedEvent = await window.nostr.signEvent(event);
    
    // Publish to local relay first
    if (window.nostr.relay?.connected) {
      await publishToRelay(window.nostr.relay.url, signedEvent);
    }
    
    // Publish to external relays
    const relays = await window.nostr.getRelays();
    const writeRelays = Object.entries(relays)
      .filter(([url, policy]) => policy.write)
      .map(([url]) => url);
    
    await Promise.allSettled(
      writeRelays.map(url => publishToRelay(url, signedEvent))
    );
    
    return signedEvent;
    
  } catch (error) {
    if (error.message.includes('denied')) {
      console.error('User denied signing permission');
      throw new Error('Permission denied by user');
    } else if (error.message.includes('invalid')) {
      console.error('Invalid event format');
      throw new Error('Event validation failed');
    } else {
      console.error('Unknown signing error:', error);
      throw error;
    }
  }
}
```

## Testing Your Application

### Local Development

Test your Nostr app during development:

```javascript
// Mock window.nostr for testing
if (!window.nostr && process.env.NODE_ENV === 'development') {
  window.nostr = {
    async getPublicKey() {
      return 'npub1testpubkey...';
    },
    async signEvent(event) {
      return { ...event, id: 'test-id', sig: 'test-sig', pubkey: 'test-pubkey' };
    },
    async getRelays() {
      return { 'wss://test-relay.com': { read: true, write: true } };
    },
    nip04: {
      async encrypt(pubkey, plaintext) { return 'encrypted-' + plaintext; },
      async decrypt(pubkey, ciphertext) { return ciphertext.replace('encrypted-', ''); }
    }
  };
}
```

### Integration Testing

Test with real Tungsten instance:

```javascript
describe('Nostr Integration', () => {
  beforeEach(() => {
    // Ensure we're testing in Tungsten
    if (!window.nostr?.relay?.url) {
      throw new Error('Tests must run in dryft browser');
    }
  });
  
  test('can get public key', async () => {
    const pubkey = await window.nostr.getPublicKey();
    expect(pubkey).toMatch(/^npub1[a-z0-9]+$/);
  });
  
  test('can query local relay', async () => {
    const events = await window.nostr.relay.query({ limit: 10 });
    expect(Array.isArray(events)).toBe(true);
  });
});
```

## Security Considerations

### Input Validation

Always validate event data:

```javascript
function validateEvent(event) {
  if (!event.kind || typeof event.kind !== 'number') {
    throw new Error('Invalid event kind');
  }
  
  if (!event.content || typeof event.content !== 'string') {
    throw new Error('Invalid event content');
  }
  
  if (!Array.isArray(event.tags)) {
    throw new Error('Invalid event tags');
  }
  
  // Additional validation...
  return true;
}

async function safeSignEvent(event) {
  validateEvent(event);
  return await window.nostr.signEvent(event);
}
```

### Permission Handling

Respect user permissions and provide clear feedback:

```javascript
class PermissionAwareClient {
  async requestPermission(operation) {
    try {
      switch (operation) {
        case 'getPublicKey':
          return await window.nostr.getPublicKey();
        case 'signEvent':
          // Don't pre-call, just indicate capability
          return true;
        default:
          throw new Error('Unknown operation');
      }
    } catch (error) {
      if (error.message.includes('denied')) {
        this.showPermissionDeniedMessage(operation);
        return false;
      }
      throw error;
    }
  }
  
  showPermissionDeniedMessage(operation) {
    // Show user-friendly message about permission denial
    console.log(`Permission denied for ${operation}. Check browser settings.`);
  }
}
```

### Rate Limiting

Respect Tungsten's built-in rate limits:

```javascript
class RateLimitedClient {
  constructor() {
    this.requestQueue = [];
    this.processing = false;
  }
  
  async queueRequest(operation, params) {
    return new Promise((resolve, reject) => {
      this.requestQueue.push({ operation, params, resolve, reject });
      this.processQueue();
    });
  }
  
  async processQueue() {
    if (this.processing || this.requestQueue.length === 0) return;
    
    this.processing = true;
    
    while (this.requestQueue.length > 0) {
      const { operation, params, resolve, reject } = this.requestQueue.shift();
      
      try {
        const result = await window.nostr[operation](...params);
        resolve(result);
        
        // Small delay to respect rate limits
        await new Promise(resolve => setTimeout(resolve, 100));
        
      } catch (error) {
        if (error.message.includes('rate limit')) {
          // Put request back in queue and wait longer
          this.requestQueue.unshift({ operation, params, resolve, reject });
          await new Promise(resolve => setTimeout(resolve, 1000));
        } else {
          reject(error);
        }
      }
    }
    
    this.processing = false;
  }
}
```

## Resources

- [NIP-07 Specification](https://github.com/nostr-protocol/nips/blob/master/07.md)
- [Nostr Protocol Documentation](https://github.com/nostr-protocol/nips)
- [Tungsten API Reference](api-reference.md)
- [Code Examples](examples/)
- [Community Discord](https://discord.gg/tungsten)

## Getting Help

- [GitHub Issues](https://github.com/sandwichfarm/dryft/issues) - Bug reports and feature requests
- [Developer Discord](https://discord.gg/tungsten-dev) - Real-time developer support
- [API Documentation](api-reference.md) - Complete API reference
- [Stack Overflow](https://stackoverflow.com/questions/tagged/tungsten-browser) - Q&A with the community