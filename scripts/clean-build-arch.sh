#!/bin/bash
set -e

# Clean build artifacts for a specific architecture
# This is useful when switching architectures or fixing SDK issues

CHROMIUM_DIR="${CHROMIUM_DIR:-$HOME/chromium/src}"
ARCH="${1:-all}"

echo "Cleaning Chromium build artifacts..."

case "$ARCH" in
    x64)
        echo "Cleaning x64 builds..."
        rm -rf "$CHROMIUM_DIR/out/"*-x64
        ;;
    arm64)
        echo "Cleaning arm64 builds..."
        rm -rf "$CHROMIUM_DIR/out/"*-arm64
        ;;
    all)
        echo "Cleaning all macOS builds..."
        rm -rf "$CHROMIUM_DIR/out/"*-macos-*
        ;;
    *)
        echo "Usage: $0 [x64|arm64|all]"
        exit 1
        ;;
esac

echo "Clean complete!"
echo "You can now build fresh with: make macos"