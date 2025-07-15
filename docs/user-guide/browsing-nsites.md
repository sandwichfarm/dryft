# Browsing Nsites

Learn how to browse static websites hosted on Nostr using Tungsten Browser's native Nsite support.

## What are Nsites?

Nsites are static websites hosted directly on the Nostr network using kind 34128 events. They provide:

- **Decentralized Hosting**: No single point of failure
- **Censorship Resistance**: Cannot be taken down by traditional means  
- **Content Integrity**: Cryptographically signed by the author
- **Version Control**: Built-in history through event replacements

## Supported URL Formats

Tungsten supports several ways to access Nsites:

### 1. nostr:// Protocol URLs

Direct access using the Nostr protocol:
```
nostr://npub1abc123...def456/
nostr://npub1abc123...def456/about.html
nostr://npub1abc123...def456/blog/post1.html
```

### 2. Subdomain Format (Recommended)

Many Nsite hosting services use subdomain routing:
```
https://npub1abc123...def456.nsite.com/
https://npub1abc123...def456.nostr.band/
```

### 3. Path-based Format

Some services embed the npub in the path:
```
https://nsite.com/npub1abc123...def456/
https://nostr.band/nsite/npub1abc123...def456/
```

## Finding Nsites

### Discovery Methods

**Social Networks**: Look for Nsite links on Nostr social platforms:
- Damus, Primal, Nostrgram, Snort
- Search for #nsite hashtag
- Follow developers who build Nsites

