#!/bin/bash
# Setup build caching for Tungsten Browser builds

set -e

# Color codes
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m'

# Detect platform
PLATFORM=$(uname -s | tr '[:upper:]' '[:lower:]')
CACHE_DIR="$HOME/.cache/tungsten-build"

# Function to setup ccache
setup_ccache() {
    echo -e "${GREEN}Setting up ccache...${NC}"
    
    if ! command -v ccache &> /dev/null; then
        echo -e "${RED}ccache not found. Please install ccache first.${NC}"
        return 1
    fi
    
    # Configure ccache
    ccache --set-config=max_size=20G
    ccache --set-config=compression=true
    ccache --set-config=compression_level=6
    ccache --set-config=sloppiness=file_macro,locale,time_macros
    ccache --set-config=base_dir=$PWD
    
    # Create cache directory
    mkdir -p "$CACHE_DIR/ccache"
    export CCACHE_DIR="$CACHE_DIR/ccache"
    
    # Show ccache stats
    ccache -s
    
    echo -e "${GREEN}ccache configured with 20GB cache at $CCACHE_DIR${NC}"
}

# Function to setup sccache (Windows/Rust builds)
setup_sccache() {
    echo -e "${GREEN}Setting up sccache...${NC}"
    
    if ! command -v sccache &> /dev/null; then
        echo -e "${YELLOW}sccache not found. Installing sccache...${NC}"
        
        if [[ "$PLATFORM" == "darwin" ]]; then
            brew install sccache
        elif [[ "$PLATFORM" == "linux" ]]; then
            # Download sccache binary
            SCCACHE_URL="https://github.com/mozilla/sccache/releases/download/v${SCCACHE_VERSION}/sccache-v${SCCACHE_VERSION}-x86_64-unknown-linux-musl.tar.gz"
            
            curl -L "$SCCACHE_URL" | tar xz
            sudo mv sccache-*/sccache /usr/local/bin/
            rm -rf sccache-*
        fi
    fi
    
    # Configure sccache
    export SCCACHE_DIR="$CACHE_DIR/sccache"
    export SCCACHE_CACHE_SIZE="20G"
    
    mkdir -p "$SCCACHE_DIR"
    
    # Start sccache server
    sccache --start-server || true
    
    # Show sccache stats
    sccache -s
    
    echo -e "${GREEN}sccache configured with 20GB cache at $SCCACHE_DIR${NC}"
}

# Function to create build cache wrapper scripts
create_wrapper_scripts() {
    echo -e "${GREEN}Creating compiler wrapper scripts...${NC}"
    
    WRAPPER_DIR="$HOME/.local/bin"
    mkdir -p "$WRAPPER_DIR"
    
    # Create ccache wrappers for GCC/Clang
    cat > "$WRAPPER_DIR/cached-cc" << 'EOF'
#!/bin/bash
exec ccache cc "$@"
EOF
    
    cat > "$WRAPPER_DIR/cached-c++" << 'EOF'
#!/bin/bash
exec ccache c++ "$@"
EOF
    
    cat > "$WRAPPER_DIR/cached-gcc" << 'EOF'
#!/bin/bash
exec ccache gcc "$@"
EOF
    
    cat > "$WRAPPER_DIR/cached-g++" << 'EOF'
#!/bin/bash
exec ccache g++ "$@"
EOF
    
    cat > "$WRAPPER_DIR/cached-clang" << 'EOF'
#!/bin/bash
exec ccache clang "$@"
EOF
    
    cat > "$WRAPPER_DIR/cached-clang++" << 'EOF'
#!/bin/bash
exec ccache clang++ "$@"
EOF
    
    # Make wrapper scripts executable
    chmod +x "$WRAPPER_DIR"/cached-*
    
    echo -e "${GREEN}Wrapper scripts created in $WRAPPER_DIR${NC}"
    echo -e "${YELLOW}Add $WRAPPER_DIR to your PATH to use cached compilers${NC}"
}

