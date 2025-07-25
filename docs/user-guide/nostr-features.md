# Nostr Features

Complete guide to using Tungsten's built-in Nostr capabilities.

## Overview

Tungsten provides native Nostr protocol support without requiring extensions. All Nostr features are built directly into the browser core for maximum performance and security.

## Available Features

### ✅ NIP-07 Window.nostr API
- Native `window.nostr` object on all websites
- No extension installation required
- Full NIP-07 compatibility

### ✅ Local Relay Integration  
- Built-in WebSocket relay on `ws://localhost:8081`
- SQLite backend with full query support
- Automatic sync with external relays

### ✅ Multi-Account Management
- Manage multiple Nostr identities
- Quick account switching per website
- Secure platform-native key storage

### ✅ Pre-loaded Libraries
- Popular Nostr libraries available instantly
- NDK, nostr-tools, applesauce, and more
- No CDN dependencies

### ✅ Enhanced Permissions
- Granular permissions per origin and method
- Persistent permission memory
- User-friendly permission dialogs

## Basic Nostr Usage

### Check Nostr Availability

On any website, open Developer Tools (F12) and test:

```javascript
// Check if Nostr is available
console.log('Nostr available:', !!window.nostr);

// Check for dryft-specific features
console.log('dryft features:', Boolean(window.nostr?.relay?.url));

// List available methods
console.log('Available methods:', Object.keys(window.nostr));
```

### Get Your Public Key

```javascript
// Get your public key (requires permission)
const pubkey = await window.nostr.getPublicKey();
console.log('Your pubkey:', pubkey);

// Convert to npub format (if nostr-tools is loaded)
const { nip19 } = await import(window.nostr.libs['nostr-tools']);
const npub = nip19.npubEncode(pubkey);
console.log('Your npub:', npub);
```

### Sign Your First Event

```javascript
// Create a simple text note
const event = {
  kind: 1,
  created_at: Math.floor(Date.now() / 1000),
  tags: [['t', 'tungsten'], ['t', 'nostr']],
  content: 'Hello from dryft browser! 🚀'
};

// Sign the event
const signedEvent = await window.nostr.signEvent(event);
console.log('Signed event:', signedEvent);
```

## Using Pre-loaded Libraries

Tungsten includes popular Nostr libraries for instant use:

### Available Libraries

```javascript
// Check what's available
console.log('Libraries:', Object.keys(window.nostr.libs));
console.log('Versions:', window.nostr.libs.versions);

// Typical output:
// Libraries: ['ndk', 'nostr-tools', 'applesauce-core', 'applesauce-content', 'applesauce-lists', 'alby-sdk']
```

### Using NDK

```javascript
// Import NDK
const NDK = await import(window.nostr.libs.ndk);
const ndk = new NDK.default({
  relayUrls: ['wss://relay.damus.io', 'wss://nos.lol']
});

// Connect to relays
await ndk.connect();

// Fetch your profile
const user = ndk.getUser({ npub: 'your-npub-here' });
const profile = await user.fetchProfile();
console.log('Profile:', profile);
```

### Using nostr-tools

```javascript
// Import nostr-tools
const { SimplePool, nip19, getEventHash, signEvent } = 
  await import(window.nostr.libs['nostr-tools']);

// Create a relay pool
const pool = new SimplePool();

// Query events
const events = await pool.querySync(
  ['wss://relay.damus.io'],
  { kinds: [1], limit: 10 }
);
console.log('Recent events:', events);
```

### Using Applesauce

```javascript
// Import applesauce modules
const { PublishRequest } = await import(window.nostr.libs['applesauce-core']);
const { parseContent } = await import(window.nostr.libs['applesauce-content']);

// Parse note content
const content = 'Check out nostr:npub1abc123... and #bitcoin';
const parsed = parseContent(content);
console.log('Parsed content:', parsed);
```

## Local Relay Features

Tungsten's built-in relay provides enhanced functionality:

### Query Local Events

```javascript
// Query your local relay
const localEvents = await window.nostr.relay.query({
  kinds: [1],
  authors: [await window.nostr.getPublicKey()],
  limit: 20
});

console.log(`Found ${localEvents.length} of your notes locally`);
```

### Check Relay Status

```javascript
// Relay connection info
console.log('Local relay URL:', window.nostr.relay.url);
console.log('Connected:', window.nostr.relay.connected);
console.log('Total events stored:', window.nostr.relay.eventCount);
console.log('Storage used:', window.nostr.relay.storageUsed, 'bytes');
```

