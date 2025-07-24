# Low Level Design Document: Linux Build Process
## Tungsten Browser - Linux Platform Build Implementation

### 1. Component Overview

The Linux build process provides native compilation for x64 and ARM64 architectures, producing .deb, .rpm, and tarball packages. This implementation serves as the reference build configuration for other platforms and supports both local development and CI/CD workflows.

### 2. Build Environment Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Linux Build Environment                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                  Build Dependencies                       │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ build-essential│ │ Python 3.8+  │  │ depot_tools│   │   │
│  │  │ GCC/Clang    │  │              │  │            │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ X11/Wayland │  │ GTK3/4       │  │ NSS/NSPR   │   │   │
│  │  │ Libraries   │  │              │  │            │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      Build Scripts Layer                         │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    Core Build Scripts                     │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ build.py     │  │ setup.sh      │  │ version.sh │   │   │
│  │  │ (unified)    │  │               │  │            │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ trunk.sh     │  │ package.sh    │  │ args.gn    │   │   │
│  │  │              │  │               │  │ (generator)│   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                    Chromium Build System                         │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                      GN/Ninja                             │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ GN Generate  │  │ Ninja Build   │  │ Compiler   │   │   │
│  │  │              │  │               │  │ (GCC/Clang)│   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      Packaging Layer                             │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                  Package Generators                       │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ DEB Builder  │  │ RPM Builder   │  │ Tarball    │   │   │
│  │  │ (dpkg-deb)   │  │ (rpmbuild)    │  │ Creator    │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 3. Build Prerequisites

#### 3.1 System Requirements
```bash
# Minimum system specifications
- CPU: 4+ cores (8+ recommended)
- RAM: 8GB minimum (16GB+ recommended)
- Disk: 100GB free space
- OS: Ubuntu 20.04+ / Debian 10+ / Fedora 33+

# Supported architectures
- x86_64 (Intel/AMD 64-bit)
- aarch64 (ARM 64-bit)
```

#### 3.2 Dependency Installation Script
```bash
#!/bin/bash
# scripts/install-build-deps-linux.sh

set -e

# Detect distribution
if [ -f /etc/debian_version ]; then
    DISTRO="debian"
elif [ -f /etc/fedora-release ]; then
    DISTRO="fedora"
elif [ -f /etc/arch-release ]; then
    DISTRO="arch"
else
    echo "Unsupported distribution"
    exit 1
fi

# Common dependencies
COMMON_DEPS="git python3 python3-pip ninja-build pkg-config"

# Distribution-specific installation
case $DISTRO in
    debian)
        sudo apt-get update
        sudo apt-get install -y \
            $COMMON_DEPS \
            build-essential \
            clang \
            cmake \
            curl \
            gperf \
            libasound2-dev \
            libatk-bridge2.0-dev \
            libatspi2.0-dev \
            libcups2-dev \
            libdbus-1-dev \
            libdrm-dev \
            libgtk-3-dev \
            libnss3-dev \
            libpulse-dev \
            libxss-dev \
            libxtst-dev \
            xvfb
        ;;
    fedora)
        sudo dnf install -y \
            $COMMON_DEPS \
            @development-tools \
            clang \
            cmake \
            cups-devel \
            dbus-devel \
            gtk3-devel \
            libdrm-devel \
            nss-devel \
            pulseaudio-libs-devel
        ;;
    arch)
        sudo pacman -Syu --needed \
            $COMMON_DEPS \
            base-devel \
            clang \
            cmake \
            gtk3 \
            nss \
            alsa-lib \
            libcups
        ;;
esac

# Install depot_tools
if [ ! -d "$HOME/depot_tools" ]; then
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git $HOME/depot_tools
fi
export PATH="$HOME/depot_tools:$PATH"

# Python dependencies
pip3 install --user httplib2 six
```

### 4. Unified Build Script Implementation