# Function to configure GN args for caching
setup_gn_cache_args() {
    echo -e "${GREEN}Creating GN args for build caching...${NC}"
    
    cat > "$CACHE_DIR/cache_args.gn" << EOF
# Build cache configuration for Tungsten
# Include this in your args.gn: import("$CACHE_DIR/cache_args.gn")

# Enable ccache
cc_wrapper = "ccache"

# Faster builds with component build (debug only)
is_component_build = is_debug

# Use system libraries where possible (Linux)
use_system_libjpeg = true
use_system_libpng = true
use_system_zlib = true
use_system_harfbuzz = true
use_system_freetype = true

# Disable features not needed for development
enable_nacl = false
enable_remoting = false

# Optimize for build speed in debug
if (is_debug) {
  symbol_level = 1
  enable_iterator_debugging = false
}
EOF
    
    echo -e "${GREEN}GN cache args saved to $CACHE_DIR/cache_args.gn${NC}"
}

# Function to setup GitHub Actions cache
setup_github_actions_cache() {
    echo -e "${GREEN}Creating GitHub Actions cache configuration...${NC}"
    
    mkdir -p .github
    
    cat > .github/cache-config.yml << 'EOF'
# Cache configuration for GitHub Actions
# Include this in your workflow with:
# - uses: actions/cache@v4
#   with:
#     path: |
#       ~/.cache/ccache
#       ~/.cache/sccache
#       ~/.cache/tungsten-build
#       src/out/*/obj
#     key: ${{ runner.os }}-${{ matrix.arch }}-build-${{ hashFiles('**/*.gn*') }}
#     restore-keys: |
#       ${{ runner.os }}-${{ matrix.arch }}-build-

cache_paths:
  ccache: ~/.cache/ccache
  sccache: ~/.cache/sccache
  tungsten: ~/.cache/tungsten-build
  chromium_obj: src/out/*/obj
  
cache_size_limits:
  ccache: 20G
  sccache: 20G
  
environment_variables:
  CCACHE_DIR: ~/.cache/ccache
  SCCACHE_DIR: ~/.cache/sccache
  CCACHE_COMPRESS: "1"
  CCACHE_COMPRESSLEVEL: "6"
  CCACHE_MAXSIZE: "20G"
  SCCACHE_CACHE_SIZE: "20G"
EOF
    
    echo -e "${GREEN}GitHub Actions cache config saved${NC}"
}

# Function to show cache statistics
show_cache_stats() {
    echo -e "${GREEN}Cache Statistics:${NC}"
    
    if command -v ccache &> /dev/null; then
        echo -e "${YELLOW}ccache stats:${NC}"
        ccache -s
    fi
    
    if command -v sccache &> /dev/null; then
        echo -e "${YELLOW}sccache stats:${NC}"
        sccache -s
    fi
    
    if [ -d "$CACHE_DIR" ]; then
        echo -e "${YELLOW}Cache directory size:${NC}"
        du -sh "$CACHE_DIR"
    fi
}

# Function to clean cache
clean_cache() {
    echo -e "${YELLOW}Cleaning build cache...${NC}"
    
    if command -v ccache &> /dev/null; then
        ccache -C
    fi
    
    if command -v sccache &> /dev/null; then
        sccache --stop-server || true
        rm -rf "$SCCACHE_DIR"
    fi
    
    echo -e "${GREEN}Cache cleaned${NC}"
}

# Main function
main() {
    echo -e "${GREEN}Tungsten Build Cache Setup${NC}"
    echo
    
    case "${1:-setup}" in
        setup)
            setup_ccache
            if [[ "$PLATFORM" == "darwin" ]] || [[ "$PLATFORM" == "windows" ]]; then
                setup_sccache
            fi
            create_wrapper_scripts
            setup_gn_cache_args
            setup_github_actions_cache
            
            echo
            echo -e "${GREEN}Build cache setup complete!${NC}"
            echo
            echo "To use the cache, add these to your shell configuration:"
            echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
            echo "  export CCACHE_DIR=\"$CACHE_DIR/ccache\""
            echo "  export SCCACHE_DIR=\"$CACHE_DIR/sccache\""
            echo
            echo "For GN builds, add to your args.gn:"
            echo "  import(\"$CACHE_DIR/cache_args.gn\")"
            ;;
            
        stats)
            show_cache_stats
            ;;
            
        clean)
            clean_cache
            ;;
            
        *)
            echo "Usage: $0 [setup|stats|clean]"
            echo "  setup - Configure build caching"
            echo "  stats - Show cache statistics"
            echo "  clean - Clean build cache"
            exit 1
            ;;
    esac
}

main "$@"