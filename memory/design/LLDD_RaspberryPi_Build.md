# Low Level Design Document: Raspberry Pi Build Process
## Tungsten Browser - ARM64 Embedded Platform Build Implementation

### 1. Component Overview

The Raspberry Pi build process provides optimized compilation for ARM64 (aarch64) architecture, specifically targeting Raspberry Pi 3B+, 4, and 5 models. This implementation supports both native compilation on Pi hardware and cross-compilation from x64 Linux hosts, producing lightweight packages optimized for embedded systems with limited resources.

### 2. Build Environment Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                 Raspberry Pi Build Environment                   │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              Cross-Compilation Toolchain                  │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ GCC ARM64    │  │ Sysroot       │  │ depot_tools│   │   │
│  │  │ Cross Tools  │  │ (Pi OS libs)  │  │            │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ QEMU ARM64   │  │ Python 3.8+   │  │ Ninja Build│   │   │
│  │  │ (optional)   │  │               │  │            │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      Build Scripts Layer                         │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                Raspberry Pi Build Scripts                 │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ build.py     │  │ setup_pi.sh   │  │ sysroot_   │   │   │
│  │  │ (unified)    │  │               │  │ creator.sh │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ cross_compile│  │ native_build  │  │ optimize_  │   │   │
│  │  │ .py          │  │ .sh           │  │ pi.sh      │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                    Target Optimization Layer                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                  Pi-Specific Optimizations                │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ GPU Memory   │  │ NEON SIMD     │  │ Thermal    │   │   │
│  │  │ Split Config │  │ Optimization  │  │ Management │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      Packaging Layer                             │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   Package Formats                         │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ DEB Package  │  │ Snap Package  │  │ AppImage   │   │   │
│  │  │ (ARM64)      │  │ (optional)    │  │ (portable) │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 3. Build Prerequisites

#### 3.1 System Requirements
```bash
# For native compilation on Raspberry Pi:
- Model: Raspberry Pi 3B+, 4, or 5 (64-bit OS required)
- RAM: 4GB minimum (8GB recommended)
- Storage: 32GB SD card minimum (64GB+ recommended)
- OS: Raspberry Pi OS 64-bit (Debian 11+ based)
- Swap: 4GB swap file recommended

# For cross-compilation on x64 Linux:
- CPU: 8+ cores recommended
- RAM: 16GB minimum
- Disk: 100GB free space
- OS: Ubuntu 20.04+ or Debian 11+

# Target Raspberry Pi specifications:
- Architecture: ARM64/aarch64
- Minimum OS: Raspberry Pi OS 64-bit (Bullseye)
- GPU Memory Split: 128MB recommended
```

