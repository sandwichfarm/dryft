# Low Level Design Document: macOS Build Process
## Tungsten Browser - macOS Platform Build Implementation

### 1. Component Overview

The macOS build process provides native compilation for Intel (x64) and Apple Silicon (arm64) architectures, producing universal binaries, DMG installers, and notarized applications. This implementation leverages Xcode toolchain while supporting both local development and CI/CD workflows with proper code signing and notarization.

### 2. Build Environment Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    macOS Build Environment                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                  Build Dependencies                       │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ Xcode 14+    │  │ macOS SDK     │  │ depot_tools│   │   │
│  │  │              │  │ 13.0+         │  │            │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ Python 3.8+ │  │ Homebrew      │  │ Rosetta 2  │   │   │
│  │  │              │  │               │  │ (optional) │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      Build Scripts Layer                         │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   macOS Build Scripts                     │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ build.py     │  │ setup_mac.sh  │  │ codesign.sh│   │   │
│  │  │ (unified)    │  │               │  │            │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ create_dmg.sh│  │ notarize.sh   │  │ universal  │   │   │
│  │  │              │  │               │  │ _binary.sh │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      Xcode Toolchain Layer                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   Compiler & Tools                        │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ Apple Clang  │  │ ld64 Linker   │  │ dsymutil   │   │   │
│  │  │              │  │               │  │            │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ codesign     │  │ xcrun         │  │ lipo       │   │   │
│  │  │              │  │               │  │ (universal)│   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                    Distribution Layer                            │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                  Package Generators                       │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ DMG Creator  │  │ PKG Builder   │  │ App Bundle │   │   │
│  │  │              │  │               │  │ Packager   │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  │  ┌──────────────┐  ┌───────────────┐                   │   │
│  │  │ Notarization │  │ Sparkle       │                   │   │
│  │  │ Service      │  │ Updates       │                   │   │
│  │  └──────────────┘  └───────────────┘                   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 3. Build Prerequisites

#### 3.1 System Requirements
```bash
# Minimum system specifications
- macOS: 12.0+ (Monterey) for Intel, 13.0+ (Ventura) for Apple Silicon
- Xcode: 14.0+ (15.0+ recommended)
- CPU: 4+ cores (8+ recommended, Apple Silicon preferred)
- RAM: 16GB minimum (32GB+ recommended)
- Disk: 150GB free space (SSD required)

# Supported architectures
- x86_64 (Intel Macs)
- arm64 (Apple Silicon - M1/M2/M3)
- Universal Binary (x86_64 + arm64)
```

#### 3.2 Dependency Installation Script
```bash
#!/bin/bash
# scripts/install-build-deps-macos.sh

set -e

# Check macOS version
MACOS_VERSION=$(sw_vers -productVersion)
echo "macOS version: $MACOS_VERSION"

# Check if Xcode is installed
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Error: Xcode is not installed. Please install from App Store."
    exit 1
fi

# Accept Xcode license
sudo xcodebuild -license accept 2>/dev/null || true

# Install Xcode command line tools
xcode-select --install 2>/dev/null || true

# Install Homebrew if not present
if ! command -v brew &> /dev/null; then
    echo "Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    
    # Add Homebrew to PATH based on architecture
    if [[ $(uname -m) == "arm64" ]]; then
        echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
        eval "$(/opt/homebrew/bin/brew shellenv)"
    else
        echo 'eval "$(/usr/local/bin/brew shellenv)"' >> ~/.zprofile
        eval "$(/usr/local/bin/brew shellenv)"
    fi
fi

# Update Homebrew
brew update

# Install build dependencies
echo "Installing build dependencies..."
brew install python@3.11 ninja ccache

# Install depot_tools
if [ ! -d "$HOME/depot_tools" ]; then
    echo "Installing depot_tools..."
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git $HOME/depot_tools
fi

# Add depot_tools to PATH
export PATH="$HOME/depot_tools:$PATH"
echo 'export PATH="$HOME/depot_tools:$PATH"' >> ~/.zshrc

# Python dependencies
pip3 install --user httplib2 six

# Setup ccache
echo "Configuring ccache..."
ccache --max-size=10G
echo 'export CCACHE_DIR=$HOME/.ccache' >> ~/.zshrc

# Rosetta 2 for Apple Silicon (if needed for x86_64 builds)
if [[ $(uname -m) == "arm64" ]]; then
    echo "Checking Rosetta 2..."
    if ! pgrep -q oahd; then
        echo "Installing Rosetta 2..."
        softwareupdate --install-rosetta --agree-to-license
    fi
fi

echo "macOS build dependencies installed successfully!"
echo "Please restart your terminal to apply environment changes."
```

