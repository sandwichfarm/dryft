# Privacy and Security

Complete guide to privacy and security features in Tungsten Browser.

## Overview

Tungsten prioritizes security and privacy by design, with multiple layers of protection for your Nostr keys, personal data, and browsing activity.

## Key Security Features

### 🔐 Secure Key Storage

**Platform-Native Security**:
- **Windows**: Credential Manager with DPAPI encryption
- **macOS**: Keychain Services with hardware-backed security
- **Linux**: GNOME Keyring or KWallet integration
- **Fallback**: Encrypted file storage with master password

**Key Protection**:
- Private keys never exposed to web content
- Isolated key service process
- Hardware security module support (when available)
- Secure key derivation for encryption

### 🛡️ Permission System

**Granular Controls**:
- Per-origin permissions for each Nostr method
- Per-account permissions for multi-identity management
- Per-event-kind permissions for signing
- Time-based permission expiration

**Permission Types**:
```
📋 Permission Categories:
├── Profile Access (getPublicKey)
├── Event Signing (signEvent)
├── Relay Management (getRelays)
├── Encryption (nip04, nip44)
├── Local Relay Access
└── Blossom File Operations
```

### 🔒 Content Security

**Nsite Security**:
- Sandboxed execution environment
- Strict Content Security Policy (CSP)
- No access to browser APIs or file system
- Cryptographic content verification

**Local Services Security**:
- Localhost-only binding by default
- No external network access without permission
- Encrypted inter-process communication
- Resource usage limits

## Privacy Features

### 🕶️ Private Browsing

**Enhanced Private Mode**:
- No Nostr keys accessible in private windows
- Local relay disabled in private mode
- Blossom uploads blocked by default
- No persistent storage of Nostr data

**Privacy-First Defaults**:
- No telemetry collection by default
- No usage analytics sent to servers
- Minimal data retention policies
- User control over all data sharing

### 🌐 Network Privacy

**Relay Privacy**:
- Multiple relay connections to prevent tracking
- Rotating connection patterns
- Optional Tor/proxy support for relay connections
- No IP address logging by default

**Content Privacy**:
- Blossom files uploaded without metadata by default
- Optional anonymous upload mode
- No content scanning or analysis
- User control over content sharing

## Security Settings

### Account Security

**Access Control Settings**:
1. **Settings** → **Privacy and Security** → **Nostr Accounts**
2. **Passphrase Protection**:
   - Require passphrase for signing operations
   - Auto-lock after inactivity
   - Biometric unlock (where supported)
3. **Account Isolation**:
   - Separate permissions per account
   - Account-specific relay configurations
   - Isolated storage per identity

**Advanced Security Options**:
```json
{
  "tungsten.security": {
    "require_passphrase": true,
    "passphrase_timeout": 300,
    "max_failed_attempts": 3,
    "lockout_duration": 600,
    "biometric_unlock": true,
    "hardware_security": "preferred"
  }
}
```

### Permission Management

**Configure Default Permissions**:
1. **Settings** → **Privacy and Security** → **Site Permissions**
2. **Nostr API Permissions**:
   - **Ask (Recommended)**: Prompt for each new operation
   - **Allow**: Automatically grant permissions (risky)
   - **Block**: Deny all Nostr access

**Per-Site Permission Review**:
```javascript
// View current permissions for a site
console.log('Current permissions:', await navigator.permissions.query({
  name: 'nostr',
  origin: window.location.origin
}));

// Request specific permissions
const permission = await navigator.permissions.request({
  name: 'nostr',
  methods: ['getPublicKey', 'signEvent']
});
```

### Content Security

**CSP Configuration**:
```
Default CSP for Nsites:
default-src 'self';
script-src 'self' 'unsafe-inline';
style-src 'self' 'unsafe-inline';
img-src 'self' data: blossom:;
connect-src 'self' ws: wss:;
```

**Custom CSP Settings**:
1. **Settings** → **Advanced** → **Security**
2. **Content Security Policy**:
   - Strict (default): Maximum security
   - Balanced: Some external resources allowed
   - Permissive: For development only

## Security Best Practices

### 👤 Account Security

**Strong Account Management**:
- Use unique passphrases for each account
- Enable biometric unlock when available
- Regularly review account permissions
- Keep backup keys in secure, offline storage

**Multi-Account Strategy**:
```
🏢 Account Organization:
├── 🔒 High Security Account
│   ├── Long-term identity
│   ├── Financial transactions
│   └── Important communications
├── 🌐 Social Account  
│   ├── Public social media
│   ├── Content creation
│   └── Community participation
└── 🧪 Development Account
    ├── Testing applications
    ├── Experimental features
    └── Temporary interactions
```

### 🔐 Key Management

**Backup Security**:
- Store backups in multiple secure locations
- Use hardware security keys when possible
- Encrypt backup files with strong passwords
- Test backup recovery periodically

**Key Rotation**:
```javascript
// Create new account for key rotation
async function rotateKeys() {
  // 1. Create new account
  const newAccount = await createNewAccount();
  
  // 2. Update profile to point to new key
  await publishKeyRotationEvent(newAccount.pubkey);
  
  // 3. Gradually migrate to new identity
  // 4. Revoke old key after transition period
}
```

### 🌐 Browsing Security

**Safe Nsite Browsing**:
- Verify author identity before trusting content
- Check for NIP-05 verification badges
- Be cautious with unfamiliar or suspicious content
- Don't enter sensitive information on untrusted Nsites

