# Troubleshooting Guide

Comprehensive troubleshooting guide for Tungsten Browser and its Nostr features.

## Quick Navigation

- [Common Issues](#common-issues) - Most frequently encountered problems
- [Installation Problems](#installation-problems) - Setup and installation issues
- [Nostr Feature Issues](#nostr-feature-issues) - NIP-07 and account problems
- [Local Services](#local-services) - Relay and Blossom server issues
- [Performance Issues](#performance-issues) - Speed and memory problems
- [Debug Procedures](#debug-procedures) - Advanced debugging techniques
- [FAQ](#faq) - Frequently asked questions
- [Getting Help](#getting-help) - Where to find additional support

## Common Issues

### 1. window.nostr is undefined

**Symptoms**: JavaScript shows `window.nostr` is undefined on websites

**Causes & Solutions**:

```javascript
// Check if Nostr is available
console.log('window.nostr available:', typeof window.nostr !== 'undefined');
```

**Solution A: Check Origin**
- Tungsten only injects `window.nostr` on `https://` and `http://localhost` origins
- Won't work on `chrome://`, `file://`, or other special schemes
- Test on a real website: `https://nostrgram.co`

**Solution B: Check Settings**
1. Open Settings → Privacy & Security → Site Permissions
2. Ensure "Nostr API" is enabled
3. Check if the specific site is blocked

**Solution C: Restart Browser**
- Close all Tungsten windows completely
- Restart Tungsten
- Test on a fresh tab

### 2. Permission Denied Errors

**Symptoms**: API calls throw "User denied permission" errors

**Debug Steps**:
```javascript
try {
  const pubkey = await window.nostr.getPublicKey();
  console.log('Success:', pubkey);
} catch (error) {
  console.error('Error type:', error.name);
  console.error('Error message:', error.message);
}
```

**Solutions**:

**Check Permission Settings**:
1. Settings → Privacy & Security → Site Permissions
2. Click "Nostr API" 
3. Find the website in question
4. Change permission to "Allow" or "Ask"

**Reset Site Permissions**:
1. Right-click in address bar → Site Information
2. Click "Reset permissions"
3. Refresh page and try again

**Check Account Status**:
1. Settings → Nostr Accounts
2. Ensure at least one account is active
3. Try switching to a different account

### 3. Local Relay Not Working

**Symptoms**: `window.nostr.relay.connected` is false

**Debug Commands**:
```javascript
console.log('Relay URL:', window.nostr.relay.url);
console.log('Connected:', window.nostr.relay.connected);
console.log('Event count:', window.nostr.relay.eventCount);
```

**Solutions**:

**Check Service Status**:
1. Open `ws://localhost:8081` in a WebSocket test tool
2. Should connect and accept WebSocket upgrade

**Port Conflicts**:
```bash
# Check if port 8081 is in use
netstat -an | grep 8081   # Linux/macOS
netstat -an | findstr 8081  # Windows

# Kill conflicting process if needed
lsof -ti:8081 | xargs kill -9  # Linux/macOS
```

**Restart Services**:
1. Settings → Advanced → Reset local services
2. Or restart Tungsten completely

### 4. Blossom Upload Failures

**Symptoms**: `window.blossom.upload()` fails or times out

**Debug Code**:
```javascript
try {
  const hash = await window.blossom.upload(file);
  console.log('Upload successful:', hash);
} catch (error) {
  console.error('Upload failed:', error.message);
  console.error('Error type:', error.name);
}
```

**Solutions**:

**Check File Size**:
- Default limit: 100MB per file
- Check: Settings → Advanced → Blossom Configuration
- Increase limit if needed

**Check Available Space**:
```javascript
// Check Blossom status
console.log('Storage used:', window.blossom.storageUsed);
console.log('Storage limit:', window.blossom.storageLimit);
```

**Network Issues**:
1. Test with small file first (< 1MB)
2. Check network connection
3. Try uploading different file types

## Installation Problems

### Windows Installation Issues

**"Windows protected your PC" Dialog**:
```
Solution:
1. Click "More info"
2. Click "Run anyway"
3. This happens because Tungsten is not yet code-signed
```

**Installation Fails with Error Code**:
```
Diagnostic Steps:
1. Run installer as Administrator
2. Check Windows Event Viewer for detailed errors
3. Temporarily disable antivirus
4. Ensure Windows is up to date
```

**Missing Visual C++ Redistributables**:
```
Error: VCRUNTIME140.dll missing
Solution:
1. Download Microsoft Visual C++ Redistributable
2. Install both x64 and x86 versions
3. Restart and try installation again
```

### macOS Installation Issues

**"Tungsten Browser cannot be opened"**:
```
Solution:
1. Right-click Tungsten in Applications
2. Select "Open" from context menu
3. Click "Open" in security dialog
```

**Quarantine Issues**:
```bash
# Remove quarantine attribute
xattr -dr com.apple.quarantine "/Applications/Tungsten Browser.app"

# Verify removal
xattr -l "/Applications/Tungsten Browser.app"
```

**macOS Version Compatibility**:
- Minimum: macOS 10.15 (Catalina)
- Check version: Apple Menu → About This Mac
- Update macOS if version is too old

### Linux Installation Issues

**Missing Dependencies**:
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install libnss3 libatk-bridge2.0-0 libxss1 libgtk-3-0 libgbm1

# Fedora/CentOS
sudo dnf install nss atk at-spi2-atk libXcomposite libXrandr gtk3

# Arch Linux
sudo pacman -S nss atk at-spi2-atk libxcomposite libxrandr gtk3
```

**AppImage Won't Run**:
```bash
# Make executable
chmod +x TungstenBrowser.AppImage

# Check FUSE
sudo apt install fuse  # Ubuntu/Debian
sudo dnf install fuse  # Fedora

# Alternative: Extract and run
./TungstenBrowser.AppImage --appimage-extract
./squashfs-root/AppRun
```

## Nostr Feature Issues

### Account Creation Problems

**Can't Create New Account**:
1. Check enterprise policies (may be disabled)
2. Settings → Nostr Accounts → "Add Account" should be available
3. Try restarting browser

**Key Import Failures**:
```javascript
// Test key format validation
const testKey = 'nsec1...'; // your key
console.log('Key format valid:', /^nsec1[a-z0-9]{58}$/.test(testKey));
```

**Supported formats**:
- nsec (bech32): `nsec1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq`
- Hex: `0000000000000000000000000000000000000000000000000000000000000000`
- Mnemonic: 12 or 24 word phrases

### Signing Issues

**Events Won't Sign**:
```javascript
// Debug signing
const event = {
  kind: 1,
  created_at: Math.floor(Date.now() / 1000),
  tags: [],
  content: 'Test event'
};

try {
  const signed = await window.nostr.signEvent(event);
  console.log('Signing successful');
} catch (error) {
  console.error('Signing failed:', error);
}
```

**Common signing errors**:
- `Invalid event format`: Check required fields (kind, created_at, tags, content)
- `User denied`: Check permission settings
- `No account active`: Ensure account is selected
- `Rate limited`: Wait and try again

### Multi-Account Issues

**Account Switching Not Working**:
1. Check if multiple accounts are configured
2. Look for account switcher in toolbar
3. Settings → Nostr Accounts → verify accounts are listed

**Account Data Lost**:
1. Check if browser profile was reset
2. Look for backup files in browser data directory
3. Re-import accounts from backup

## Local Services

### Local Relay Debugging

**Service Won't Start**:
```bash
# Check logs
tungsten --enable-logging --vmodule=local_relay*=2

# Check port availability
telnet localhost 8081
```

**Common startup issues**:
- Port 8081 already in use
- Insufficient disk space
- Database corruption
- Permission issues on data directory

**Database Issues**:
```
Symptoms: Queries fail or return no results
Solutions:
1. Settings → Advanced → "Reset Local Relay Database"
2. Or manually delete: ~/.tungsten/Default/relay.db
3. Restart browser to rebuild database
```

### Blossom Server Debugging

**Service Won't Start**:
```bash
# Test HTTP server
curl http://localhost:8080/status

# Expected response
{"status": "ok", "storage_used": 1234567, "files": 42}
```

**Upload Issues**:
- Check available disk space
- Verify file permissions on storage directory
- Test with different file sizes
- Check file type restrictions

**Download Issues**:
- Verify file hash is correct
- Check if file exists: `curl http://localhost:8080/files/<hash>`
- Look for network connectivity issues

## Performance Issues

### Browser Slow to Start

**Diagnosis**:
```bash
# Start with timing information
tungsten --enable-logging --log-level=0 --vmodule=startup*=2
```

**Common causes**:
- Large number of stored events in local relay
- Many Blossom files stored locally
- Corrupted profile data
- Insufficient system resources

**Solutions**:
1. Clean up local data: Settings → Advanced → Clear Browsing Data
2. Reset local services: Settings → Advanced → Reset Local Services
3. Use fresh profile: `tungsten --user-data-dir=/tmp/fresh-profile`

### High Memory Usage

**Monitor memory usage**:
1. Open Task Manager / Activity Monitor
2. Look for multiple Tungsten processes
3. Check memory usage over time

**Memory optimization**:
```json
// In preferences
{
  "tungsten.local_relay.cache_size": 268435456,  // 256MB
  "tungsten.blossom.cache_size": 536870912,      // 512MB
  "tungsten.performance.lazy_load_accounts": true
}
```

### Slow API Responses

**Measure API performance**:
```javascript
async function benchmarkAPI() {
  const start = performance.now();
  try {
    await window.nostr.getPublicKey();
    const end = performance.now();
    console.log(`getPublicKey took ${end - start}ms`);
  } catch (error) {
    console.error('API call failed:', error);
  }
}
```

**Performance targets**:
- `getPublicKey()`: < 50ms
- `signEvent()`: < 200ms
- Local relay queries: < 100ms
- Blossom uploads: Depends on file size

## Debug Procedures

### Enable Debug Logging

**Command line flags**:
```bash
# General debug logging
tungsten --enable-logging --v=1

# Specific module logging
tungsten --vmodule=nostr*=2,blossom*=2,local_relay*=2

# Log to file
tungsten --enable-logging --log-file=/tmp/tungsten.log
```

**Runtime logging**:
```javascript
// Enable in DevTools console
localStorage.setItem('tungsten.debug', 'true');

// Enable specific modules
localStorage.setItem('tungsten.debug.nostr', 'true');
localStorage.setItem('tungsten.debug.blossom', 'true');
```

### Check Browser State

```javascript
// Check Nostr state
console.log('Nostr object:', window.nostr);
console.log('Available methods:', Object.keys(window.nostr));

// Check local relay
console.log('Relay status:', {
  url: window.nostr.relay.url,
  connected: window.nostr.relay.connected,
  events: window.nostr.relay.eventCount
});

// Check Blossom
console.log('Blossom available:', typeof window.blossom !== 'undefined');
```

### Network Diagnostics

```bash
# Test WebSocket connection to local relay
wscat -c ws://localhost:8081

# Test HTTP connection to Blossom
curl -X GET http://localhost:8080/status

# Test external relay connectivity
wscat -c wss://relay.damus.io
```

### Database Diagnostics

**Local Relay Database**:
```bash
# Examine database (requires sqlite3)
sqlite3 ~/.tungsten/Default/relay.db
.tables
.schema events
SELECT COUNT(*) FROM events;
```

**Browser Database**:
1. Open DevTools → Application tab
2. Check Local Storage, Session Storage, IndexedDB
3. Look for tungsten-related entries

## FAQ

### General Questions

**Q: Is Tungsten safe to use?**
A: Yes, Tungsten is built on Chromium with enhanced security for Nostr features. Private keys are stored using your OS's secure storage system.

**Q: Can I use Tungsten as my main browser?**
A: Yes, Tungsten includes all standard browser features plus native Nostr support.

**Q: Do I need a Nostr extension with Tungsten?**
A: No, Tungsten has built-in Nostr support. Extensions may conflict and should be disabled.

### Feature Questions

**Q: Can I import accounts from other Nostr clients?**
A: Yes, you can import private keys in nsec, hex, or mnemonic formats.

**Q: Do Nostr websites work without setup?**
A: Yes, but you need at least one account configured to sign events.

**Q: Can I use multiple accounts on the same website?**
A: Yes, use the account switcher in the toolbar to change accounts per site.

### Technical Questions

**Q: Where are my private keys stored?**
A: In your OS's secure storage:
- Windows: Credential Manager
- macOS: Keychain
- Linux: GNOME Keyring or KWallet

**Q: Can I backup my accounts?**
A: Yes, go to Settings → Nostr Accounts → Export Account Data

**Q: What happens if I forget my passphrase?**
A: You'll need to use your private key backup to restore the account.

## Getting Help

### Community Support

- **GitHub Issues**: [Report bugs and request features](https://github.com/sandwichfarm/tungsten/issues)
- **Discord**: Join the Tungsten community server
- **Reddit**: r/TungstenBrowser for community discussions
- **Matrix**: #tungsten:matrix.org

### Professional Support

- **Enterprise Support**: Available for business deployments
- **Developer Support**: Technical support for application developers
- **Training**: Available for teams adopting Tungsten

### Before Reporting Issues

Please include:

1. **Version Information**:
   ```
   Tungsten version: (Help → About Tungsten)
   Operating System: 
   Browser flags used:
   ```

2. **Steps to Reproduce**:
   - What you were trying to do
   - What you expected to happen
   - What actually happened

3. **Debug Information**:
   ```bash
   # Generate debug info
   tungsten --dump-config > config.txt
   tungsten --log-file=debug.log --enable-logging --v=2
   ```

4. **Error Messages**:
   - Screenshots of error dialogs
   - Console output from DevTools
   - Log file excerpts

### Emergency Recovery

**If Tungsten won't start**:
```bash
# Reset to defaults
tungsten --reset-settings --no-default-browser-check

# Use safe mode
tungsten --disable-extensions --disable-plugins --disable-background-mode

# Fresh profile
tungsten --user-data-dir=/tmp/emergency-profile
```

**If accounts are lost**:
1. Check backup files in browser profile directory
2. Look for `.bak` files in settings folder
3. Use account recovery wizard: Settings → Advanced → Recover Accounts

**If data is corrupted**:
1. Export important data first if possible
2. Settings → Advanced → Reset Tungsten to Defaults
3. Re-import accounts and bookmarks