```python
#!/usr/bin/env python3
# build.py - Unified Linux build script

import argparse
import os
import subprocess
import sys
import multiprocessing
from pathlib import Path

class LinuxBuilder:
    def __init__(self, args):
        self.args = args
        self.src_dir = Path(__file__).parent.parent
        self.out_dir = self.src_dir / "out" / args.build_type
        self.arch = args.arch or self.detect_arch()
        self.jobs = args.jobs or multiprocessing.cpu_count()
        
    def detect_arch(self):
        """Detect system architecture"""
        machine = os.uname().machine
        if machine in ['x86_64', 'amd64']:
            return 'x64'
        elif machine in ['aarch64', 'arm64']:
            return 'arm64'
        else:
            raise ValueError(f"Unsupported architecture: {machine}")
    
    def setup_environment(self):
        """Configure build environment variables"""
        env = os.environ.copy()
        env['PATH'] = f"{os.path.expanduser('~/depot_tools')}:{env['PATH']}"
        env['CHROMIUM_BUILDTOOLS_PATH'] = str(self.src_dir / 'buildtools')
        
        # Tungsten-specific flags
        env['TUNGSTEN_ENABLE_NOSTR'] = '1'
        env['TUNGSTEN_ENABLE_LOCAL_RELAY'] = '1'
        env['TUNGSTEN_ENABLE_BLOSSOM'] = '1'
        
        return env
    
    def generate_args_gn(self):
        """Generate args.gn for Linux build"""
        args_content = f"""
# Generated Linux build configuration
target_os = "linux"
target_cpu = "{self.arch}"
is_official_build = {str(not self.args.debug).lower()}
is_debug = {str(self.args.debug).lower()}
is_component_build = {str(self.args.component_build).lower()}

# Optimization flags
enable_nacl = false
symbol_level = {1 if self.args.debug else 0}
blink_symbol_level = {1 if self.args.debug else 0}
enable_iterator_debugging = false

# Media codecs
proprietary_codecs = true
ffmpeg_branding = "Chrome"
enable_hevc_demuxing = true
enable_dolby_vision_demuxing = true
enable_platform_encrypted_hevc = true
enable_platform_encrypted_dolby_vision = true
enable_platform_hevc = true
enable_platform_dolby_vision = true

# Tungsten features
enable_nostr = true
enable_local_relay = true
enable_blossom_server = true
enable_nsite_support = true

# Linux-specific
use_sysroot = true
use_gold = true
use_lld = false
use_vaapi = true
gtk_version = 3

# Instruction set optimization
"""
        if self.args.instruction_set == 'avx2':
            args_content += 'use_avx2 = true\n'
        elif self.args.instruction_set == 'avx512':
            args_content += 'use_avx512 = true\n'
        
        # Write args.gn
        args_path = self.out_dir / "args.gn"
        args_path.parent.mkdir(parents=True, exist_ok=True)
        args_path.write_text(args_content)
        
        return args_path
    
    def sync_source(self):
        """Sync Chromium source"""
        if self.args.skip_sync:
            return
            
        print("Syncing Chromium source...")
        env = self.setup_environment()
        
        # Run trunk.sh equivalent
        subprocess.run([
            str(self.src_dir / "scripts" / "trunk.sh")
        ], env=env, check=True)
    
    def apply_patches(self):
        """Apply Tungsten patches"""
        print("Applying Tungsten patches...")
        env = self.setup_environment()
        
        # Run setup.sh
        subprocess.run([
            str(self.src_dir / "scripts" / "setup.sh")
        ], env=env, check=True)
    
    def configure_build(self):
        """Run GN configuration"""
        print(f"Configuring build for {self.arch}...")
        env = self.setup_environment()
        
        # Generate args.gn
        self.generate_args_gn()
        
        # Run gn gen
        subprocess.run([
            "gn", "gen", str(self.out_dir)
        ], env=env, cwd=self.src_dir, check=True)
    
    def build(self):
        """Execute the build"""
        print(f"Building with {self.jobs} jobs...")
        env = self.setup_environment()
        
        # Build targets
        targets = [
            "chrome",
            "chrome_sandbox",
            "chromedriver",
            "thorium_shell"
        ]
        
        subprocess.run([
            "autoninja", "-C", str(self.out_dir),
            f"-j{self.jobs}"
        ] + targets, env=env, check=True)
    
    def package(self):
        """Create distribution packages"""
        if self.args.skip_package:
            return
            
        print("Creating packages...")
        env = self.setup_environment()
        
        # Run packaging script
        subprocess.run([
            str(self.src_dir / "scripts" / "package.sh"),
            "--build-dir", str(self.out_dir),
            "--arch", self.arch
        ], env=env, check=True)
    
    def run_tests(self):
        """Run test suite"""
        if self.args.skip_tests:
            return
            
        print("Running tests...")
        env = self.setup_environment()
        
        # Unit tests
        subprocess.run([
            str(self.out_dir / "unit_tests"),
            "--gtest_filter=Nostr*:Blossom*"
        ], env=env, check=True)
        
        # Browser tests
        subprocess.run([
            str(self.out_dir / "browser_tests"),
            "--gtest_filter=Nostr*:Nsite*",
            "--use-xvfb"
        ], env=env, check=True)
    
    def build_all(self):
        """Execute complete build pipeline"""
        self.sync_source()
        self.apply_patches()
        self.configure_build()
        self.build()
        self.run_tests()
        self.package()

def main():
    parser = argparse.ArgumentParser(description='Tungsten Linux Build Script')
    parser.add_argument('--arch', choices=['x64', 'arm64'],
                       help='Target architecture (auto-detected if not specified)')
    parser.add_argument('--build-type', default='Release',
                       choices=['Release', 'Debug'],
                       help='Build type')
    parser.add_argument('--debug', action='store_true',
                       help='Build debug version')
    parser.add_argument('--component-build', action='store_true',
                       help='Use component build for faster linking')
    parser.add_argument('--jobs', '-j', type=int,
                       help='Number of parallel jobs')
    parser.add_argument('--instruction-set', 
                       choices=['sse2', 'avx2', 'avx512'],
                       default='sse2',
                       help='CPU instruction set optimization')
    parser.add_argument('--skip-sync', action='store_true',
                       help='Skip source sync')
    parser.add_argument('--skip-tests', action='store_true',
                       help='Skip test execution')
    parser.add_argument('--skip-package', action='store_true',
                       help='Skip package creation')
    
    args = parser.parse_args()
    
    if args.debug:
        args.build_type = 'Debug'
    
    builder = LinuxBuilder(args)
    
    try:
        builder.build_all()
        print(f"Build completed successfully!")
        print(f"Output directory: {builder.out_dir}")
    except subprocess.CalledProcessError as e:
        print(f"Build failed: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()
```

