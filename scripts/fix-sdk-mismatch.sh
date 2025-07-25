#!/bin/bash
set -e

# Fix SDK version mismatch in Chromium build
# This happens when Xcode is updated during a build

CHROMIUM_DIR="${CHROMIUM_DIR:-$HOME/chromium/src}"

echo "Fixing SDK version mismatches in Chromium build..."

# Find all object files that might have old SDK versions
echo "Searching for affected object files..."

# SwiftShader is commonly affected
if [ -d "$CHROMIUM_DIR/out" ]; then
    echo "Cleaning SwiftShader objects..."
    find "$CHROMIUM_DIR/out" -path "*/third_party/swiftshader/*" -name "*.o" -delete 2>/dev/null || true
    find "$CHROMIUM_DIR/out" -name "*swiftshader*.dylib*" -delete 2>/dev/null || true
    find "$CHROMIUM_DIR/out" -name "*swiftshader*.a" -delete 2>/dev/null || true
    
    # Specifically target the problematic files mentioned in the error
    echo "Cleaning specific problematic objects..."
    find "$CHROMIUM_DIR/out" -name "ETC_Decoder.o" -delete 2>/dev/null || true
    find "$CHROMIUM_DIR/out" -name "MCCodeEmitter.o" -delete 2>/dev/null || true
    
    # Clean all Device subdirectory objects
    find "$CHROMIUM_DIR/out" -path "*/Device/Device/*" -name "*.o" -delete 2>/dev/null || true
    find "$CHROMIUM_DIR/out" -path "*/swiftshader_llvm_most/*" -name "*.o" -delete 2>/dev/null || true
    
    # Also clean validation layers which often have similar issues
    echo "Cleaning Vulkan validation layer objects..."
    find "$CHROMIUM_DIR/out" -path "*/third_party/vulkan-validation-layers/*" -name "*.o" -delete 2>/dev/null || true
    find "$CHROMIUM_DIR/out" -name "*VkLayer*.dylib*" -delete 2>/dev/null || true
    
    # Clean any LTO temp files
    echo "Cleaning LTO temporary files..."
    find "$CHROMIUM_DIR/out" -name "ld-temp.o" -delete 2>/dev/null || true
    find "$CHROMIUM_DIR/out" -name "tmp*" -type f -delete 2>/dev/null || true
    
    echo "SDK mismatch cleanup complete!"
    echo ""
    echo "You can now resume the build with: make macos"
else
    echo "No Chromium build directory found at $CHROMIUM_DIR/out"
    exit 1
fi