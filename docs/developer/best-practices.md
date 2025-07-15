# Best Practices

Security, performance, and development best practices for building Nostr applications with Tungsten Browser.

## Security Best Practices

### 🔐 Key Management

**Never Handle Private Keys**:
```javascript
// ❌ DON'T: Try to access private keys
// Private keys are never exposed to web content in Tungsten

// ✅ DO: Use the signing API
const signedEvent = await window.nostr.signEvent(event);
```

**Validate All User Input**:
```javascript
function validateEvent(event) {
  // Check required fields
  if (!event.kind || !event.content || !event.tags || !event.created_at) {
    throw new Error('Missing required event fields');
  }
  
  // Validate data types
  if (typeof event.kind !== 'number') {
    throw new Error('Event kind must be number');
  }
  
  if (typeof event.content !== 'string') {
    throw new Error('Event content must be string');
  }
  
  if (!Array.isArray(event.tags)) {
    throw new Error('Event tags must be array');
  }
  
  // Check limits
  if (event.content.length > 100000) {
    throw new Error('Event content too long');
  }
  
  if (event.tags.length > 100) {
    throw new Error('Too many tags');
  }
  
  return true;
}
```

**Implement Rate Limiting**:
```javascript
class RateLimiter {
  constructor() {
    this.operations = new Map();
  }
  
  checkLimit(operation, maxOps = 10, windowMs = 60000) {
    const now = Date.now();
    const windowStart = now - windowMs;
    
    if (!this.operations.has(operation)) {
      this.operations.set(operation, []);
    }
    
    const ops = this.operations.get(operation);
    
    // Remove old operations
    const recentOps = ops.filter(time => time > windowStart);
    this.operations.set(operation, recentOps);
    
    // Check limit
    if (recentOps.length >= maxOps) {
      throw new Error(`Rate limit exceeded for ${operation}`);
    }
    
    // Record this operation
    recentOps.push(now);
  }
}

const rateLimiter = new RateLimiter();

async function signEvent(event) {
  rateLimiter.checkLimit('signEvent', 20, 60000); // 20 per minute
  return await window.nostr.signEvent(event);
}
```

### 🛡️ Permission Management

**Request Minimal Permissions**:
```javascript
// ❌ DON'T: Request all permissions upfront
// ✅ DO: Request permissions as needed

class PermissionManager {
  constructor() {
    this.grantedPermissions = new Set();
  }
  
  async requestGetPublicKey() {
    if (this.grantedPermissions.has('getPublicKey')) {
      return await window.nostr.getPublicKey();
    }
    
    try {
      const pubkey = await window.nostr.getPublicKey();
      this.grantedPermissions.add('getPublicKey');
      return pubkey;
    } catch (error) {
      if (error.message.includes('denied')) {
        throw new Error('User denied public key access');
      }
      throw error;
    }
  }
  
  async requestSigning() {
    // Test signing permission with a dummy event
    try {
      await window.nostr.signEvent({
        kind: 1,
        created_at: Math.floor(Date.now() / 1000),
        tags: [],
        content: ''
      });
      this.grantedPermissions.add('signEvent');
      return true;
    } catch (error) {
      if (error.message.includes('denied')) {
        return false;
      }
      throw error;
    }
  }
}
```

**Handle Permission Denials Gracefully**:
```javascript
async function handlePermissionDenial(operation) {
  const message = {
    getPublicKey: 'To connect your Nostr identity, please allow access to your public key.',
    signEvent: 'To publish notes, please allow event signing.',
    getRelays: 'To sync with your relays, please allow relay access.',
  }[operation];
  
  // Show user-friendly message
  showNotification(message, 'info');
  
  // Offer alternative actions
  showAlternativeActions(operation);
}

function showAlternativeActions(operation) {
  if (operation === 'getPublicKey') {
    // Offer to manually enter npub
    showManualNpubInput();
  } else if (operation === 'signEvent') {
    // Explain how to enable signing
    showSigningHelp();
  }
}
```

### 🔒 Content Security

**Sanitize User Content**:
```javascript
function sanitizeContent(content) {
  // Remove potentially dangerous HTML
  const div = document.createElement('div');
  div.textContent = content;
  const sanitized = div.innerHTML;
  
  // Additional sanitization for Nostr-specific content
  return sanitized
    .replace(/javascript:/gi, '')
    .replace(/data:text\/html/gi, '')
    .replace(/<script[^>]*>.*?<\/script>/gi, '');
}

function renderSafeContent(content) {
  const sanitized = sanitizeContent(content);
  
  // Convert nostr: links to safe links
  const withNostrLinks = sanitized.replace(
    /nostr:([a-z0-9]+)/gi,
    '<a href="#" onclick="handleNostrLink(\'$1\')">nostr:$1</a>'
  );
  
  return withNostrLinks;
}
```