### 5. CI/CD Integration

#### 5.1 GitHub Actions Workflow
```yaml
# .github/workflows/build-linux.yml
name: Linux Build

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
        default: 'x64'
        type: choice
        options:
          - x64
          - arm64
      instruction_set:
        description: 'Instruction Set'
        required: true
        default: 'sse2'
        type: choice
        options:
          - sse2
          - avx2
          - avx512

jobs:
  build:
    runs-on: ${{ matrix.runner }}
    strategy:
      matrix:
        include:
          - arch: x64
            runner: ubuntu-22.04
            instruction_set: [sse2, avx2]
          - arch: arm64
            runner: [self-hosted, linux, arm64]
            instruction_set: [base]
    
    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          fetch-depth: 0
      
      - name: Setup Build Cache
        uses: actions/cache@v3
        with:
          path: |
            ~/.ccache
            out/
          key: linux-${{ matrix.arch }}-${{ github.sha }}
          restore-keys: |
            linux-${{ matrix.arch }}-
      
      - name: Install Dependencies
        run: |
          ./scripts/install-build-deps-linux.sh
          echo "$HOME/depot_tools" >> $GITHUB_PATH
      
      - name: Configure ccache
        run: |
          sudo apt-get install -y ccache
          ccache --max-size=10G
          ccache --set-config=compression=true
      
      - name: Build
        env:
          CCACHE_DIR: ~/.ccache
        run: |
          python3 build.py \
            --arch=${{ matrix.arch }} \
            --instruction-set=${{ matrix.instruction_set }} \
            --jobs=4
      
      - name: Upload Artifacts
        uses: actions/upload-artifact@v3
        with:
          name: tungsten-linux-${{ matrix.arch }}-${{ matrix.instruction_set }}
          path: |
            out/Release/*.deb
            out/Release/*.rpm
            out/Release/*.tar.xz
```

### 6. Package Generation

#### 6.1 DEB Package Structure
```bash
# scripts/create_deb.sh
#!/bin/bash

set -e

BUILD_DIR="$1"
ARCH="$2"
VERSION=$(cat $BUILD_DIR/../../chrome/VERSION | tr '\n' '.' | sed 's/\.$//')

# Package structure
DEB_ROOT="tungsten-browser_${VERSION}_${ARCH}"
mkdir -p $DEB_ROOT/{DEBIAN,opt/tungsten,usr/share/{applications,pixmaps}}

# Control file
cat > $DEB_ROOT/DEBIAN/control << EOF
Package: tungsten-browser
Version: $VERSION
Architecture: $ARCH
Maintainer: Tungsten Team <tungsten@example.com>
Depends: libasound2, libatk-bridge2.0-0, libgtk-3-0, libnss3, libxss1
Section: web
Priority: optional
Description: Tungsten Browser - Nostr-native web browser
 Tungsten is a privacy-focused browser with native Nostr protocol support,
 including built-in NIP-07 interface, local relay, and Blossom server.
EOF

# Copy binaries
cp -r $BUILD_DIR/tungsten $DEB_ROOT/opt/
cp $BUILD_DIR/chrome $DEB_ROOT/opt/tungsten/tungsten-browser
cp $BUILD_DIR/chrome_sandbox $DEB_ROOT/opt/tungsten/
chmod 4755 $DEB_ROOT/opt/tungsten/chrome_sandbox

# Desktop entry
cat > $DEB_ROOT/usr/share/applications/tungsten-browser.desktop << EOF
[Desktop Entry]
Name=Tungsten Browser
Comment=Nostr-native web browser
Exec=/opt/tungsten/tungsten-browser %U
Terminal=false
Type=Application
Icon=tungsten-browser
Categories=Network;WebBrowser;
MimeType=text/html;text/xml;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;x-scheme-handler/nostr;
EOF

# Build package
dpkg-deb --build $DEB_ROOT
rm -rf $DEB_ROOT
```

