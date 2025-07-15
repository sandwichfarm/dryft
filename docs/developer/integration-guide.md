# Integration Guide

Step-by-step guide to integrating your web application with Tungsten Browser's native Nostr capabilities.

## Overview

This guide walks you through building Nostr-enabled web applications that work seamlessly with Tungsten Browser. We'll cover detection, basic operations, error handling, and advanced features.

## Quick Integration Checklist

- [ ] Detect Tungsten Browser capabilities
- [ ] Implement graceful fallbacks for other browsers
- [ ] Handle user permissions properly
- [ ] Add error handling for all operations
- [ ] Test with multiple accounts
- [ ] Optimize for Tungsten's enhanced features

## Step 1: Environment Detection

### Basic Detection

```javascript
// Detect Nostr support
function detectNostrSupport() {
  const hasNostr = typeof window.nostr !== 'undefined';
  const hasTungsten = window.nostr?.relay?.url !== undefined;
  const hasBlossom = typeof window.blossom !== 'undefined';
  
  return {
    supported: hasNostr,
    isTungsten: hasTungsten,
    hasBlossom: hasBlossom,
    features: {
      localRelay: hasTungsten,
      preloadedLibs: !!window.nostr?.libs,
      multiAccount: !!window.nostr?.accounts,
      blossomStorage: hasBlossom
    }
  };
}

// Usage
const nostrEnv = detectNostrSupport();
console.log('Nostr environment:', nostrEnv);
```

### Feature-Specific Detection

```javascript
// Check for specific Tungsten features
class TungstenDetector {
  static get isAvailable() {
    return typeof window.nostr !== 'undefined';
  }
  
  static get isTungsten() {
    return window.nostr?.relay?.url !== undefined;
  }
  
  static get hasLocalRelay() {
    return window.nostr?.relay?.connected === true;
  }
  
  static get hasBlossom() {
    return typeof window.blossom !== 'undefined';
  }
  
  static get availableLibraries() {
    return Object.keys(window.nostr?.libs || {});
  }
  
  static get supportsMultiAccount() {
    return typeof window.nostr?.accounts !== 'undefined';
  }
}

// Conditional feature usage
if (TungstenDetector.isTungsten) {
  console.log('Enhanced Tungsten features available');
  enableTungstenFeatures();
} else if (TungstenDetector.isAvailable) {
  console.log('Basic Nostr support available');
  enableBasicNostrFeatures();
} else {
  console.log('No Nostr support - show installation guide');
  showNostrInstallPrompt();
}
```

## Step 2: Basic Nostr Operations

### User Authentication

```javascript
class NostrAuth {
  constructor() {
    this.currentUser = null;
    this.permissions = new Set();
  }
  
  async connect() {
    try {
      // Request public key (triggers permission prompt)
      const pubkey = await window.nostr.getPublicKey();
      this.currentUser = pubkey;
      this.permissions.add('getPublicKey');
      
      console.log('Connected to user:', pubkey);
      return pubkey;
    } catch (error) {
      if (error.message.includes('denied')) {
        throw new Error('User denied connection');
      }
      throw error;
    }
  }
  
  async requestSigningPermission() {
    try {
      // Test signing with a minimal event
      const testEvent = {
        kind: 1,
        created_at: Math.floor(Date.now() / 1000),
        tags: [],
        content: ''
      };
      
      await window.nostr.signEvent(testEvent);
      this.permissions.add('signEvent');
      return true;
    } catch (error) {
      if (error.message.includes('denied')) {
        return false;
      }
      throw error;
    }
  }
  
  hasPermission(permission) {
    return this.permissions.has(permission);
  }
  
  isConnected() {
    return this.currentUser !== null;
  }
}

// Usage
const auth = new NostrAuth();

async function initializeApp() {
  try {
    await auth.connect();
    updateUI({ connected: true, user: auth.currentUser });
  } catch (error) {
    updateUI({ connected: false, error: error.message });
  }
}
```

### Event Publishing