**Directories**: Browse Nsite directories:
- [nsite.directory](https://nsite.directory) - Curated Nsite collection
- Nostr marketplace events (kind 30018)
- GitHub awesome-nostr lists

**Word of Mouth**: Ask the Nostr community for recommendations

### Example Nsites to Try

```
nostr://npub1example1...demo/           # Sample portfolio site
nostr://npub1example2...blog/           # Personal blog
nostr://npub1example3...store/          # E-commerce demo
```

## Browsing Experience

### Navigation

Nsites behave like regular websites within Tungsten:

- **Standard Navigation**: Use back/forward buttons, bookmarks, history
- **Right-click Menu**: Save pages, view source, inspect elements
- **Developer Tools**: Full DevTools support for debugging
- **Extensions**: Compatible with most browser extensions

### Performance Features

**Local Caching**: 
- Pages cached locally for faster subsequent visits
- Offline browsing when content is cached
- Automatic cache management

**Content Verification**:
- Cryptographic signature verification for all content
- Invalid signatures shown with security warnings
- Content integrity indicators in address bar

### Security Model

**Sandboxed Execution**:
- Nsites run in isolated environments
- Limited access to browser APIs
- No access to file system or hardware

**Content Security Policy**:
- Strict CSP prevents malicious code execution
- External resource loading controlled
- JavaScript execution in restricted context

## Content Types Supported

### Static Assets

**Web Standards**:
- HTML, CSS, JavaScript
- Images: PNG, JPEG, GIF, WebP, SVG
- Fonts: WOFF, WOFF2, TTF, OTF
- Documents: PDF (via browser viewer)

**Modern Web Features**:
- CSS Grid and Flexbox
- ES6+ JavaScript (where supported)
- Web Components
- Progressive Web App manifests

### Dynamic Content

**Client-side Processing**:
- Static site generators (Jekyll, Hugo output)
- Single Page Applications (React, Vue, Angular)
- WebAssembly modules
- Service Workers (limited)

**Limitations**:
- No server-side processing
- No database connections
- No file uploads to external servers
- Limited localStorage access

## Troubleshooting Nsite Access

### Common Issues

**Site Not Loading**:
```
Symptoms: "Nsite not found" or loading errors
Solutions:
1. Verify the npub format is correct
2. Check if the user has published kind 34128 events
3. Try accessing through different hosting services
4. Check network connectivity
```

**Slow Loading**:
```
Symptoms: Pages take long time to load
Solutions:
1. Clear browser cache and try again
2. Check if user's relay list is accessible
3. Try during off-peak hours
4. Switch to different Nsite hosting service
```

**Content Security Errors**:
```
Symptoms: Scripts blocked, resources not loading
Solutions:
1. Check browser console for CSP violations
2. Verify content signatures are valid
3. Ensure external resources are allowed
4. Contact site author about CSP configuration
```

### Debug Commands

Open Developer Tools (F12) and check:

```javascript
// Check Nsite loading status
console.log('Current URL:', window.location.href);
console.log('Nsite metadata:', window.nsite);

// Verify content integrity
console.log('Content signature valid:', window.nsite?.signatureValid);

// Check cached content
console.log('Cached files:', window.nsite?.cache?.list());
```

## Creating Nsite Bookmarks

### Save Frequently Visited Nsites

**Bookmark Bar**:
1. Navigate to the Nsite
2. Click the star icon in address bar
3. Choose "Bookmark Bar" as location
4. Edit name to be more descriptive

**Bookmark Folders**:
- Create "Nsites" folder for organization
- Categorize by type: "Blogs", "Portfolios", "Stores"
- Use descriptive names instead of long npub strings

### Bookmark Organization

```
📁 Nsites
├── 📁 Blogs
│   ├── Alice's Tech Blog
│   ├── Bob's Travel Journal
│   └── Crypto Insights
├── 📁 Portfolios
│   ├── Designer Portfolio - Charlie
│   └── Developer Showcase - Dana
└── 📁 Tools & Apps
    ├── Nostr Calculator
    └── Bitcoin Price Tracker
```

## Privacy and Security

### Privacy Benefits

**No Tracking**: 
- Nsites cannot use traditional tracking methods
- No cookies from ad networks
- No server-side analytics by default

**Decentralized Access**:
- Access through multiple relay servers
- No single point of monitoring
- Geographic distribution

### Security Considerations

**Content Trust**:
- Always verify the author's npub
- Check for verification badges (NIP-05)
- Be cautious with unfamiliar authors

**Malicious Content**:
- Nsites can still contain malicious JavaScript
- Don't enter sensitive information unless you trust the author
- Use standard web security practices

**External Resources**:
- Some Nsites may load external resources
- Check what external domains are being accessed
- Be aware of privacy implications

## Advanced Features

### Developer Mode

Enable enhanced debugging for Nsite development:

```javascript
// Enable in DevTools console
localStorage.setItem('tungsten.nsite.debug', 'true');

// View Nsite metadata
console.table(window.nsite?.metadata);

// Inspect content loading
window.nsite?.events?.forEach(event => {
  console.log(`${event.tags.find(t => t[0] === 'd')[1]}: ${event.content.length} bytes`);
});
```

### Content Caching

**Cache Management**:
- Settings → Privacy and Security → Clear Browsing Data
- Select "Nsite Content" to clear cached Nsite files
- Individual site cache clearing in DevTools

**Cache Settings**:
```javascript
// Adjust cache settings (advanced users)
window.nsite?.cache?.setMaxSize(100 * 1024 * 1024); // 100MB
window.nsite?.cache?.setTTL(24 * 60 * 60 * 1000);   // 24 hours
```

### Offline Access

**Enable Offline Mode**:
1. Visit Nsite while online
2. Content automatically cached
3. Access when offline through bookmarks
4. Limited functionality without network

**Offline Indicators**:
- Address bar shows offline status
- Some features may be disabled
- Content may be stale

## Integration with Nostr Features

### Social Integration

**Share Nsites**:
- Use Tungsten's built-in sharing to post Nsite links
- Include descriptions and screenshots
- Tag with #nsite for discovery

**Author Following**:
- Follow Nsite authors on Nostr social networks
- Get notified of new content updates
- Engage with authors directly

### Content Updates

**Automatic Updates**:
- Tungsten checks for content updates periodically
- New versions downloaded automatically
- Notification when updates are available

**Manual Refresh**:
- Ctrl+Shift+R for hard refresh
- Bypasses cache and fetches latest content
- Useful for development and testing

## Performance Tips

### Optimal Browsing

**Network Settings**:
- Ensure good connectivity to Nostr relays
- Use relays geographically close to you
- Configure multiple backup relays

**Browser Settings**:
- Enable hardware acceleration
- Increase cache size for better performance
- Disable unnecessary extensions while browsing Nsites

**Content Optimization**:
- Authors should optimize images and assets
- Use compression for faster loading
- Minimize external dependencies

## Future Developments

### Planned Features

**Enhanced Discovery**:
- Built-in Nsite directory
- Category-based browsing
- Recommendation engine

**Better Integration**:
- Native Nsite publishing tools
- Integration with Nostr social features
- Enhanced offline capabilities

**Performance Improvements**:
- Faster content loading
- Better caching strategies
- Optimized relay selection

### Community Involvement

**Feedback and Suggestions**:
- Report issues with specific Nsites
- Suggest new features for Nsite browsing
- Participate in Tungsten development discussions

**Content Creation**:
- Build your own Nsites
- Share with the community
- Help improve the ecosystem

For more information on creating Nsites, see the [Developer Documentation](../developer/README.md).