### 4. Unified Build Script Implementation

```python
#!/usr/bin/env python3
# build.py - macOS platform support

import argparse
import os
import subprocess
import sys
import platform
from pathlib import Path

class MacOSBuilder:
    def __init__(self, args):
        self.args = args
        self.src_dir = Path(__file__).parent.parent
        self.out_dir = self.src_dir / "out" / args.build_type
        self.arch = args.arch or self.detect_arch()
        self.xcode_path = self.detect_xcode()
        
    def detect_arch(self):
        """Detect system architecture"""
        machine = platform.machine()
        if machine == 'x86_64':
            return 'x64'
        elif machine == 'arm64':
            return 'arm64'
        else:
            raise ValueError(f"Unsupported architecture: {machine}")
    
    def detect_xcode(self):
        """Detect Xcode installation path"""
        result = subprocess.run(
            ['xcode-select', '-p'],
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            raise RuntimeError("Xcode not found. Please install Xcode.")
        return Path(result.stdout.strip()).parent.parent
    
    def setup_environment(self):
        """Configure macOS build environment"""
        env = os.environ.copy()
        
        # Add depot_tools to PATH
        depot_tools = os.path.expanduser("~/depot_tools")
        env['PATH'] = f"{depot_tools}:{env['PATH']}"
        
        # Xcode environment
        env['DEVELOPER_DIR'] = str(self.xcode_path)
        
        # macOS SDK
        sdk_path = subprocess.run(
            ['xcrun', '--sdk', 'macosx', '--show-sdk-path'],
            capture_output=True,
            text=True
        ).stdout.strip()
        env['SDKROOT'] = sdk_path
        
        # ccache
        if Path('/opt/homebrew/bin/ccache').exists():
            env['CC_WRAPPER'] = '/opt/homebrew/bin/ccache'
        elif Path('/usr/local/bin/ccache').exists():
            env['CC_WRAPPER'] = '/usr/local/bin/ccache'
        
        # Tungsten-specific flags
        env['TUNGSTEN_ENABLE_NOSTR'] = '1'
        env['TUNGSTEN_ENABLE_LOCAL_RELAY'] = '1'
        env['TUNGSTEN_ENABLE_BLOSSOM'] = '1'
        
        return env
    
    def generate_args_gn(self):
        """Generate args.gn for macOS build"""
        args_content = f"""
# Generated macOS build configuration
target_os = "mac"
target_cpu = "{self.arch}"
is_official_build = {str(not self.args.debug).lower()}
is_debug = {str(self.args.debug).lower()}
is_component_build = {str(self.args.component_build).lower()}

# macOS-specific settings
mac_deployment_target = "10.15"
mac_min_system_version = "10.15"
enable_stripping = {str(not self.args.debug).lower()}
enable_dsyms = {str(self.args.debug).lower()}

# Code signing (local builds)
mac_signing_identity = ""
mac_installer_signing_identity = ""

# Optimization
symbol_level = {1 if self.args.debug else 0}
blink_symbol_level = {1 if self.args.debug else 0}

# Media codecs
proprietary_codecs = true
ffmpeg_branding = "Chrome"
enable_av1_decoder = true
enable_hevc_parser_and_hw_decoder = true

# Tungsten features
enable_nostr = true
enable_local_relay = true
enable_blossom_server = true
enable_nsite_support = true

# macOS features
use_system_xcode = true
"""
        
        # Universal binary support
        if self.args.universal:
            args_content += """
# Universal binary
target_cpu = "x64"
extra_cflags = ["-arch", "x86_64", "-arch", "arm64"]
extra_ldflags = ["-arch", "x86_64", "-arch", "arm64"]
"""
        
        # Write args.gn
        args_path = self.out_dir / "args.gn"
        args_path.parent.mkdir(parents=True, exist_ok=True)
        args_path.write_text(args_content)
        
        return args_path
    
    def build(self):
        """Execute macOS build"""
        print(f"Building for macOS {self.arch}...")
        env = self.setup_environment()
        
        # Configure build
        self.generate_args_gn()
        
        # Run gn gen
        subprocess.run([
            "gn", "gen", str(self.out_dir)
        ], env=env, cwd=self.src_dir, check=True)
        
        # Build targets
        targets = [
            "chrome",
            "chromedriver",
            "thorium_shell"
        ]
        
        # Use autoninja for build
        subprocess.run([
            "autoninja", "-C", str(self.out_dir)
        ] + targets, env=env, check=True)
        
        # Build universal binary if requested
        if self.args.universal:
            self.build_universal_binary()
    
    def build_universal_binary(self):
        """Build universal binary (Intel + Apple Silicon)"""
        print("Building universal binary...")
        
        # Build x64 if on arm64
        if self.arch == 'arm64':
            x64_builder = MacOSBuilder(self.args)
            x64_builder.arch = 'x64'
            x64_builder.out_dir = self.src_dir / "out" / f"{self.args.build_type}_x64"
            x64_builder.build()
        
        # Build arm64 if on x64
        elif self.arch == 'x64':
            arm64_builder = MacOSBuilder(self.args)
            arm64_builder.arch = 'arm64'
            arm64_builder.out_dir = self.src_dir / "out" / f"{self.args.build_type}_arm64"
            arm64_builder.build()
        
        # Create universal binary
        self.create_universal_app()
    
    def create_universal_app(self):
        """Combine x64 and arm64 builds into universal binary"""
        x64_dir = self.src_dir / "out" / f"{self.args.build_type}_x64"
        arm64_dir = self.src_dir / "out" / f"{self.args.build_type}_arm64"
        universal_dir = self.src_dir / "out" / f"{self.args.build_type}_universal"
        
        # Create universal directory
        universal_dir.mkdir(parents=True, exist_ok=True)
        
        # Copy app bundle structure
        import shutil
        app_name = "Tungsten.app"
        shutil.copytree(
            arm64_dir / app_name,
            universal_dir / app_name,
            dirs_exist_ok=True
        )
        
        # Create universal binaries using lipo
        binaries = [
            "Tungsten.app/Contents/MacOS/Tungsten",
            "Tungsten.app/Contents/Frameworks/Tungsten Framework.framework/Tungsten Framework",
            "Tungsten.app/Contents/Frameworks/Tungsten Helper.app/Contents/MacOS/Tungsten Helper"
        ]
        
        for binary in binaries:
            x64_binary = x64_dir / binary
            arm64_binary = arm64_dir / binary
            universal_binary = universal_dir / binary
            
            if x64_binary.exists() and arm64_binary.exists():
                subprocess.run([
                    "lipo", "-create",
                    str(x64_binary), str(arm64_binary),
                    "-output", str(universal_binary)
                ], check=True)
    
    def code_sign(self):
        """Code sign the application"""
        if self.args.skip_sign:
            return
        
        print("Code signing application...")
        
        app_path = self.out_dir / "Tungsten.app"
        
        # Get signing identity
        identity = os.environ.get('TUNGSTEN_SIGNING_IDENTITY', '-')
        
        # Deep sign the app bundle
        subprocess.run([
            "codesign",
            "--deep",
            "--force",
            "--verify",
            "--verbose",
            "--sign", identity,
            "--options", "runtime",
            "--entitlements", str(self.src_dir / "build" / "mac" / "app.entitlements"),
            str(app_path)
        ], check=True)
        
        # Verify signature
        subprocess.run([
            "codesign", "--verify", "--deep", "--strict", str(app_path)
        ], check=True)
    
    def notarize(self):
        """Notarize the application with Apple"""
        if self.args.skip_notarize or self.args.debug:
            return
        
        print("Notarizing application...")
        
        app_path = self.out_dir / "Tungsten.app"
        
        # Get notarization credentials
        apple_id = os.environ.get('TUNGSTEN_APPLE_ID')
        team_id = os.environ.get('TUNGSTEN_TEAM_ID')
        password = os.environ.get('TUNGSTEN_APP_PASSWORD')
        
        if not all([apple_id, team_id, password]):
            print("Warning: Notarization credentials not found, skipping")
            return
        
        # Create ZIP for notarization
        zip_path = self.out_dir / "Tungsten-notarize.zip"
        subprocess.run([
            "ditto", "-c", "-k", "--keepParent",
            str(app_path), str(zip_path)
        ], check=True)
        
        # Submit for notarization
        result = subprocess.run([
            "xcrun", "notarytool", "submit",
            str(zip_path),
            "--apple-id", apple_id,
            "--team-id", team_id,
            "--password", password,
            "--wait"
        ], capture_output=True, text=True)
        
        if result.returncode == 0:
            # Staple the notarization
            subprocess.run([
                "xcrun", "stapler", "staple", str(app_path)
            ], check=True)
            print("Notarization completed successfully")
        else:
            print(f"Notarization failed: {result.stderr}")
    
    def create_dmg(self):
        """Create DMG installer"""
        if self.args.skip_package:
            return
        
        print("Creating DMG installer...")
        
        app_path = self.out_dir / "Tungsten.app"
        dmg_path = self.out_dir / f"Tungsten-{self.arch}.dmg"
        
        # Create temporary directory for DMG contents
        dmg_temp = self.out_dir / "dmg_temp"
        dmg_temp.mkdir(exist_ok=True)
        
        # Copy app to temp directory
        import shutil
        shutil.copytree(app_path, dmg_temp / "Tungsten.app", dirs_exist_ok=True)
        
        # Create Applications symlink
        os.symlink("/Applications", str(dmg_temp / "Applications"))
        
        # Create DMG
        subprocess.run([
            "hdiutil", "create",
            "-volname", "Tungsten Browser",
            "-srcfolder", str(dmg_temp),
            "-ov",
            "-format", "UDZO",
            str(dmg_path)
        ], check=True)
        
        # Clean up
        shutil.rmtree(dmg_temp)
        
        # Sign DMG
        if not self.args.skip_sign:
            identity = os.environ.get('TUNGSTEN_SIGNING_IDENTITY', '-')
            subprocess.run([
                "codesign", "--force", "--sign", identity, str(dmg_path)
            ], check=True)
        
        print(f"DMG created: {dmg_path}")
    
    def create_pkg(self):
        """Create PKG installer for enterprise deployment"""
        if not self.args.create_pkg:
            return
        
        print("Creating PKG installer...")
        
        app_path = self.out_dir / "Tungsten.app"
        pkg_path = self.out_dir / f"Tungsten-{self.arch}.pkg"
        
        # Build PKG
        subprocess.run([
            "pkgbuild",
            "--root", str(app_path.parent),
            "--install-location", "/Applications",
            "--identifier", "com.tungsten.browser",
            "--version", self.get_version(),
            "--sign", os.environ.get('TUNGSTEN_INSTALLER_IDENTITY', '-'),
            str(pkg_path)
        ], check=True)
        
        print(f"PKG created: {pkg_path}")
    
    def get_version(self):
        """Get version from Chrome VERSION file"""
        version_file = self.src_dir / "chrome" / "VERSION"
        with open(version_file) as f:
            lines = f.readlines()
            return '.'.join(line.split('=')[1].strip() for line in lines)
```