```javascript
class NostrPublisher {
  constructor(auth) {
    this.auth = auth;
  }
  
  async publishNote(content, tags = []) {
    if (!this.auth.isConnected()) {
      throw new Error('Not connected to Nostr');
    }
    
    const event = {
      kind: 1,
      created_at: Math.floor(Date.now() / 1000),
      tags: tags,
      content: content
    };
    
    try {
      const signedEvent = await window.nostr.signEvent(event);
      await this.broadcastEvent(signedEvent);
      return signedEvent;
    } catch (error) {
      console.error('Publishing failed:', error);
      throw error;
    }
  }
  
  async broadcastEvent(event) {
    const results = [];
    
    // Try local relay first (if available)
    if (window.nostr?.relay?.connected) {
      try {
        await this.publishToRelay(window.nostr.relay.url, event);
        results.push({ relay: 'local', success: true });
      } catch (error) {
        results.push({ relay: 'local', success: false, error });
      }
    }
    
    // Publish to user's relays
    try {
      const relays = await window.nostr.getRelays();
      const writeRelays = Object.entries(relays)
        .filter(([url, policy]) => policy.write)
        .map(([url]) => url);
      
      const relayResults = await Promise.allSettled(
        writeRelays.map(url => this.publishToRelay(url, event))
      );
      
      relayResults.forEach((result, index) => {
        results.push({
          relay: writeRelays[index],
          success: result.status === 'fulfilled',
          error: result.reason
        });
      });
    } catch (error) {
      console.warn('Could not get relay list:', error);
    }
    
    return results;
  }
  
  async publishToRelay(url, event) {
    // Implementation depends on your WebSocket relay client
    // This is a simplified example
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(url);
      
      ws.onopen = () => {
        ws.send(JSON.stringify(['EVENT', event]));
      };
      
      ws.onmessage = (message) => {
        const [type, eventId, success, reason] = JSON.parse(message.data);
        if (type === 'OK') {
          if (success) {
            resolve();
          } else {
            reject(new Error(reason));
          }
        }
        ws.close();
      };
      
      ws.onerror = reject;
    });
  }
}
```

## Step 3: Using Pre-loaded Libraries

### NDK Integration

```javascript
class TungstenNDKClient {
  constructor() {
    this.ndk = null;
    this.initialized = false;
  }
  
  async initialize() {
    if (!window.nostr?.libs?.ndk) {
      throw new Error('NDK not available in Tungsten');
    }
    
    // Import NDK from Tungsten's pre-loaded libraries
    const NDK = await import(window.nostr.libs.ndk);
    
    // Create NDK instance with user's relays
    const relays = await window.nostr.getRelays();
    const relayUrls = Object.keys(relays);
    
    this.ndk = new NDK.default({
      relayUrls: relayUrls,
      signer: new TungstenSigner() // Custom signer for Tungsten
    });
    
    await this.ndk.connect();
    this.initialized = true;
    
    console.log('NDK initialized with', relayUrls.length, 'relays');
  }
  
  async getUser(identifier) {
    if (!this.initialized) await this.initialize();
    
    return this.ndk.getUser({ npub: identifier });
  }
  
  async fetchProfile(pubkey) {
    const user = await this.getUser(pubkey);
    return await user.fetchProfile();
  }
  
  async publishEvent(event) {
    if (!this.initialized) await this.initialize();
    
    const ndkEvent = new this.ndk.NDKEvent(this.ndk, event);
    return await ndkEvent.publish();
  }
}

// Custom signer for Tungsten
class TungstenSigner {
  async sign(event) {
    return await window.nostr.signEvent(event);
  }
  
  async getPublicKey() {
    return await window.nostr.getPublicKey();
  }
}
```

### nostr-tools Integration

```javascript
class TungstenNostrTools {
  constructor() {
    this.tools = null;
    this.pool = null;
  }
  
  async initialize() {
    // Import nostr-tools from Tungsten
    this.tools = await import(window.nostr.libs['nostr-tools']);
    
    // Create relay pool with user's relays
    this.pool = new this.tools.SimplePool();
    
    console.log('nostr-tools initialized');
  }
  
  async queryEvents(filter, relays = null) {
    if (!this.pool) await this.initialize();
    
    // Use provided relays or get from user settings
    const targetRelays = relays || await this.getUserRelays();
    
    return await this.pool.querySync(targetRelays, filter);
  }
  
  async getUserRelays() {
    const relays = await window.nostr.getRelays();
    return Object.entries(relays)
      .filter(([url, policy]) => policy.read)
      .map(([url]) => url);
  }
  
  generateKeyPair() {
    return this.tools.generateSecretKey();
  }
  
  npubToHex(npub) {
    return this.tools.nip19.decode(npub).data;
  }
  
  hexToNpub(hex) {
    return this.tools.nip19.npubEncode(hex);
  }
}
```

## Step 4: Local Relay Integration

### Enhanced Data Access