#### 3.2 Cross-Compilation Setup Script
```bash
#!/bin/bash
# scripts/install-build-deps-raspi.sh

set -e

# Detect if running on Raspberry Pi or cross-compile host
if [[ $(uname -m) == "aarch64" ]] && [[ -f /proc/device-tree/model ]]; then
    IS_RASPBERRY_PI=true
    echo "Detected Raspberry Pi hardware"
else
    IS_RASPBERRY_PI=false
    echo "Setting up cross-compilation environment"
fi

if [ "$IS_RASPBERRY_PI" = true ]; then
    # Native compilation setup
    echo "Setting up native Raspberry Pi build environment..."
    
    # Increase swap for compilation
    if [ ! -f /swapfile ]; then
        sudo fallocate -l 4G /swapfile
        sudo chmod 600 /swapfile
        sudo mkswap /swapfile
        sudo swapon /swapfile
        echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab
    fi
    
    # Install build dependencies
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        git \
        python3 \
        python3-pip \
        ninja-build \
        pkg-config \
        libgtk-3-dev \
        libnss3-dev \
        libxss-dev \
        libasound2-dev \
        libxtst-dev \
        libatspi2.0-dev \
        libdrm-dev \
        libgbm-dev
    
    # Optimize for compilation
    echo "Optimizing system for compilation..."
    sudo systemctl stop bluetooth
    sudo systemctl stop avahi-daemon
    
else
    # Cross-compilation setup
    echo "Setting up cross-compilation for Raspberry Pi..."
    
    # Install cross-compilation toolchain
    sudo apt-get update
    sudo apt-get install -y \
        gcc-aarch64-linux-gnu \
        g++-aarch64-linux-gnu \
        binutils-aarch64-linux-gnu \
        qemu-user-static \
        debootstrap \
        schroot
    
    # Create sysroot
    SYSROOT_DIR="$HOME/raspi-sysroot"
    if [ ! -d "$SYSROOT_DIR" ]; then
        echo "Creating Raspberry Pi sysroot..."
        sudo debootstrap \
            --arch=arm64 \
            --foreign \
            --include=libgtk-3-dev,libnss3-dev,libxss-dev,libasound2-dev \
            bullseye \
            "$SYSROOT_DIR" \
            http://deb.debian.org/debian/
        
        # Copy qemu for chroot
        sudo cp /usr/bin/qemu-aarch64-static "$SYSROOT_DIR/usr/bin/"
        
        # Complete debootstrap
        sudo chroot "$SYSROOT_DIR" /debootstrap/debootstrap --second-stage
        
        # Fix symbolic links for cross-compilation
        sudo find "$SYSROOT_DIR" -type l -lname '/*' -exec sh -c '
            link=$(readlink "$0")
            rm "$0"
            ln -s "'$SYSROOT_DIR'$link" "$0"
        ' {} \;
    fi
fi

# Install depot_tools
if [ ! -d "$HOME/depot_tools" ]; then
    echo "Installing depot_tools..."
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git $HOME/depot_tools
fi

export PATH="$HOME/depot_tools:$PATH"
echo 'export PATH="$HOME/depot_tools:$PATH"' >> ~/.bashrc

# Python dependencies
pip3 install --user httplib2 six

echo "Raspberry Pi build environment setup complete!"
```

### 4. Unified Build Script Implementation