## Performance Best Practices

### ⚡ Efficient API Usage

**Batch Operations**:
```javascript
// ❌ DON'T: Make many individual calls
for (const event of events) {
  await window.nostr.signEvent(event);
}

// ✅ DO: Batch when possible
async function batchSign(events, batchSize = 5) {
  const results = [];
  
  for (let i = 0; i < events.length; i += batchSize) {
    const batch = events.slice(i, i + batchSize);
    const batchPromises = batch.map(event => window.nostr.signEvent(event));
    
    try {
      const batchResults = await Promise.all(batchPromises);
      results.push(...batchResults);
    } catch (error) {
      // Handle batch errors appropriately
      console.error('Batch signing failed:', error);
    }
    
    // Small delay between batches to respect rate limits
    if (i + batchSize < events.length) {
      await new Promise(resolve => setTimeout(resolve, 100));
    }
  }
  
  return results;
}
```

**Use Local Relay Efficiently**:
```javascript
class EfficientRelayClient {
  constructor() {
    this.cache = new Map();
    this.pending = new Map();
  }
  
  async query(filter, useCache = true) {
    const filterKey = JSON.stringify(filter);
    
    // Return cached results if available
    if (useCache && this.cache.has(filterKey)) {
      const cached = this.cache.get(filterKey);
      if (Date.now() - cached.timestamp < 60000) { // 1 minute cache
        return cached.events;
      }
    }
    
    // Deduplicate concurrent requests
    if (this.pending.has(filterKey)) {
      return await this.pending.get(filterKey);
    }
    
    const queryPromise = this.performQuery(filter);
    this.pending.set(filterKey, queryPromise);
    
    try {
      const events = await queryPromise;
      
      // Cache results
      this.cache.set(filterKey, {
        events,
        timestamp: Date.now()
      });
      
      return events;
    } finally {
      this.pending.delete(filterKey);
    }
  }
  
  async performQuery(filter) {
    if (window.nostr?.relay?.connected) {
      return await window.nostr.relay.query(filter);
    }
    
    // Fallback to external relays
    const tools = await import(window.nostr.libs['nostr-tools']);
    const pool = new tools.SimplePool();
    const relays = await this.getRelayList();
    
    return await pool.querySync(relays, filter);
  }
}
```

### 🚀 Library Loading Optimization

**Lazy Load Libraries**:
```javascript
class LibraryLoader {
  constructor() {
    this.loaded = new Map();
    this.loading = new Map();
  }
  
  async loadLibrary(name) {
    // Return if already loaded
    if (this.loaded.has(name)) {
      return this.loaded.get(name);
    }
    
    // Return existing promise if currently loading
    if (this.loading.has(name)) {
      return await this.loading.get(name);
    }
    
    // Start loading
    const loadPromise = this.performLoad(name);
    this.loading.set(name, loadPromise);
    
    try {
      const library = await loadPromise;
      this.loaded.set(name, library);
      return library;
    } finally {
      this.loading.delete(name);
    }
  }
  
  async performLoad(name) {
    if (!window.nostr?.libs?.[name]) {
      throw new Error(`Library ${name} not available`);
    }
    
    return await import(window.nostr.libs[name]);
  }
  
  // Preload commonly used libraries
  async preloadEssentials() {
    const essentials = ['ndk', 'nostr-tools'];
    
    const promises = essentials.map(name => 
      this.loadLibrary(name).catch(error => {
        console.warn(`Failed to preload ${name}:`, error);
      })
    );
    
    await Promise.allSettled(promises);
  }
}

const libraryLoader = new LibraryLoader();

// Preload on app initialization
document.addEventListener('DOMContentLoaded', () => {
  libraryLoader.preloadEssentials();
});
```

### 📦 Memory Management

**Clean Up Resources**:
```javascript
class ResourceManager {
  constructor() {
    this.webSockets = new Set();
    this.intervals = new Set();
    this.objectUrls = new Set();
  }
  
  createWebSocket(url) {
    const ws = new WebSocket(url);
    this.webSockets.add(ws);
    
    // Auto-cleanup on close
    ws.addEventListener('close', () => {
      this.webSockets.delete(ws);
    });
    
    return ws;
  }
  
  setInterval(callback, ms) {
    const id = setInterval(callback, ms);
    this.intervals.add(id);
    return id;
  }
  
  createObjectURL(blob) {
    const url = URL.createObjectURL(blob);
    this.objectUrls.add(url);
    return url;
  }
  
  cleanup() {
    // Close all WebSockets
    for (const ws of this.webSockets) {
      ws.close();
    }
    this.webSockets.clear();
    
    // Clear all intervals
    for (const id of this.intervals) {
      clearInterval(id);
    }
    this.intervals.clear();
    
    // Revoke all object URLs
    for (const url of this.objectUrls) {
      URL.revokeObjectURL(url);
    }
    this.objectUrls.clear();
  }
}

// Global resource manager
const resourceManager = new ResourceManager();

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
  resourceManager.cleanup();
});
```

