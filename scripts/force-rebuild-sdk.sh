#!/bin/bash
set -e

# Force Chromium to rebuild with new SDK after Xcode update
# This is the proper way to handle SDK changes in Chromium

CHROMIUM_DIR="${CHROMIUM_DIR:-$HOME/chromium/src}"
BUILD_DIR="${1:-out/Release-macos-x64}"

echo "Forcing rebuild with new SDK..."

# Touch the mac_sdk.py to force it to re-detect SDK
touch "$CHROMIUM_DIR/build/config/mac/mac_sdk.py"

# Remove the SDK detection cache
rm -f "$CHROMIUM_DIR/$BUILD_DIR/sdk_info.json" 2>/dev/null || true

# Force GN to regenerate
echo "Regenerating build files..."
export PATH="$PATH:$HOME/depot_tools"
cd "$CHROMIUM_DIR" && gn gen "$BUILD_DIR" || {
    # If that fails, try with full paths
    echo "Trying with depot_tools gn..."
    ~/depot_tools/gn gen "$CHROMIUM_DIR/$BUILD_DIR" --root="$CHROMIUM_DIR"
}

echo "Build files regenerated with new SDK!"
echo "Resume build with: make macos"