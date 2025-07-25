#!/bin/bash
# Setup script for dryft browser development

set -e

echo "Setting up dryft browser development environment..."

# Check if we're in the right directory
if [ ! -f "README.md" ] || [ ! -d "src" ]; then
    echo "Error: This script must be run from the dryft root directory"
    exit 1
fi

# Create build output directory
echo "Creating build directories..."
mkdir -p src/out/Release

# Copy Tungsten build args
if [ ! -f "src/out/Release/args.gn" ]; then
    echo "Setting up build arguments..."
    cp tungsten_args.gn src/out/Release/args.gn
else
    echo "Build arguments already exist, skipping..."
fi

# Apply patches if needed
echo "Checking for patches to apply..."
if [ -f "src/chrome/browser/BUILD_nostr_integration.patch" ]; then
    echo "Applying Nostr integration patch..."
    cd src
    patch -p1 < chrome/browser/BUILD_nostr_integration.patch || true
    cd ..
fi

# Create symlinks for easier access
echo "Creating convenience symlinks..."
ln -sf src/components/nostr nostr_components || true
ln -sf src/chrome/browser/nostr chrome_nostr || true

# Generate build files
echo "Generating build files..."
cd src
gn gen out/Release
cd ..

echo "Tungsten setup complete!"
echo ""
echo "Next steps:"
echo "1. cd src"
echo "2. autoninja -C out/Release chrome"
echo ""
echo "To enable Nostr features in your build, ensure these are set in args.gn:"
echo "  enable_nostr = true"
echo "  enable_local_relay = true"
echo "  enable_blossom_server = true"