#### 6.2 RPM Package Spec
```spec
# tungsten-browser.spec
Name:           tungsten-browser
Version:        %{VERSION}
Release:        1%{?dist}
Summary:        Nostr-native web browser

License:        BSD
URL:            https://github.com/tungsten-browser/tungsten
Source0:        tungsten-browser-%{version}.tar.xz

BuildRequires:  gcc-c++
BuildRequires:  gtk3-devel
BuildRequires:  nss-devel
Requires:       gtk3
Requires:       nss

%description
Tungsten is a privacy-focused browser with native Nostr protocol support,
including built-in NIP-07 interface, local relay, and Blossom server.

%prep
%setup -q

%build
# Build handled by main build script

%install
mkdir -p %{buildroot}/opt/tungsten
cp -r * %{buildroot}/opt/tungsten/

mkdir -p %{buildroot}/usr/share/applications
cat > %{buildroot}/usr/share/applications/tungsten-browser.desktop << EOF
[Desktop Entry]
Name=Tungsten Browser
Comment=Nostr-native web browser
Exec=/opt/tungsten/tungsten-browser %U
Terminal=false
Type=Application
Icon=tungsten-browser
Categories=Network;WebBrowser;
EOF

%files
/opt/tungsten
/usr/share/applications/tungsten-browser.desktop

%changelog
* Date Author <email> - version
- Initial package
```

### 7. Build Optimization

#### 7.1 Distributed Build Configuration
```python
# scripts/configure_distributed_build.py
#!/usr/bin/env python3

import os
import json

def configure_goma():
    """Configure Goma distributed build"""
    goma_config = {
        "compiler_proxy_port": 8088,
        "compiler_proxy_threads": 16,
        "cache_dir": os.path.expanduser("~/.goma_cache"),
        "max_cache_size_mb": 50000,
    }
    
    with open(os.path.expanduser("~/.goma_config"), 'w') as f:
        json.dump(goma_config, f)
    
    # Add to args.gn
    return """
use_goma = true
goma_dir = "{}"
""".format(os.path.expanduser("~/goma"))

def configure_sccache():
    """Configure sccache for distributed caching"""
    return """
cc_wrapper = "sccache"
"""

def configure_icecc():
    """Configure Icecream distributed compilation"""
    return """
linux_use_bundled_binutils = false
use_icecream = true
"""
```

### 8. Performance Profiling

```bash
#!/bin/bash
# scripts/profile_build.sh

# Enable build timing
export NINJA_STATUS="[%f/%t %r %es] "

# Profile memory usage
/usr/bin/time -v ninja -C out/Release chrome 2>&1 | tee build_profile.log

# Analyze hot paths
perf record -g ninja -C out/Release chrome
perf report

# Generate build graph
gn desc out/Release //chrome --tree
```

### 9. Container Build Support

```dockerfile
# Dockerfile.linux-build
FROM ubuntu:22.04

# Install base dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    python3 \
    python3-pip \
    curl \
    lsb-release \
    sudo

# Create build user
RUN useradd -m builder && \
    echo "builder ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

USER builder
WORKDIR /home/builder

# Install depot_tools
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

ENV PATH="/home/builder/depot_tools:${PATH}"

# Pre-install build dependencies
COPY scripts/install-build-deps-linux.sh /tmp/
RUN /tmp/install-build-deps-linux.sh

# Build script entrypoint
COPY build.py /home/builder/
ENTRYPOINT ["python3", "/home/builder/build.py"]
```

### 10. Troubleshooting Guide

#### 10.1 Common Build Issues

| Issue | Solution |
|-------|----------|
| Out of memory | Reduce job count with `-j` flag |
| Missing dependencies | Run `install-build-deps-linux.sh` |
| GN errors | Delete out/ directory and reconfigure |
| Linking failures | Use component build for development |
| ccache misses | Clear cache with `ccache -C` |

#### 10.2 Debug Commands
```bash
# Verbose build output
autoninja -C out/Debug -v chrome

# Check build configuration
gn desc out/Release //chrome:chrome

# Analyze build dependencies
gn path out/Release //chrome //content

# List build targets
gn ls out/Release

# Check for missing dependencies
ldd out/Release/chrome | grep "not found"
```

This comprehensive Linux build implementation provides a robust foundation for both local development and CI/CD automation while maintaining compatibility with existing Chromium build infrastructure.