```javascript
class LocalRelayClient {
  constructor() {
    this.isAvailable = window.nostr?.relay?.url !== undefined;
    this.url = window.nostr?.relay?.url;
  }
  
  async queryLocal(filter) {
    if (!this.isAvailable) {
      throw new Error('Local relay not available');
    }
    
    return await window.nostr.relay.query(filter);
  }
  
  async getLocalStats() {
    return {
      connected: window.nostr.relay.connected,
      eventCount: window.nostr.relay.eventCount,
      storageUsed: window.nostr.relay.storageUsed
    };
  }
  
  // Hybrid query: local first, then external relays
  async hybridQuery(filter, externalRelays = []) {
    const results = [];
    
    // Query local relay first (fastest)
    if (this.isAvailable) {
      try {
        const localEvents = await this.queryLocal(filter);
        results.push(...localEvents);
        console.log(`Found ${localEvents.length} events locally`);
      } catch (error) {
        console.warn('Local relay query failed:', error);
      }
    }
    
    // Query external relays for additional events
    if (externalRelays.length > 0) {
      try {
        const tools = await import(window.nostr.libs['nostr-tools']);
        const pool = new tools.SimplePool();
        
        const externalEvents = await pool.querySync(externalRelays, filter);
        results.push(...externalEvents);
        console.log(`Found ${externalEvents.length} events from external relays`);
      } catch (error) {
        console.warn('External relay query failed:', error);
      }
    }
    
    // Deduplicate by event ID
    const uniqueEvents = [];
    const seenIds = new Set();
    
    for (const event of results) {
      if (!seenIds.has(event.id)) {
        seenIds.add(event.id);
        uniqueEvents.push(event);
      }
    }
    
    return uniqueEvents;
  }
}
```

## Step 5: Blossom File Integration

### File Upload and Management

```javascript
class BlossomFileManager {
  constructor() {
    this.isAvailable = typeof window.blossom !== 'undefined';
  }
  
  async uploadFile(file, metadata = {}) {
    if (!this.isAvailable) {
      throw new Error('Blossom not available');
    }
    
    const uploadMetadata = {
      type: file.type,
      alt: metadata.alt || file.name,
      ...metadata
    };
    
    try {
      const hash = await window.blossom.upload(file, uploadMetadata);
      
      return {
        hash,
        url: `blossom://${hash}`,
        httpUrl: `http://localhost:8080/files/${hash}`,
        size: file.size,
        type: file.type
      };
    } catch (error) {
      console.error('Blossom upload failed:', error);
      throw error;
    }
  }
  
  async createImageGallery(files) {
    const uploads = await Promise.all(
      files.map(file => this.uploadFile(file, {
        tags: ['gallery', 'image'],
        collection: 'user-gallery'
      }))
    );
    
    return uploads;
  }
  
  async generateThumbnail(imageFile, maxSize = 300) {
    return new Promise((resolve) => {
      const canvas = document.createElement('canvas');
      const ctx = canvas.getContext('2d');
      const img = new Image();
      
      img.onload = async () => {
        // Calculate thumbnail dimensions
        const ratio = Math.min(maxSize / img.width, maxSize / img.height);
        canvas.width = img.width * ratio;
        canvas.height = img.height * ratio;
        
        // Draw thumbnail
        ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
        
        // Convert to blob and upload
        canvas.toBlob(async (blob) => {
          const result = await this.uploadFile(blob, {
            tags: ['thumbnail'],
            originalHash: 'pending' // Set after main upload
          });
          resolve(result);
        }, 'image/jpeg', 0.8);
      };
      
      img.src = URL.createObjectURL(imageFile);
    });
  }
}
```

## Step 6: Error Handling and UX

### Comprehensive Error Handling

```javascript
class NostrErrorHandler {
  static handleError(error, operation) {
    console.error(`Nostr ${operation} error:`, error);
    
    // User-friendly error messages
    const userMessage = this.getUserMessage(error, operation);
    
    // Log for debugging
    this.logError(error, operation);
    
    // Show user notification
    this.showUserNotification(userMessage, 'error');
    
    return userMessage;
  }
  
  static getUserMessage(error, operation) {
    if (error.message.includes('denied')) {
      return `Permission denied for ${operation}. Please check your browser settings.`;
    }
    
    if (error.message.includes('rate limit')) {
      return 'Too many requests. Please wait a moment and try again.';
    }
    
    if (error.message.includes('network') || error.message.includes('connection')) {
      return 'Network connection failed. Please check your internet connection.';
    }
    
    if (error.message.includes('invalid')) {
      return 'Invalid data format. Please check your input.';
    }
    
    // Default message
    return `${operation} failed. Please try again.`;
  }
  