### 5. CI/CD Integration

#### 5.1 GitHub Actions Workflow
```yaml
# .github/workflows/build-macos.yml
name: macOS Build

on:
  push:
    branches: [main, dev]
  pull_request:
    branches: [main]
  workflow_dispatch:
    inputs:
      arch:
        description: 'Architecture'
        required: true
        default: 'arm64'
        type: choice
        options:
          - x64
          - arm64
          - universal
      notarize:
        description: 'Notarize build'
        required: false
        default: false
        type: boolean

jobs:
  build:
    runs-on: ${{ matrix.runner }}
    strategy:
      matrix:
        include:
          - arch: x64
            runner: macos-13  # Intel
          - arch: arm64
            runner: macos-14  # M1
          - arch: universal
            runner: macos-14
    
    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          fetch-depth: 0
      
      - name: Setup Xcode
        uses: maxim-lobanov/setup-xcode@v1
        with:
          xcode-version: latest-stable
      
      - name: Install Dependencies
        run: |
          ./scripts/install-build-deps-macos.sh
          echo "$HOME/depot_tools" >> $GITHUB_PATH
      
      - name: Setup Build Cache
        uses: actions/cache@v3
        with:
          path: |
            ~/.ccache
            out/
          key: macos-${{ matrix.arch }}-${{ github.sha }}
          restore-keys: |
            macos-${{ matrix.arch }}-
      
      - name: Configure ccache
        run: |
          ccache --max-size=10G
          ccache --zero-stats
      
      - name: Build
        env:
          CCACHE_DIR: ~/.ccache
        run: |
          python3 build.py \
            --platform=macos \
            --arch=${{ matrix.arch }} \
            --jobs=8
      
      - name: Code Sign
        if: github.ref == 'refs/heads/main'
        env:
          TUNGSTEN_SIGNING_IDENTITY: ${{ secrets.MACOS_SIGNING_IDENTITY }}
          TUNGSTEN_INSTALLER_IDENTITY: ${{ secrets.MACOS_INSTALLER_IDENTITY }}
        run: |
          python3 build.py --sign-only --arch=${{ matrix.arch }}
      
      - name: Notarize
        if: github.event.inputs.notarize == 'true'
        env:
          TUNGSTEN_APPLE_ID: ${{ secrets.APPLE_ID }}
          TUNGSTEN_TEAM_ID: ${{ secrets.TEAM_ID }}
          TUNGSTEN_APP_PASSWORD: ${{ secrets.APP_PASSWORD }}
        run: |
          python3 build.py --notarize-only --arch=${{ matrix.arch }}
      
      - name: Create Installers
        run: |
          python3 build.py --package-only --arch=${{ matrix.arch }}
      
      - name: Upload Artifacts
        uses: actions/upload-artifact@v3
        with:
          name: tungsten-macos-${{ matrix.arch }}
          path: |
            out/Release/*.dmg
            out/Release/*.pkg
            out/Release_universal/*.dmg
```