```python
#!/usr/bin/env python3
# build.py - Raspberry Pi platform support

import argparse
import os
import subprocess
import sys
import platform
from pathlib import Path

class RaspberryPiBuilder:
    def __init__(self, args):
        self.args = args
        self.src_dir = Path(__file__).parent.parent
        self.out_dir = self.src_dir / "out" / "raspi"
        self.is_native = platform.machine() == 'aarch64'
        self.sysroot = None if self.is_native else Path.home() / "raspi-sysroot"
        self.pi_model = self.detect_pi_model()
        
    def detect_pi_model(self):
        """Detect Raspberry Pi model"""
        if not self.is_native:
            return self.args.pi_model or 'pi4'
        
        try:
            with open('/proc/device-tree/model', 'r') as f:
                model = f.read().strip()
                if 'Pi 5' in model:
                    return 'pi5'
                elif 'Pi 4' in model:
                    return 'pi4'
                elif 'Pi 3' in model:
                    return 'pi3'
        except:
            pass
        
        return 'pi4'  # Default
    
    def setup_environment(self):
        """Configure Raspberry Pi build environment"""
        env = os.environ.copy()
        
        # Add depot_tools to PATH
        depot_tools = os.path.expanduser("~/depot_tools")
        env['PATH'] = f"{depot_tools}:{env['PATH']}"
        
        if not self.is_native:
            # Cross-compilation environment
            env['CC'] = 'aarch64-linux-gnu-gcc'
            env['CXX'] = 'aarch64-linux-gnu-g++'
            env['AR'] = 'aarch64-linux-gnu-ar'
            env['STRIP'] = 'aarch64-linux-gnu-strip'
            env['PKG_CONFIG_PATH'] = f"{self.sysroot}/usr/lib/aarch64-linux-gnu/pkgconfig"
            env['PKG_CONFIG_SYSROOT_DIR'] = str(self.sysroot)
        
        # Tungsten-specific flags
        env['TUNGSTEN_ENABLE_NOSTR'] = '1'
        env['TUNGSTEN_ENABLE_LOCAL_RELAY'] = '1'
        env['TUNGSTEN_ENABLE_BLOSSOM'] = '1'
        
        return env
    
    def get_pi_optimization_flags(self):
        """Get Raspberry Pi specific optimization flags"""
        flags = {
            'pi3': {
                'arm_version': 8,
                'arm_arch': 'armv8-a',
                'arm_cpu': 'cortex-a53',
                'arm_fpu': 'crypto-neon-fp-armv8',
                'arm_float_abi': 'hard',
                'arm_use_neon': True,
            },
            'pi4': {
                'arm_version': 8,
                'arm_arch': 'armv8-a',
                'arm_cpu': 'cortex-a72',
                'arm_fpu': 'crypto-neon-fp-armv8',
                'arm_float_abi': 'hard',
                'arm_use_neon': True,
            },
            'pi5': {
                'arm_version': 8,
                'arm_arch': 'armv8.2-a',
                'arm_cpu': 'cortex-a76',
                'arm_fpu': 'crypto-neon-fp-armv8',
                'arm_float_abi': 'hard',
                'arm_use_neon': True,
            }
        }
        
        return flags.get(self.pi_model, flags['pi4'])
    
    def generate_args_gn(self):
        """Generate args.gn for Raspberry Pi build"""
        pi_flags = self.get_pi_optimization_flags()
        
        args_content = f"""
# Generated Raspberry Pi build configuration
target_os = "linux"
target_cpu = "arm64"
is_official_build = {str(not self.args.debug).lower()}
is_debug = {str(self.args.debug).lower()}
is_component_build = false

# Cross-compilation settings
"""
        
        if not self.is_native:
            args_content += f"""
use_sysroot = true
sysroot = "{self.sysroot}"
custom_toolchain = "//build/toolchain/linux:clang_arm64"
"""
        
        args_content += f"""
# ARM optimization flags
arm_version = {pi_flags['arm_version']}
arm_arch = "{pi_flags['arm_arch']}"
arm_cpu = "{pi_flags['arm_cpu']}"
arm_fpu = "{pi_flags['arm_fpu']}"
arm_float_abi = "{pi_flags['arm_float_abi']}"
arm_use_neon = {str(pi_flags['arm_use_neon']).lower()}

# Raspberry Pi optimizations
enable_nacl = false
use_thin_lto = false  # Reduce memory usage
symbol_level = 0
enable_resource_allowlist_generation = true
v8_enable_lite_mode = true  # Lighter V8 for embedded

# Media optimizations for Pi
use_v4l2_codec = true
use_linux_v4l2 = true
proprietary_codecs = true
ffmpeg_branding = "Chrome"

# Reduce binary size
exclude_unwind_tables = true
enable_widevine = false

# Tungsten features
enable_nostr = true
enable_local_relay = true
enable_blossom_server = true
enable_nsite_support = true

# GPU settings for Pi
use_ozone = true
ozone_auto_platforms = false
ozone_platform = "headless"
ozone_platform_headless = true
ozone_platform_wayland = true
ozone_platform_x11 = true
use_system_minigbm = false
use_system_libdrm = true

# Feature flags for embedded
enable_basic_printing = false
enable_print_preview = false
enable_google_now = false
enable_one_click_signin = false
"""
        
        # Memory constraints for different Pi models
        if self.pi_model == 'pi3':
            args_content += """
# Pi 3 memory constraints
use_jumbo_build = false
max_jobs_per_link = 1
concurrent_links = 1
"""
        elif self.pi_model == 'pi4':
            args_content += """
# Pi 4 optimizations
use_jumbo_build = true
jumbo_file_merge_limit = 50
"""
        
        # Write args.gn
        args_path = self.out_dir / "args.gn"
        args_path.parent.mkdir(parents=True, exist_ok=True)
        args_path.write_text(args_content)
        
        return args_path
    
    def optimize_for_pi(self):
        """Apply Raspberry Pi specific optimizations"""
        if not self.is_native:
            return
        
        print("Applying Raspberry Pi optimizations...")
        
        # Set GPU memory split
        subprocess.run([
            "sudo", "raspi-config", "nonint", "do_memory_split", "128"
        ], check=False)
        
        # Disable unnecessary services during build
        services = ['bluetooth', 'avahi-daemon', 'triggerhappy']
        for service in services:
            subprocess.run([
                "sudo", "systemctl", "stop", service
            ], check=False)
        
        # Set CPU governor to performance
        subprocess.run([
            "sudo", "sh", "-c",
            "echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
        ], check=False)
    
    def build(self):
        """Execute Raspberry Pi build"""
        print(f"Building for Raspberry Pi ({self.pi_model})...")
        print(f"Build mode: {'Native' if self.is_native else 'Cross-compilation'}")
        
        env = self.setup_environment()
        
        # Apply optimizations if native
        if self.is_native:
            self.optimize_for_pi()
        
        # Configure build
        self.generate_args_gn()
        
        # Run gn gen
        subprocess.run([
            "gn", "gen", str(self.out_dir)
        ], env=env, cwd=self.src_dir, check=True)
        
        # Determine job count based on available memory
        jobs = self.calculate_job_count()
        
        # Build targets
        targets = [
            "chrome",
            "chrome_sandbox",
            "chromedriver"
        ]
        
        # Use autoninja for build
        subprocess.run([
            "autoninja", "-C", str(self.out_dir),
            f"-j{jobs}"
        ] + targets, env=env, check=True)
    
    def calculate_job_count(self):
        """Calculate optimal job count for Pi"""
        if not self.is_native:
            return os.cpu_count() or 4
        
        # Get available memory
        try:
            with open('/proc/meminfo', 'r') as f:
                for line in f:
                    if line.startswith('MemTotal:'):
                        mem_kb = int(line.split()[1])
                        mem_gb = mem_kb / (1024 * 1024)
                        
                        # Rough estimate: 1 job per 2GB RAM
                        return max(1, int(mem_gb / 2))
        except:
            pass
        
        return 2  # Conservative default
    
    def create_lightweight_package(self):
        """Create optimized package for Pi"""
        if self.args.skip_package:
            return
        
        print("Creating Raspberry Pi package...")
        
        # Strip binaries aggressively
        strip_cmd = 'aarch64-linux-gnu-strip' if not self.is_native else 'strip'
        
        binaries = ['chrome', 'chrome_sandbox', 'chromedriver']
        for binary in binaries:
            binary_path = self.out_dir / binary
            if binary_path.exists():
                subprocess.run([
                    strip_cmd, '--strip-all', str(binary_path)
                ], check=True)
        
        # Create DEB package
        self.create_deb_package()
        
        # Create AppImage for portability
        if self.args.create_appimage:
            self.create_appimage()
    
    def create_deb_package(self):
        """Create Debian package for Raspberry Pi"""
        version = self.get_version()
        arch = 'arm64'
        
        deb_name = f"tungsten-browser-raspi_{version}_{arch}"
        deb_dir = self.out_dir / deb_name
        
        # Create package structure
        deb_dir.mkdir(exist_ok=True)
        (deb_dir / "DEBIAN").mkdir(exist_ok=True)
        (deb_dir / "opt" / "tungsten").mkdir(parents=True, exist_ok=True)
        (deb_dir / "usr" / "share" / "applications").mkdir(parents=True, exist_ok=True)
        
        # Control file with Pi-specific dependencies
        control_content = f"""Package: tungsten-browser-raspi
Version: {version}
Architecture: {arch}
Maintainer: Tungsten Team <tungsten@example.com>
Depends: libc6, libgtk-3-0, libnss3, libxss1, libgles2
Recommends: chromium-codecs-ffmpeg-extra
Conflicts: tungsten-browser
Section: web
Priority: optional
Description: Tungsten Browser for Raspberry Pi
 Lightweight Nostr-native browser optimized for Raspberry Pi
 with hardware acceleration support.
"""
        
        (deb_dir / "DEBIAN" / "control").write_text(control_content)
        
        # Copy binaries
        import shutil
        for file in ['chrome', 'chrome_sandbox', 'chromedriver']:
            src = self.out_dir / file
            if src.exists():
                dst = deb_dir / "opt" / "tungsten" / file
                shutil.copy2(src, dst)
                if file == 'chrome':
                    dst.rename(dst.parent / 'tungsten-browser')
        
        # Desktop entry
        desktop_content = """[Desktop Entry]
Name=Tungsten Browser
Comment=Nostr-native web browser for Raspberry Pi
Exec=/opt/tungsten/tungsten-browser %U
Terminal=false
Type=Application
Icon=tungsten-browser
Categories=Network;WebBrowser;
MimeType=text/html;text/xml;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;x-scheme-handler/nostr;
StartupNotify=true
"""
        
        (deb_dir / "usr" / "share" / "applications" / "tungsten-browser.desktop").write_text(desktop_content)
        
        # Build package
        subprocess.run([
            "dpkg-deb", "--build", str(deb_dir)
        ], check=True)
        
        print(f"Created: {deb_name}.deb")
    
    def create_appimage(self):
        """Create AppImage for portable execution"""
        print("Creating AppImage...")
        
        # Download AppImage tools
        appimagetool = self.out_dir / "appimagetool"
        if not appimagetool.exists():
            subprocess.run([
                "wget", "-O", str(appimagetool),
                "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-aarch64.AppImage"
            ], check=True)
            appimagetool.chmod(0o755)
        
        # Create AppDir structure
        appdir = self.out_dir / "Tungsten.AppDir"
        appdir.mkdir(exist_ok=True)
        
        # Copy files and create structure
        # ... (AppImage creation logic)
        
        # Build AppImage
        subprocess.run([
            str(appimagetool), str(appdir),
            str(self.out_dir / "Tungsten-aarch64.AppImage")
        ], check=True)
    
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
# .github/workflows/build-raspi.yml
name: Raspberry Pi Build

on:
  push:
    branches: [main, dev]
  pull_request:
    branches: [main]
  workflow_dispatch:
    inputs:
      pi_model:
        description: 'Target Pi Model'
        required: true
        default: 'pi4'
        type: choice
        options:
          - pi3
          - pi4
          - pi5

jobs:
  build:
    runs-on: ubuntu-latest
    container:
      image: debian:bullseye
      options: --privileged
    
    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          fetch-depth: 0
      
      - name: Setup Cross-Compilation
        run: |
          apt-get update
          apt-get install -y sudo git python3 python3-pip
          ./scripts/install-build-deps-raspi.sh
          echo "$HOME/depot_tools" >> $GITHUB_PATH
      
      - name: Setup Build Cache
        uses: actions/cache@v3
        with:
          path: |
            ~/.ccache
            out/
            ~/raspi-sysroot
          key: raspi-${{ github.sha }}
          restore-keys: |
            raspi-
      
      - name: Configure ccache
        run: |
          apt-get install -y ccache
          ccache --max-size=10G
          ccache --zero-stats
      
      - name: Build
        env:
          CCACHE_DIR: ~/.ccache
        run: |
          python3 build.py \
            --platform=raspi \
            --pi-model=${{ github.event.inputs.pi_model || 'pi4' }} \
            --jobs=4
      
      - name: Create Packages
        run: |
          python3 build.py --package-only --platform=raspi
      
      - name: Upload Artifacts
        uses: actions/upload-artifact@v3
        with:
          name: tungsten-raspi-${{ github.event.inputs.pi_model || 'pi4' }}
          path: |
            out/raspi/*.deb
            out/raspi/*.AppImage
```