  static logError(error, operation) {
    // Send to error tracking service in production
    const errorLog = {
      operation,
      error: error.message,
      stack: error.stack,
      timestamp: Date.now(),
      userAgent: navigator.userAgent,
      url: window.location.href
    };
    
    console.log('Error log:', errorLog);
    // In production: sendToErrorTracker(errorLog);
  }
  
  static showUserNotification(message, type = 'info') {
    // Create and show notification
    const notification = document.createElement('div');
    notification.className = `notification notification-${type}`;
    notification.textContent = message;
    
    document.body.appendChild(notification);
    
    // Auto-remove after 5 seconds
    setTimeout(() => {
      notification.remove();
    }, 5000);
  }
}

// Usage in async functions
async function safeNostrOperation(operation) {
  try {
    return await operation();
  } catch (error) {
    return NostrErrorHandler.handleError(error, 'operation');
  }
}
```

### Progressive Enhancement

```javascript
class ProgressiveNostrApp {
  constructor() {
    this.features = {
      basic: false,
      tungsten: false,
      blossom: false
    };
    
    this.init();
  }
  
  async init() {
    // Detect available features
    this.features.basic = typeof window.nostr !== 'undefined';
    this.features.tungsten = window.nostr?.relay?.url !== undefined;
    this.features.blossom = typeof window.blossom !== 'undefined';
    
    // Enable features progressively
    if (this.features.basic) {
      await this.enableBasicFeatures();
    }
    
    if (this.features.tungsten) {
      await this.enableTungstenFeatures();
    }
    
    if (this.features.blossom) {
      await this.enableBlossomFeatures();
    }
    
    // Update UI based on available features
    this.updateUI();
  }
  
  async enableBasicFeatures() {
    console.log('Enabling basic Nostr features');
    
    // Enable basic signing and posting
    document.getElementById('post-button').disabled = false;
    document.getElementById('connect-button').disabled = false;
  }
  
  async enableTungstenFeatures() {
    console.log('Enabling Tungsten-specific features');
    
    // Enable local relay features
    document.getElementById('local-search').style.display = 'block';
    document.getElementById('offline-mode').style.display = 'block';
    
    // Enable pre-loaded library features
    document.getElementById('advanced-tools').style.display = 'block';
  }
  
  async enableBlossomFeatures() {
    console.log('Enabling Blossom file features');
    
    // Enable file upload features
    document.getElementById('file-upload').style.display = 'block';
    document.getElementById('media-gallery').style.display = 'block';
  }
  
  updateUI() {
    const statusDiv = document.getElementById('nostr-status');
    
    if (this.features.tungsten) {
      statusDiv.innerHTML = '✅ Tungsten Browser - Full features available';
      statusDiv.className = 'status-tungsten';
    } else if (this.features.basic) {
      statusDiv.innerHTML = '⚡ Nostr Extension - Basic features available';
      statusDiv.className = 'status-basic';
    } else {
      statusDiv.innerHTML = '❌ No Nostr support - Install Tungsten or extension';
      statusDiv.className = 'status-none';
    }
  }
}

// Initialize app when page loads
document.addEventListener('DOMContentLoaded', () => {
  new ProgressiveNostrApp();
});
```

## Step 7: Testing and Validation

### Integration Testing

```javascript
class NostrIntegrationTest {
  constructor() {
    this.tests = [];
    this.results = [];
  }
  
  async runAllTests() {
    console.log('Running Nostr integration tests...');
    
    this.addTest('Basic Detection', this.testDetection);
    this.addTest('Connection', this.testConnection);
    this.addTest('Event Signing', this.testSigning);
    this.addTest('Local Relay', this.testLocalRelay);
    this.addTest('Blossom Upload', this.testBlossomUpload);
    
    for (const test of this.tests) {
      try {
        await test.fn();
        this.results.push({ name: test.name, status: 'PASS' });
        console.log(`✅ ${test.name}: PASS`);
      } catch (error) {
        this.results.push({ name: test.name, status: 'FAIL', error });
        console.log(`❌ ${test.name}: FAIL -`, error.message);
      }
    }
    
    return this.results;
  }
  
  addTest(name, fn) {
    this.tests.push({ name, fn: fn.bind(this) });
  }
  