### 6. macOS-Specific Features

#### 6.1 App Bundle Structure
```bash
# scripts/create_app_bundle.sh
#!/bin/bash

set -e

BUILD_DIR="$1"
APP_NAME="Tungsten"

# Create app bundle structure
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
CONTENTS="$APP_BUNDLE/Contents"
MACOS="$CONTENTS/MacOS"
RESOURCES="$CONTENTS/Resources"
FRAMEWORKS="$CONTENTS/Frameworks"

mkdir -p "$MACOS" "$RESOURCES" "$FRAMEWORKS"

# Copy main executable
cp "$BUILD_DIR/chrome" "$MACOS/$APP_NAME"

# Create Info.plist
cat > "$CONTENTS/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>Tungsten</string>
    <key>CFBundleIconFile</key>
    <string>app.icns</string>
    <key>CFBundleIdentifier</key>
    <string>com.tungsten.browser</string>
    <key>CFBundleName</key>
    <string>Tungsten</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>$(cat $BUILD_DIR/../../chrome/VERSION | tr '\n' '.' | sed 's/\.$//')</string>
    <key>CFBundleURLTypes</key>
    <array>
        <dict>
            <key>CFBundleURLName</key>
            <string>Nostr Protocol</string>
            <key>CFBundleURLSchemes</key>
            <array>
                <string>nostr</string>
            </array>
        </dict>
        <dict>
            <key>CFBundleURLName</key>
            <string>Web Protocol</string>
            <key>CFBundleURLSchemes</key>
            <array>
                <string>http</string>
                <string>https</string>
            </array>
        </dict>
    </array>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>LSMinimumSystemVersion</key>
    <string>10.15</string>
</dict>
</plist>
EOF

# Copy resources
cp "$BUILD_DIR"/*.pak "$RESOURCES/"
cp "$BUILD_DIR"/icudtl.dat "$RESOURCES/"
cp -r "$BUILD_DIR"/locales "$RESOURCES/"

# Copy frameworks
cp -r "$BUILD_DIR"/*.framework "$FRAMEWORKS/" 2>/dev/null || true

# Copy helper apps
for helper in "Tungsten Helper" "Tungsten Helper (GPU)" "Tungsten Helper (Plugin)" "Tungsten Helper (Renderer)"; do
    if [ -d "$BUILD_DIR/$helper.app" ]; then
        cp -r "$BUILD_DIR/$helper.app" "$FRAMEWORKS/"
    fi
done
```