**External Resource Awareness**:
```javascript
// Monitor external resource loading
const observer = new MutationObserver((mutations) => {
  mutations.forEach((mutation) => {
    mutation.addedNodes.forEach((node) => {
      if (node.src && !node.src.startsWith('blossom://')) {
        console.warn('External resource loaded:', node.src);
      }
    });
  });
});

observer.observe(document.body, {
  childList: true,
  subtree: true
});
```

## Advanced Security Configuration

### Enterprise Security

**Policy-Based Configuration**:
```json
{
  "tungsten.enterprise.security": {
    "force_passphrase": true,
    "min_passphrase_length": 12,
    "require_hardware_security": true,
    "allowed_origins": ["https://*.company.com"],
    "blocked_origins": ["*"],
    "max_account_count": 2,
    "audit_logging": true
  }
}
```

**Audit and Compliance**:
- Comprehensive audit logging
- Permission change tracking
- Failed authentication monitoring
- Compliance reporting features

### Developer Security

**Secure Development**:
```javascript
// Validate all event data
function validateEventSecurity(event) {
  // Check event size limits
  if (JSON.stringify(event).length > 100000) {
    throw new Error('Event too large');
  }
  
  // Validate content for suspicious patterns
  if (containsSuspiciousContent(event.content)) {
    throw new Error('Suspicious content detected');
  }
  
  // Check tag limits
  if (event.tags.length > 100) {
    throw new Error('Too many tags');
  }
  
  return true;
}

// Rate limiting for security
class SecurityRateLimiter {
  constructor() {
    this.attempts = new Map();
  }
  
  checkRate(operation, limit = 10, window = 60000) {
    const now = Date.now();
    const key = `${operation}-${Math.floor(now / window)}`;
    
    const count = this.attempts.get(key) || 0;
    if (count >= limit) {
      throw new Error('Rate limit exceeded');
    }
    
    this.attempts.set(key, count + 1);
    return true;
  }
}
```

### Network Security

**Secure Relay Connections**:
```javascript
// Configure secure relay connections
const secureRelayConfig = {
  enableTLS: true,
  verifySSL: true,
  allowSelfSigned: false,
  maxConnections: 10,
  connectionTimeout: 30000,
  authRequired: false
};

// Tor/Proxy configuration for enhanced privacy
const privacyConfig = {
  useProxy: true,
  proxyType: 'socks5',
  proxyHost: '127.0.0.1',
  proxyPort: 9050, // Tor default
  rotateConnections: true
};
```

## Security Monitoring

### Built-in Monitoring

**Security Alerts**:
Tungsten monitors for suspicious activity:
- Multiple failed authentication attempts
- Unusual permission requests
- Suspicious event patterns
- Potential phishing attempts

**Security Dashboard**:
1. **Settings** → **Privacy and Security** → **Security Dashboard**
2. View recent security events
3. Review permission changes
4. Monitor key usage

### User Monitoring

**Regular Security Checks**:
```javascript
// Check account security status
async function securityHealthCheck() {
  const issues = [];
  
  // Check for weak passphrases
  if (!await checkPassphraseStrength()) {
    issues.push('Weak passphrase detected');
  }
  
  // Check for overly permissive settings
  const permissions = await getGlobalPermissions();
  if (permissions.defaultPolicy === 'allow') {
    issues.push('Overly permissive default permissions');
  }
  
  // Check for old accounts
  const accounts = await getAccounts();
  const oldAccounts = accounts.filter(acc => 
    Date.now() - acc.created > 365 * 24 * 60 * 60 * 1000
  );
  
  if (oldAccounts.length > 0) {
    issues.push(`${oldAccounts.length} accounts older than 1 year`);
  }
  
  return issues;
}
```

## Incident Response

### Security Incidents

**If Your Account is Compromised**:
1. **Immediate Actions**:
   - Revoke all permissions immediately
   - Change passphrase
   - Review recent activity
   - Create new account if necessary

2. **Investigation**:
   - Check security dashboard for suspicious activity
   - Review permission grants
   - Analyze recent events and signatures

3. **Recovery**:
   - Restore from secure backup
   - Update all affected profiles
   - Notify contacts of key change

### Emergency Procedures

**Key Compromise Recovery**:
```javascript
// Emergency key revocation
async function emergencyKeyRevocation() {
  // 1. Create revocation event
  const revocationEvent = {
    kind: 5, // Deletion event
    created_at: Math.floor(Date.now() / 1000),
    tags: [
      ['e', 'event-id-to-revoke'],
      ['reason', 'key-compromise']
    ],
    content: 'This key has been compromised and is no longer valid'
  };
  
  // 2. Sign with compromised key one last time
  const signedRevocation = await window.nostr.signEvent(revocationEvent);
  
  // 3. Broadcast to all relays
  await broadcastToAllRelays(signedRevocation);
  
  // 4. Delete local key
  await deleteAccount();
}
```

**Data Breach Response**:
1. Assess scope of breach
2. Secure remaining accounts
3. Notify affected users
4. Implement additional security measures
5. Monitor for further suspicious activity

## Security Updates

### Staying Secure

**Regular Updates**:
- Keep Tungsten updated to latest version
- Monitor security announcements
- Review and update security settings periodically
- Audit account permissions regularly

**Security Resources**:
- [Tungsten Security Blog](https://tungsten.security/blog)
- [Security Announcements](https://github.com/sandwichfarm/tungsten/security)
- [Report Security Issues](mailto:security@tungsten.browser)

For technical security details, see the [Developer Security Guide](../developer/security.md).