  async testDetection() {
    if (typeof window.nostr === 'undefined') {
      throw new Error('window.nostr not available');
    }
    
    if (typeof window.nostr.getPublicKey !== 'function') {
      throw new Error('getPublicKey method not available');
    }
  }
  
  async testConnection() {
    const pubkey = await window.nostr.getPublicKey();
    
    if (!pubkey || typeof pubkey !== 'string') {
      throw new Error('Invalid public key returned');
    }
    
    if (pubkey.length !== 64) {
      throw new Error('Public key has incorrect length');
    }
  }
  
  async testSigning() {
    const testEvent = {
      kind: 1,
      created_at: Math.floor(Date.now() / 1000),
      tags: [['test', 'true']],
      content: 'Test event for integration testing'
    };
    
    const signed = await window.nostr.signEvent(testEvent);
    
    if (!signed.id || !signed.sig || !signed.pubkey) {
      throw new Error('Signed event missing required fields');
    }
  }
  
  async testLocalRelay() {
    if (!window.nostr?.relay?.query) {
      console.log('Local relay not available - skipping test');
      return;
    }
    
    const events = await window.nostr.relay.query({ limit: 1 });
    
    if (!Array.isArray(events)) {
      throw new Error('Local relay query did not return array');
    }
  }
  
  async testBlossomUpload() {
    if (typeof window.blossom === 'undefined') {
      console.log('Blossom not available - skipping test');
      return;
    }
    
    // Create test file
    const testData = 'test file content';
    const testFile = new Blob([testData], { type: 'text/plain' });
    
    const hash = await window.blossom.upload(testFile);
    
    if (!hash || typeof hash !== 'string') {
      throw new Error('Blossom upload did not return valid hash');
    }
    
    // Clean up test file
    await window.blossom.delete(hash);
  }
}

// Run tests in development
if (process.env.NODE_ENV === 'development') {
  window.runNostrTests = async () => {
    const tester = new NostrIntegrationTest();
    return await tester.runAllTests();
  };
}
```

## Production Deployment

### Performance Optimization

```javascript
// Lazy load Nostr features
const NostrLazyLoader = {
  loaded: false,
  
  async load() {
    if (this.loaded) return;
    
    // Only load when needed
    if (typeof window.nostr !== 'undefined') {
      // Initialize Nostr components
      await this.initializeNostr();
      this.loaded = true;
    }
  },
  
  async initializeNostr() {
    // Pre-connect to relays
    if (window.nostr.relay?.connected) {
      console.log('Local relay ready');
    }
    
    // Preload essential libraries
    if (window.nostr.libs) {
      // Cache library imports for faster access
      this.cachedLibs = {};
      for (const [name, url] of Object.entries(window.nostr.libs)) {
        if (['ndk', 'nostr-tools'].includes(name)) {
          this.cachedLibs[name] = import(url);
        }
      }
    }
  }
};

// Load on user interaction
document.addEventListener('click', NostrLazyLoader.load.bind(NostrLazyLoader), { once: true });
```

### Security Hardening

```javascript
// Input validation and sanitization
class NostrSecurity {
  static validateEvent(event) {
    const maxContentLength = 100000;
    const maxTagCount = 100;
    const maxTagLength = 1000;
    
    if (typeof event.content !== 'string') {
      throw new Error('Event content must be string');
    }
    
    if (event.content.length > maxContentLength) {
      throw new Error('Event content too long');
    }
    
    if (!Array.isArray(event.tags)) {
      throw new Error('Event tags must be array');
    }
    
    if (event.tags.length > maxTagCount) {
      throw new Error('Too many tags');
    }
    
    for (const tag of event.tags) {
      if (!Array.isArray(tag)) {
        throw new Error('Each tag must be array');
      }
      
      if (tag.join('').length > maxTagLength) {
        throw new Error('Tag too long');
      }
    }
    
    return true;
  }
  
  static sanitizeContent(content) {
    // Remove potentially dangerous content
    return content
      .replace(/<script[^>]*>.*?<\/script>/gi, '')
      .replace(/javascript:/gi, '')
      .replace(/on\w+=/gi, '');
  }
}
```

This integration guide provides a complete foundation for building Nostr applications with Tungsten Browser. For more specific examples, see the [Code Examples](examples/) directory.

## Next Steps

1. Review the [API Reference](api-reference.md) for complete method documentation
2. Explore [Code Examples](examples/) for specific use cases
3. Check [Best Practices](best-practices.md) for security and performance tips
4. Join the [developer community](https://discord.gg/tungsten-dev) for support