#### 6.2 Entitlements Configuration
```xml
<!-- build/mac/app.entitlements -->
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <!-- Required for notarization -->
    <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
    <true/>
    <key>com.apple.security.cs.allow-jit</key>
    <true/>
    <key>com.apple.security.cs.disable-library-validation</key>
    <true/>
    
    <!-- Network access -->
    <key>com.apple.security.network.client</key>
    <true/>
    <key>com.apple.security.network.server</key>
    <true/>
    
    <!-- File access -->
    <key>com.apple.security.files.user-selected.read-write</key>
    <true/>
    <key>com.apple.security.files.downloads.read-write</key>
    <true/>
    
    <!-- Camera/Microphone (optional) -->
    <key>com.apple.security.device.camera</key>
    <true/>
    <key>com.apple.security.device.microphone</key>
    <true/>
    
    <!-- Local relay server -->
    <key>com.apple.security.network.server</key>
    <true/>
</dict>
</plist>
```

### 7. Sparkle Update Framework

#### 7.1 Sparkle Integration
```objective-c
// chrome/browser/mac/sparkle_updater.mm
#import <Sparkle/Sparkle.h>
#import "chrome/browser/mac/sparkle_updater.h"

@interface TungstenSparkleDelegate : NSObject <SPUUpdaterDelegate>
@end

@implementation TungstenSparkleDelegate

- (NSSet<NSString *> *)allowedChannelsForUpdater:(SPUUpdater *)updater {
  NSString* channel = GetUpdateChannel();
  return [NSSet setWithObject:channel];
}

- (void)updater:(SPUUpdater *)updater 
        didFinishLoadingAppcast:(SUAppcast *)appcast {
  LOG(INFO) << "Sparkle: Loaded appcast with " 
            << [[appcast items] count] << " items";
}

@end

namespace tungsten {

void InitializeSparkleUpdater() {
  static SPUStandardUpdaterController* updaterController = nil;
  if (updaterController) return;
  
  TungstenSparkleDelegate* delegate = 
      [[TungstenSparkleDelegate alloc] init];
  
  updaterController = [[SPUStandardUpdaterController alloc] 
      initWithStartingUpdater:YES 
      updaterDelegate:delegate 
      userDriverDelegate:nil];
  
  // Configure update settings
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults registerDefaults:@{
    @"SUEnableAutomaticChecks": @YES,
    @"SUScheduledCheckInterval": @(86400), // Daily
    @"SUAutomaticallyUpdate": @NO
  }];
}

}  // namespace tungsten
```

