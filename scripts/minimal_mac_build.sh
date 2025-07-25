#!/bin/bash

# Minimal macOS build script for dryft
# This attempts to build with the existing source structure

set -e

echo "=== dryft Minimal macOS Build ==="
echo

# Check environment
if ! command -v gn &> /dev/null; then
    echo "Error: gn not found. Please ensure depot_tools is in your PATH"
    exit 1
fi

if ! command -v autoninja &> /dev/null; then
    echo "Error: autoninja not found. Please ensure depot_tools is in your PATH"
    exit 1
fi

# Create necessary directories and stub files
echo "Creating minimal build structure..."
mkdir -p src/build/config/{mac,compiler,sanitizers}
mkdir -p src/third_party/angle
mkdir -p src/testing

# Create minimal stub files to satisfy GN imports
touch src/third_party/angle/dotfile_settings.gni
echo "declare_args() {}" > src/build/config/compiler/compiler.gni
echo "declare_args() {}" > src/build/config/features.gni
echo "declare_args() {}" > src/build/config/sanitizers/sanitizers.gni

# Try to generate build files with minimal configuration
echo "Attempting to generate build files..."
cd src

# Use minimal args to avoid missing dependencies
cat > out/Release-macos-x64/args.gn <<EOF
# Minimal build configuration for macOS
is_debug = false
target_os = "mac"
target_cpu = "x64"

# dryft features
enable_nostr = true
enable_local_relay = true
enable_blossom_server = true
enable_nsite = true

# Disable features that require full Chromium checkout
use_goma = false
is_component_build = false
symbol_level = 0
enable_nacl = false
EOF

echo "Build configuration created."
echo
echo "Note: This is a minimal setup. For a full build, you'll need to:"
echo "1. Run: fetch --nohooks --no-history chromium"
echo "2. Run: gclient sync"
echo "3. Restore dryft modifications"
echo
echo "Or use the fetch_chromium_mac.sh script for automated setup."