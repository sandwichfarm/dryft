#!/bin/bash

# Script to fetch Chromium source for macOS build
set -e

echo "=== dryft macOS Build Setup ==="
echo "This script will help set up the Chromium source for building dryft on macOS"
echo

# Check if we're in the right directory
if [ ! -f ".gclient" ]; then
    echo "Error: Must run from dryft root directory (where .gclient exists)"
    exit 1
fi

# Ensure depot_tools is in PATH
if ! command -v gclient &> /dev/null; then
    echo "Error: gclient not found. Please ensure depot_tools is in your PATH"
    exit 1
fi

# Backup existing src if it has dryft modifications
if [ -d "src" ] && [ -f "src/chrome/browser/nostr/BUILD.gn" ]; then
    echo "Backing up existing dryft source modifications..."
    mkdir -p dryft_backup
    cp -R src/chrome/browser/nostr dryft_backup/
    cp -R src/chrome/browser/blossom dryft_backup/
    cp -R src/chrome/browser/nsite dryft_backup/
    cp -R src/components/nostr dryft_backup/
    cp -R src/content/renderer/nostr dryft_backup/
    echo "Backup created in dryft_backup/"
fi

# Update .gclient to ensure mac is included
echo "Updating .gclient configuration..."
cat > .gclient <<EOF
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {},
  },
]

target_os = [ "mac" ]
target_cpu = [ "x64", "arm64" ]
EOF

# Disable depot_tools auto-update to avoid network issues
export DEPOT_TOOLS_UPDATE=0

# Move existing src to temporary location
if [ -d "src" ]; then
    echo "Moving existing src directory..."
    mv src src_old_tungsten
fi

# Fetch Chromium (this will take a while)
echo "Fetching Chromium source (this may take 30-60 minutes)..."
echo "Using shallow clone to reduce download size..."
fetch --nohooks --no-history chromium

# Run hooks for macOS
echo "Running gclient hooks for macOS..."
gclient runhooks

# Restore dryft modifications
if [ -d "tungsten_backup" ]; then
    echo "Restoring dryft modifications..."
    cp -R tungsten_backup/nostr src/chrome/browser/
    cp -R tungsten_backup/blossom src/chrome/browser/
    cp -R tungsten_backup/nsite src/chrome/browser/
    cp -R tungsten_backup/nostr src/components/
    cp -R tungsten_backup/nostr src/content/renderer/
    
    # Also restore BUILD.gn modifications if they exist
    if [ -f "src_old_dryft/chrome/browser/BUILD.gn" ]; then
        cp src_old_dryft/chrome/browser/BUILD.gn src/chrome/browser/
    fi
    if [ -f "src_old_dryft/chrome/browser/resources/BUILD.gn" ]; then
        cp src_old_dryft/chrome/browser/resources/BUILD.gn src/chrome/browser/resources/
    fi
    if [ -f "src_old_dryft/chrome/common/BUILD.gn" ]; then
        cp src_old_dryft/chrome/common/BUILD.gn src/chrome/common/
    fi
    if [ -f "src_old_dryft/chrome/test/BUILD.gn" ]; then
        cp src_old_dryft/chrome/test/BUILD.gn src/chrome/test/
    fi
fi

echo
echo "=== Setup Complete ==="
echo "You can now build dryft with:"
echo "  cd src"
echo "  gn gen out/Release --args='is_official_build=true enable_nostr=true enable_local_relay=true enable_blossom_server=true target_cpu=\"x64\"'"
echo "  autoninja -C out/Release chrome"
echo
echo "For Apple Silicon (M1/M2), use target_cpu=\"arm64\" instead"