## Development Best Practices

### 🧪 Testing

**Mock Tungsten APIs for Testing**:
```javascript
// Test environment mock
if (process.env.NODE_ENV === 'test') {
  window.nostr = {
    async getPublicKey() {
      return 'test-pubkey-' + Math.random().toString(36).slice(2);
    },
    
    async signEvent(event) {
      return {
        ...event,
        id: 'test-id-' + Math.random().toString(36).slice(2),
        pubkey: await this.getPublicKey(),
        sig: 'test-signature-' + Math.random().toString(36).slice(2)
      };
    },
    
    async getRelays() {
      return {
        'wss://test-relay-1.com': { read: true, write: true },
        'wss://test-relay-2.com': { read: true, write: false }
      };
    },
    
    nip04: {
      async encrypt(pubkey, plaintext) {
        return 'encrypted:' + btoa(plaintext);
      },
      
      async decrypt(pubkey, ciphertext) {
        return atob(ciphertext.replace('encrypted:', ''));
      }
    },
    
    relay: {
      url: 'ws://localhost:8081',
      connected: true,
      eventCount: 1000,
      storageUsed: 5000000,
      
      async query(filter) {
        return []; // Return mock events
      }
    },
    
    libs: {
      'ndk': '/mock/ndk.js',
      'nostr-tools': '/mock/nostr-tools.js'
    }
  };
  
  window.blossom = {
    async upload(file) {
      return 'test-hash-' + Math.random().toString(36).slice(2);
    },
    
    async get(hash) {
      return new Blob(['test content'], { type: 'text/plain' });
    }
  };
}
```

**Integration Testing**:
```javascript
class NostrIntegrationTest {
  async testBasicFlow() {
    // Test connection
    const pubkey = await window.nostr.getPublicKey();
    console.assert(typeof pubkey === 'string', 'Public key should be string');
    
    // Test signing
    const event = {
      kind: 1,
      created_at: Math.floor(Date.now() / 1000),
      tags: [],
      content: 'Test event'
    };
    
    const signed = await window.nostr.signEvent(event);
    console.assert(signed.id, 'Signed event should have ID');
    console.assert(signed.sig, 'Signed event should have signature');
    
    // Test relay access (if available)
    if (window.nostr.relay) {
      const events = await window.nostr.relay.query({ limit: 1 });
      console.assert(Array.isArray(events), 'Relay query should return array');
    }
    
    console.log('✅ Basic integration test passed');
  }
}

// Run tests in development
if (process.env.NODE_ENV === 'development') {
  const tester = new NostrIntegrationTest();
  tester.testBasicFlow().catch(console.error);
}
```

### 📝 Code Organization

**Modular Architecture**:
```javascript
// nostr-client.js
export class NostrClient {
  constructor() {
    this.auth = new NostrAuth();
    this.publisher = new NostrPublisher(this.auth);
    this.relay = new RelayClient();
  }
  
  async initialize() {
    await this.auth.connect();
    await this.relay.connect();
  }
}

// auth.js
export class NostrAuth {
  // Authentication logic
}

// publisher.js
export class NostrPublisher {
  // Publishing logic
}

// relay-client.js
export class RelayClient {
  // Relay communication logic
}

// main.js
import { NostrClient } from './nostr-client.js';

const client = new NostrClient();
await client.initialize();
```

**Error Boundaries**:
```javascript
class ErrorBoundary {
  static wrap(fn, context = 'operation') {
    return async (...args) => {
      try {
        return await fn(...args);
      } catch (error) {
        console.error(`Error in ${context}:`, error);
        
        // Report to error tracking
        this.reportError(error, context);
        
        // Return safe fallback
        return this.getSafeFallback(context);
      }
    };
  }
  
  static reportError(error, context) {
    // Send to error tracking service
    if (window.errorTracker) {
      window.errorTracker.report(error, { context });
    }
  }
  
  static getSafeFallback(context) {
    const fallbacks = {
      'getPublicKey': null,
      'signEvent': null,
      'queryEvents': [],
      'uploadFile': null
    };
    
    return fallbacks[context];
  }
}

// Usage
const safeGetPublicKey = ErrorBoundary.wrap(
  () => window.nostr.getPublicKey(),
  'getPublicKey'
);
```

### 🎨 User Experience