### 6. Performance Optimization

#### 6.1 Memory-Constrained Build
```bash
#!/bin/bash
# scripts/build_raspi_low_memory.sh

# For building on Pi with limited RAM

set -e

# Enable zram compression
sudo apt-get install -y zram-tools
sudo systemctl enable zram-swap
sudo systemctl start zram-swap

# Configure build for low memory
export GN_ARGS="
is_official_build=true
is_debug=false
symbol_level=0
enable_nacl=false
use_jumbo_build=false
use_thin_lto=false
concurrent_links=1
"

# Build with minimal parallelism
ninja -C out/raspi -j1 chrome

# Build other targets sequentially
ninja -C out/raspi -j1 chrome_sandbox
ninja -C out/raspi -j1 chromedriver
```

### 7. Hardware Acceleration

#### 7.1 GPU Configuration
```cpp
// gpu/config/gpu_info_collector_linux.cc
// Raspberry Pi GPU detection and configuration

namespace gpu {

bool CollectRaspberryPiGpuInfo(GPUInfo* gpu_info) {
  // Detect VideoCore GPU
  base::FilePath vc_path("/opt/vc/lib/libbrcmGLESv2.so");
  if (base::PathExists(vc_path)) {
    gpu_info->gpu.vendor_id = 0x14e4;  // Broadcom
    gpu_info->gpu.device_id = 0x0001;  // VideoCore
    
    // Check for V3D driver (Pi 4+)
    base::FilePath v3d_path("/dev/dri/card0");
    if (base::PathExists(v3d_path)) {
      gpu_info->gpu.driver_vendor = "Mesa";
      gpu_info->gpu.driver_version = GetMesaVersion();
      gpu_info->sandboxed = false;
    }
    
    return true;
  }
  
  return false;
}

}  // namespace gpu
```