### Advanced Queries

```javascript
// Count events by kind
const kindCounts = {};
for (const kind of [0, 1, 3, 4, 5, 6, 7]) {
  const count = await window.nostr.relay.count({ kinds: [kind] });
  kindCounts[kind] = count;
}
console.log('Events by kind:', kindCounts);

// Find recent events with specific tags
const taggedEvents = await window.nostr.relay.query({
  kinds: [1],
  '#t': ['bitcoin', 'nostr'],
  since: Math.floor(Date.now() / 1000) - 86400, // last 24 hours
  limit: 50
});
```

## Working with Multiple Accounts

### Account Information

```javascript
// Get current account info (Tungsten Pro feature)
if (window.nostr.accounts) {
  const current = await window.nostr.accounts.current();
  console.log('Current account:', current.displayName);
  
  const allAccounts = await window.nostr.accounts.list();
  console.log(`You have ${allAccounts.length} accounts configured`);
}
```

### Account Switching

Use the account switcher in Tungsten's toolbar, or programmatically:

```javascript
// Switch to a specific account
if (window.nostr.accounts) {
  const accounts = await window.nostr.accounts.list();
  const workAccount = accounts.find(acc => acc.displayName === 'Work');
  
  if (workAccount) {
    await window.nostr.accounts.switch(workAccount.pubkey);
    console.log('Switched to work account');
  }
}
```

## Encryption and DMs

### NIP-04 Legacy Encryption

```javascript
// Encrypt a message
const recipientPubkey = 'a1b2c3d4...'; // recipient's hex pubkey
const message = 'Secret message for you!';

const encrypted = await window.nostr.nip04.encrypt(recipientPubkey, message);
console.log('Encrypted:', encrypted);

// Decrypt a message
const decrypted = await window.nostr.nip04.decrypt(recipientPubkey, encrypted);
console.log('Decrypted:', decrypted);
```

### Creating Encrypted DMs

```javascript
// Send an encrypted DM
const dmEvent = {
  kind: 4,
  created_at: Math.floor(Date.now() / 1000),
  tags: [['p', recipientPubkey]],
  content: await window.nostr.nip04.encrypt(recipientPubkey, 'Hello!')
};

const signedDM = await window.nostr.signEvent(dmEvent);
// Now publish to relays...
```

## Working with Relays

### Get User's Relay List

```javascript
// Get the user's configured relays
const relays = await window.nostr.getRelays();
console.log('User relays:', relays);

// Example output:
// {
//   'wss://relay.damus.io': { read: true, write: true },
//   'wss://nos.lol': { read: true, write: false },
//   'wss://relay.snort.social': { read: false, write: true }
// }
```

### Publishing to Relays

```javascript
async function publishToRelays(event) {
  // Sign the event first
  const signedEvent = await window.nostr.signEvent(event);
  
  // Get user's write relays
  const relays = await window.nostr.getRelays();
  const writeRelays = Object.entries(relays)
    .filter(([url, policy]) => policy.write)
    .map(([url]) => url);
  
  // Publish to each relay
  const results = await Promise.allSettled(
    writeRelays.map(url => publishToRelay(url, signedEvent))
  );
  
  console.log(`Published to ${results.filter(r => r.status === 'fulfilled').length}/${writeRelays.length} relays`);
}

async function publishToRelay(url, event) {
  // Placeholder function for publishing to a relay.
  // You can implement this using a library like nostr-tools.
  // Example implementation:
  /*
  const relay = new Relay(url);
  await relay.connect();
  const result = await relay.publish(event);
  relay.close();
  return result;
  */
  throw new Error('publishToRelay is not implemented. Please provide your own implementation.');
}
```

## Practical Examples

### Building a Simple Note Composer