### 8. Performance Optimization

#### 8.1 Build Optimization Flags
```python
def get_macos_optimization_flags(arch):
    """Get macOS-specific optimization flags"""
    flags = {
        'use_lld': False,  # Use Apple's ld64
        'use_thin_lto': True,
        'enable_dsyms': False,  # Only for debug builds
        'mac_strip_release': True,
    }
    
    # Architecture-specific optimizations
    if arch == 'arm64':
        flags['arm_version'] = 8
        flags['arm_arch'] = 'armv8-a'
        flags['arm_tune'] = 'apple-m1'
    
    # Universal binary flags
    flags['mac_universal_binary'] = False  # Set per build
    
    return flags
```

### 9. Testing Framework

#### 9.1 macOS Test Runner
```bash
#!/bin/bash
# scripts/run_macos_tests.sh

set -e

BUILD_DIR="${1:-out/Release}"
FILTER="${2:-Tungsten*}"

# Set up test environment
export CHROME_HEADLESS=1
export TEST_LAUNCHER_JOBS=4

echo "Running unit tests..."
"$BUILD_DIR/unit_tests" \
    --gtest_filter="$FILTER" \
    --test-launcher-jobs=$TEST_LAUNCHER_JOBS

echo "Running browser tests..."
"$BUILD_DIR/browser_tests" \
    --gtest_filter="$FILTER" \
    --test-launcher-jobs=2 \
    --enable-pixel-output-in-tests

echo "Running integration tests..."
"$BUILD_DIR/integration_tests" \
    --gtest_filter="Nostr*:Blossom*"

# macOS-specific tests
echo "Running macOS integration tests..."
"$BUILD_DIR/mac_tests" \
    --gtest_filter="*SparkleUpdate*:*Keychain*"

echo "All tests passed!"
```