### 8. Testing on Raspberry Pi

#### 8.1 Hardware Test Script
```bash
#!/bin/bash
# scripts/test_on_pi.sh

set -e

# Function to check Pi temperature
check_temp() {
    temp=$(vcgencmd measure_temp | sed 's/temp=//' | sed 's/\..*//g')
    if [ $temp -gt 80 ]; then
        echo "WARNING: High temperature detected: ${temp}C"
        echo "Pausing for cooldown..."
        sleep 30
    fi
}

# Run tests with temperature monitoring
echo "Starting Raspberry Pi browser tests..."

# Basic functionality test
timeout 300 ./out/raspi/chrome \
    --no-sandbox \
    --disable-gpu-sandbox \
    --headless \
    --dump-dom \
    https://example.com

check_temp

# Nostr functionality test
echo "Testing Nostr features..."
./out/raspi/unit_tests --gtest_filter="Nostr*" || true

check_temp

# Performance benchmark
echo "Running performance benchmark..."
./out/raspi/chrome \
    --no-sandbox \
    --enable-benchmarking \
    --disable-gpu-vsync \
    http://localhost:8080/benchmark.html

echo "All tests completed!"
```

### 9. Troubleshooting Guide

#### 9.1 Common Raspberry Pi Build Issues

| Issue | Solution |
|-------|----------|
| Out of memory during build | Enable swap, reduce job count |
| Overheating | Add cooling, build in stages |
| Cross-compile linking errors | Update sysroot, check library versions |
| GPU acceleration not working | Install Mesa drivers, check device permissions |
| Slow performance | Enable GPU memory split, disable unnecessary features |

#### 9.2 Debug Commands
```bash
# Check Pi model and specs
cat /proc/device-tree/model
cat /proc/cpuinfo | grep Model
vcgencmd get_mem arm && vcgencmd get_mem gpu

# Monitor build resources
htop
vcgencmd measure_temp
free -h

# Check cross-compilation setup
aarch64-linux-gnu-gcc --version
file out/raspi/chrome

# Test GPU acceleration
glxinfo | grep "OpenGL renderer"
es2_info | grep GL_RENDERER

# Debug crashes
gdb out/raspi/chrome
(gdb) run --no-sandbox --disable-gpu-sandbox

# Check library dependencies
ldd out/raspi/chrome | grep "not found"
```

This comprehensive Raspberry Pi build implementation provides optimized compilation for ARM64 embedded systems with careful consideration for memory constraints, thermal management, and hardware acceleration support.