```html
<div>
  <textarea id="noteContent" placeholder="What's on your mind?"></textarea>
  <button onclick="publishNote()">Publish Note</button>
  <div id="status"></div>
</div>

<script>
async function publishNote() {
  const content = document.getElementById('noteContent').value;
  const status = document.getElementById('status');
  
  if (!content.trim()) {
    status.textContent = 'Please enter some content';
    return;
  }
  
  try {
    status.textContent = 'Publishing...';
    
    // Create the note event
    const event = {
      kind: 1,
      created_at: Math.floor(Date.now() / 1000),
      tags: extractHashtags(content),
      content: content
    };
    
    // Sign with Tungsten
    const signedEvent = await window.nostr.signEvent(event);
    
    // Publish to relays (implementation needed)
    await publishToUserRelays(signedEvent);
    
    status.textContent = 'Note published successfully!';
    document.getElementById('noteContent').value = '';
    
  } catch (error) {
    status.textContent = 'Error: ' + error.message;
  }
}

function extractHashtags(content) {
  const hashtags = content.match(/#\w+/g) || [];
  return hashtags.map(tag => ['t', tag.slice(1)]);
}
</script>
```

### Profile Viewer

```html
<div>
  <input id="npubInput" placeholder="Enter npub or hex pubkey">
  <button onclick="loadProfile()">Load Profile</button>
  <div id="profileDisplay"></div>
</div>

<script>
async function loadProfile() {
  const input = document.getElementById('npubInput').value;
  const display = document.getElementById('profileDisplay');
  
  try {
    // Convert npub to hex if needed
    const { nip19 } = await import(window.nostr.libs['nostr-tools']);
    let pubkey = input;
    
    if (input.startsWith('npub')) {
      const decoded = nip19.decode(input);
      pubkey = decoded.data;
    }
    
    // Query for profile events (kind 0)
    const profileEvents = await window.nostr.relay.query({
      kinds: [0],
      authors: [pubkey],
      limit: 1
    });
    
    if (profileEvents.length === 0) {
      display.innerHTML = 'Profile not found in local relay';
      return;
    }
    
    const profile = JSON.parse(profileEvents[0].content);
    display.innerHTML = `
      <h3>${profile.name || 'Anonymous'}</h3>
      <p>${profile.about || 'No bio'}</p>
      <p>NIP-05: ${profile.nip05 || 'Not verified'}</p>
      ${profile.picture ? `<img src="${profile.picture}" width="100">` : ''}
    `;
    
  } catch (error) {
    display.innerHTML = 'Error loading profile: ' + error.message;
  }
}
</script>
```

## Performance Tips

### Efficient Event Queries

```javascript
// Use specific filters to reduce data transfer
const recentNotes = await window.nostr.relay.query({
  kinds: [1],
  since: Math.floor(Date.now() / 1000) - 3600, // last hour
  limit: 50
});

// Count events before fetching if you only need the number
const noteCount = await window.nostr.relay.count({
  kinds: [1],
  authors: [await window.nostr.getPublicKey()]
});
```

### Caching Patterns

```javascript
// Cache frequently accessed data
const profileCache = new Map();

async function getCachedProfile(pubkey) {
  if (profileCache.has(pubkey)) {
    return profileCache.get(pubkey);
  }
  
  const events = await window.nostr.relay.query({
    kinds: [0],
    authors: [pubkey],
    limit: 1
  });
  
  if (events.length > 0) {
    const profile = JSON.parse(events[0].content);
    profileCache.set(pubkey, profile);
    return profile;
  }
  
  return null;
}
```

## Security Best Practices

### Validate Events

```javascript
function isValidEvent(event) {
  return (
    typeof event.kind === 'number' &&
    typeof event.created_at === 'number' &&
    typeof event.content === 'string' &&
    Array.isArray(event.tags) &&
    event.tags.every(tag => Array.isArray(tag) && tag.every(item => typeof item === 'string'))
  );
}

// Always validate before signing
async function safeSignEvent(event) {
  if (!isValidEvent(event)) {
    throw new Error('Invalid event format');
  }
  return await window.nostr.signEvent(event);
}
```

### Handle Permissions Gracefully

```javascript
async function requestNostrPermission(operation) {
  try {
    switch (operation) {
      case 'getPublicKey':
        return await window.nostr.getPublicKey();
      case 'signEvent':
        // Don't pre-call signEvent, just indicate capability
        return true;
      default:
        throw new Error('Unknown operation');
    }
  } catch (error) {
    if (error.message.includes('denied')) {
      console.log('User denied permission for', operation);
      return null;
    }
    throw error;
  }
}
```

## Next Steps

- Learn about [Browsing Nsites](browsing-nsites.md)
- Explore [Blossom File Sharing](blossom-features.md)
- Check [Privacy and Security](privacy-security.md) settings
- See [Developer Documentation](../developer/README.md) for advanced usage