### 10. Troubleshooting Guide

#### 10.1 Common macOS Build Issues

| Issue | Solution |
|-------|----------|
| Xcode license not accepted | Run `sudo xcodebuild -license accept` |
| Missing command line tools | Run `xcode-select --install` |
| SDK version mismatch | Update Xcode from App Store |
| Code signing fails | Check keychain access and certificates |
| Notarization timeout | Check Apple Developer account status |
| Universal binary linking | Ensure both architectures built |

#### 10.2 Debug Commands
```bash
# Check Xcode installation
xcode-select -p
xcodebuild -version

# List available SDKs
xcodebuild -showsdks

# Check code signing identities
security find-identity -v -p codesigning

# Verify app bundle
codesign -dv --verbose=4 out/Release/Tungsten.app

# Check notarization status
xcrun notarytool history --apple-id $APPLE_ID --team-id $TEAM_ID

# Analyze binary architecture
lipo -info out/Release/Tungsten.app/Contents/MacOS/Tungsten

# Check for missing dylibs
otool -L out/Release/Tungsten.app/Contents/MacOS/Tungsten

# Test universal binary
arch -x86_64 out/Release_universal/Tungsten.app/Contents/MacOS/Tungsten --version
arch -arm64 out/Release_universal/Tungsten.app/Contents/MacOS/Tungsten --version
```

This comprehensive macOS build implementation provides full support for Intel and Apple Silicon architectures, including universal binary creation, code signing, notarization, and distribution packaging while maintaining compatibility with CI/CD automation.