**Progressive Enhancement**:
```javascript
class ProgressiveUI {
  constructor() {
    this.features = {
      basic: false,
      tungsten: false,
      blossom: false
    };
  }
  
  async initialize() {
    this.detectFeatures();
    this.updateUI();
    await this.enableFeatures();
  }
  
  detectFeatures() {
    this.features.basic = typeof window.nostr !== 'undefined';
    this.features.tungsten = window.nostr?.relay?.url !== undefined;
    this.features.blossom = typeof window.blossom !== 'undefined';
  }
  
  updateUI() {
    // Show/hide features based on capabilities
    document.querySelectorAll('[data-requires]').forEach(element => {
      const required = element.dataset.requires;
      const available = this.features[required];
      
      element.style.display = available ? 'block' : 'none';
      
      if (!available) {
        element.setAttribute('aria-hidden', 'true');
      }
    });
    
    // Update status indicators
    this.updateStatusIndicators();
  }
  
  updateStatusIndicators() {
    const statusElement = document.querySelector('[data-nostr-status]');
    if (!statusElement) return;
    
    if (this.features.tungsten) {
      statusElement.textContent = '🚀 Tungsten Browser - Full features';
      statusElement.className = 'status-tungsten';
    } else if (this.features.basic) {
      statusElement.textContent = '⚡ Nostr Extension - Basic features';
      statusElement.className = 'status-basic';
    } else {
      statusElement.textContent = '📱 Install Tungsten for Nostr features';
      statusElement.className = 'status-none';
    }
  }
}

// Initialize progressive UI
const ui = new ProgressiveUI();
document.addEventListener('DOMContentLoaded', () => ui.initialize());
```

**Loading States**:
```javascript
class LoadingManager {
  static show(element, message = 'Loading...') {
    element.setAttribute('data-loading', 'true');
    element.setAttribute('aria-busy', 'true');
    
    const spinner = document.createElement('div');
    spinner.className = 'loading-spinner';
    spinner.textContent = message;
    
    element.appendChild(spinner);
  }
  
  static hide(element) {
    element.removeAttribute('data-loading');
    element.removeAttribute('aria-busy');
    
    const spinner = element.querySelector('.loading-spinner');
    if (spinner) {
      spinner.remove();
    }
  }
}

// Usage
async function publishNote() {
  const button = document.getElementById('publish-btn');
  
  LoadingManager.show(button, 'Publishing...');
  
  try {
    await doPublish();
  } finally {
    LoadingManager.hide(button);
  }
}
```

## Deployment Best Practices

### 🔒 Production Security

**Environment Configuration**:
```javascript
const config = {
  development: {
    debugMode: true,
    errorReporting: false,
    allowInsecureOrigins: true,
    verboseLogging: true
  },
  
  production: {
    debugMode: false,
    errorReporting: true,
    allowInsecureOrigins: false,
    verboseLogging: false
  }
};

const env = process.env.NODE_ENV || 'development';
const currentConfig = config[env];

// Apply configuration
if (currentConfig.debugMode) {
  window.TUNGSTEN_DEBUG = true;
}
```

**Content Security Policy**:
```html
<meta http-equiv="Content-Security-Policy" content="
  default-src 'self';
  script-src 'self' 'unsafe-inline';
  style-src 'self' 'unsafe-inline';
  img-src 'self' data: blossom:;
  connect-src 'self' ws: wss:;
  font-src 'self';
  object-src 'none';
  base-uri 'self';
  form-action 'self';
">
```

### 📊 Monitoring

**Performance Monitoring**:
```javascript
class PerformanceMonitor {
  static measureOperation(name, fn) {
    return async (...args) => {
      const start = performance.now();
      
      try {
        const result = await fn(...args);
        const duration = performance.now() - start;
        
        console.log(`${name} completed in ${duration.toFixed(2)}ms`);
        
        // Report to analytics
        this.reportMetric(name, duration, 'success');
        
        return result;
      } catch (error) {
        const duration = performance.now() - start;
        
        console.error(`${name} failed after ${duration.toFixed(2)}ms:`, error);
        
        // Report error metric
        this.reportMetric(name, duration, 'error');
        
        throw error;
      }
    };
  }
  
  static reportMetric(operation, duration, status) {
    // Send to analytics service
    if (window.analytics) {
      window.analytics.track('nostr_operation', {
        operation,
        duration,
        status,
        timestamp: Date.now()
      });
    }
  }
}

// Wrap critical operations
const publishNote = PerformanceMonitor.measureOperation(
  'publishNote',
  async (content) => {
    // Publishing logic
  }
);
```

These best practices ensure your Nostr applications are secure, performant, and user-friendly while taking full advantage of Tungsten Browser's capabilities.