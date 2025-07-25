# Getting Started with dryft browser

This guide will help you install dryft browser and get started with its native Nostr features.

## System Requirements

### Minimum Requirements
- **Windows**: Windows 10 64-bit or later
- **macOS**: macOS 10.15 (Catalina) or later
- **Linux**: Ubuntu 18.04, Debian 10, or equivalent
- **RAM**: 4GB minimum, 8GB recommended
- **Storage**: 500MB available space
- **Network**: Internet connection for initial setup

### Recommended for Best Performance
- **RAM**: 8GB or more
- **Storage**: SSD with 2GB available space
- **Network**: Broadband internet connection

## Installation

### Windows

1. **Download**: Get the latest dryft installer from [releases page](https://github.com/sandwichfarm/dryft/releases)
2. **Run Installer**: Double-click `TungstenBrowserSetup.exe`
3. **Admin Rights**: Click "Yes" when prompted for administrator permissions
4. **Installation**: Follow the setup wizard (typically takes 2-3 minutes)
5. **Launch**: dryft will start automatically after installation

### macOS

1. **Download**: Get `TungstenBrowser.dmg` from [releases page](https://github.com/sandwichfarm/dryft/releases)
2. **Mount DMG**: Double-click the downloaded file
3. **Install**: Drag dryft browser to your Applications folder
4. **Security**: First launch requires right-click → "Open" due to macOS security
5. **Launch**: Find dryft in Applications or Spotlight

### Linux

#### Debian/Ubuntu (DEB Package)
```bash
# Download the .deb package
wget https://github.com/sandwichfarm/dryft-browser_amd64.deb

# Install
sudo dpkg -i dryft-browser_amd64.deb
sudo apt-get install -f  # Fix any dependency issues

# Launch
dryft-browser
```

#### RPM-based Distributions
```bash
# Download the .rpm package
wget https://github.com/sandwichfarm/dryft-browser.x86_64.rpm

# Install (Fedora/CentOS/RHEL)
sudo rpm -i dryft-browser.x86_64.rpm

# Launch
dryft-browser
```

#### AppImage (Universal)
```bash
# Download AppImage
wget https://github.com/sandwichfarm/dryft/releases/latest/download/TungstenBrowser.AppImage

# Make executable
chmod +x TungstenBrowser.AppImage

# Run
./TungstenBrowser.AppImage
```

## First Launch Setup

### 1. Welcome Screen
On first launch, you'll see the dryft welcome screen:
- **Import Data**: Option to import bookmarks/passwords from other browsers
- **Set as Default**: Option to make dryft your default browser
- **Privacy Settings**: Quick privacy configuration

### 2. Nostr Account Setup
dryft will guide you through Nostr account setup:

**Option A: Create New Account**
1. Click "Create New Nostr Account"
2. Choose a display name
3. dryft generates a secure key pair
4. Your public key (npub) and private key are shown
5. **IMPORTANT**: Back up your private key securely

**Option B: Import Existing Account**
1. Click "Import Existing Account"
2. Enter your private key (nsec format)
3. dryft will derive your public key
4. Confirm the account details

**Option C: Skip for Now**
1. Click "Skip" to use dryft without Nostr features
2. You can set up accounts later in Settings

### 3. Local Services Setup
dryft automatically configures local services:

- **Local Relay**: Starts on `ws://localhost:8081`
- **Blossom Server**: Starts on `http://localhost:8080`
- **Storage Location**: Uses `~/.dryft/` directory

You'll see a notification when services are ready (usually 10-15 seconds).

## Basic Usage

### Testing Nostr Integration

1. **Open Developer Tools**: Press `F12` or `Ctrl+Shift+I`
2. **Console Tab**: Click on the "Console" tab
3. **Test Commands**:
   ```javascript
   // Check if window.nostr is available
   console.log('Nostr available:', !!window.nostr);
   
   // Get your public key
   window.nostr.getPublicKey().then(pubkey => {
     console.log('Your pubkey:', pubkey);
   });
   
   // Check local relay
   console.log('Local relay:', window.nostr.relay.url);
   console.log('Relay connected:', window.nostr.relay.connected);
   ```

### Your First Nostr Website

Try visiting a Nostr-enabled website:

1. **Navigate** to `https://nostrgram.co` or `https://primal.net`
2. **Connect Wallet**: These sites should automatically detect dryft's built-in Nostr support
3. **No Extension Needed**: The sites work immediately without installing extensions

### Browse an Nsite

Try browsing a static website hosted on Nostr:

1. **Find an Nsite**: Look for `nostr://npub...` URLs on Nostr social networks
2. **Direct Navigation**: Paste the URL into dryft's address bar
3. **Subdomain Format**: Some sites use `npub123.nsite.example.com` format

## Next Steps

Now that dryft is installed and working:

1. **[Account Management](account-management.md)**: Learn to manage multiple Nostr accounts
2. **[Nostr Features](nostr-features.md)**: Explore all the built-in Nostr capabilities
3. **[Privacy & Security](privacy-security.md)**: Configure security settings
4. **[Blossom File Sharing](blossom-features.md)**: Use content-addressed file storage

## Troubleshooting Installation

### Windows Issues

**"Windows protected your PC" dialog:**
- Click "More info" then "Run anyway"
- This happens because dryft is not yet signed with Microsoft's expensive certificate

**Installation fails:**
- Run installer as Administrator
- Temporarily disable antivirus software
- Ensure Windows is up to date

### macOS Issues

**"dryft browser cannot be opened" dialog:**
- Right-click dryft in Applications
- Select "Open" from context menu
- Click "Open" in the security dialog

**Quarantine issues:**
```bash
# Remove quarantine attribute
xattr -dr com.apple.quarantine /Applications/dryft\ Browser.app
```

### Linux Issues

**Missing dependencies:**
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install libnss3 libatk-bridge2.0-0 libx11-xcb1 libxcomposite1 libxrandr2 libxss1 libgtk-3-0

# Fedora/CentOS
sudo dnf install nss atk at-spi2-atk libXcomposite libXrandr libXss gtk3
```

**Permission issues:**
```bash
# Ensure executable permissions
chmod +x dryft-browser

# For AppImage
chmod +x TungstenBrowser.AppImage
```

If you continue having issues, see the [Troubleshooting Guide](../troubleshooting/README.md) or [report an issue](https://github.com/sandwichfarm/dryft/issues).