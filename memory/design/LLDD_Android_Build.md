# Low Level Design Document: Android Build Process
## Tungsten Browser - Android Platform Build Implementation

### 1. Component Overview

The Android build process provides compilation for ARM32, ARM64, x86, and x64 architectures, producing APK and AAB (Android App Bundle) packages. This implementation leverages Android SDK/NDK toolchains while supporting both Chromium WebView integration and standalone browser applications for Android devices.

### 2. Build Environment Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                   Android Build Environment                      │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                  Build Dependencies                       │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ Android SDK  │  │ Android NDK   │  │ depot_tools│   │   │
│  │  │ (API 30+)    │  │ (r25+)        │  │            │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ Java 11+     │  │ Python 3.8+   │  │ Build Tools│   │   │
│  │  │              │  │               │  │ 33.0+      │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      Build Scripts Layer                         │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                 Android Build Scripts                     │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ build.py     │  │ setup_android │  │ android_   │   │   │
│  │  │ (unified)    │  │ .sh           │  │ args.gn    │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ apk_builder  │  │ bundle_builder│  │ signing.py │   │   │
│  │  │ .py          │  │ .py           │  │            │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                    Native Build Layer (NDK)                      │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                  Cross-Compilation                        │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ ARM32 Target │  │ ARM64 Target  │  │ x86 Target │   │   │
│  │  │ (armeabi-v7a)│  │ (arm64-v8a)   │  │            │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      Java/Kotlin Layer                           │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                  Android Components                       │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ Activities   │  │ Services      │  │ Content    │   │   │
│  │  │              │  │               │  │ Providers  │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────────┐
│                    Packaging & Distribution                      │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                  Package Formats                          │   │
│  │  ┌──────────────┐  ┌───────────────┐  ┌────────────┐   │   │
│  │  │ APK Builder  │  │ AAB Builder   │  │ Play Store │   │   │
│  │  │              │  │               │  │ Upload     │   │   │
│  │  └──────────────┘  └───────────────┘  └────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 3. Build Prerequisites

#### 3.1 System Requirements
```bash
# Minimum system specifications
- OS: Linux (Ubuntu 20.04+) or macOS
- CPU: 8+ cores recommended
- RAM: 16GB minimum (32GB+ recommended)
- Disk: 200GB free space
- Java: OpenJDK 11 or 17

# Target Android versions
- Minimum SDK: API 21 (Android 5.0)
- Target SDK: API 33 (Android 13)
- Compile SDK: API 33

# Supported architectures
- armeabi-v7a (32-bit ARM)
- arm64-v8a (64-bit ARM)
- x86 (32-bit Intel, emulator)
- x86_64 (64-bit Intel, emulator)
```

#### 3.2 Dependency Installation Script
```bash
#!/bin/bash
# scripts/install-build-deps-android.sh

set -e

# Detect OS
OS="unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
else
    echo "Unsupported OS: $OSTYPE"
    exit 1
fi

# Install base dependencies
if [ "$OS" == "linux" ]; then
    sudo apt-get update
    sudo apt-get install -y \
        openjdk-11-jdk \
        python3 \
        python3-pip \
        wget \
        unzip \
        build-essential
elif [ "$OS" == "macos" ]; then
    brew install openjdk@11 python@3.11
    sudo ln -sfn $(brew --prefix)/opt/openjdk@11/libexec/openjdk.jdk \
        /Library/Java/JavaVirtualMachines/openjdk-11.jdk
fi

# Set JAVA_HOME
export JAVA_HOME=$(/usr/libexec/java_home -v 11 2>/dev/null || echo "/usr/lib/jvm/java-11-openjdk-amd64")
echo "export JAVA_HOME=$JAVA_HOME" >> ~/.bashrc

# Install Android command line tools
ANDROID_HOME="$HOME/android-sdk"
mkdir -p "$ANDROID_HOME"

# Download Android command line tools
CMDLINE_TOOLS_URL="https://dl.google.com/android/repository/commandlinetools-${OS}-9477386_latest.zip"
wget -O /tmp/cmdline-tools.zip "$CMDLINE_TOOLS_URL"
unzip -q /tmp/cmdline-tools.zip -d "$ANDROID_HOME"
mkdir -p "$ANDROID_HOME/cmdline-tools/latest"
mv "$ANDROID_HOME/cmdline-tools/"* "$ANDROID_HOME/cmdline-tools/latest/" 2>/dev/null || true

# Set Android environment variables
export ANDROID_HOME="$ANDROID_HOME"
export PATH="$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH"

# Save to shell profile
echo "export ANDROID_HOME=$ANDROID_HOME" >> ~/.bashrc
echo 'export PATH="$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH"' >> ~/.bashrc

# Accept licenses
yes | sdkmanager --licenses

# Install required SDK components
echo "Installing Android SDK components..."
sdkmanager \
    "platform-tools" \
    "platforms;android-33" \
    "build-tools;33.0.2" \
    "ndk;25.2.9519653" \
    "cmake;3.22.1"

# Install additional build dependencies
if [ "$OS" == "linux" ]; then
    # 32-bit libraries for aapt
    sudo apt-get install -y lib32z1 lib32ncurses6 lib32stdc++6
fi

# Install depot_tools if not present
if [ ! -d "$HOME/depot_tools" ]; then
    echo "Installing depot_tools..."
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git $HOME/depot_tools
fi

echo "Android build dependencies installed successfully!"
echo "Please restart your terminal to apply environment changes."
```

### 4. Unified Build Script Implementation

```python
#!/usr/bin/env python3
# build.py - Android platform support

import argparse
import os
import subprocess
import sys
import json
from pathlib import Path

class AndroidBuilder:
    def __init__(self, args):
        self.args = args
        self.src_dir = Path(__file__).parent.parent
        self.out_dir = self.src_dir / "out" / f"android_{args.arch}"
        self.arch = args.arch
        self.android_home = os.environ.get('ANDROID_HOME')
        self.build_config = self.get_build_config()
        
    def get_build_config(self):
        """Get Android build configuration"""
        return {
            'armeabi-v7a': {
                'target_cpu': 'arm',
                'android_app_abi': 'armeabi-v7a',
                'arm_version': 7,
            },
            'arm64-v8a': {
                'target_cpu': 'arm64',
                'android_app_abi': 'arm64-v8a',
            },
            'x86': {
                'target_cpu': 'x86',
                'android_app_abi': 'x86',
            },
            'x86_64': {
                'target_cpu': 'x64',
                'android_app_abi': 'x86_64',
            }
        }[self.arch]
    
    def setup_environment(self):
        """Configure Android build environment"""
        env = os.environ.copy()
        
        # Add depot_tools to PATH
        depot_tools = os.path.expanduser("~/depot_tools")
        env['PATH'] = f"{depot_tools}:{env['PATH']}"
        
        # Android environment
        if not self.android_home:
            self.android_home = os.path.expanduser("~/android-sdk")
        
        env['ANDROID_HOME'] = self.android_home
        env['ANDROID_SDK_ROOT'] = self.android_home
        env['PATH'] = f"{self.android_home}/cmdline-tools/latest/bin:{env['PATH']}"
        env['PATH'] = f"{self.android_home}/platform-tools:{env['PATH']}"
        
        # Java environment
        java_home = env.get('JAVA_HOME')
        if not java_home:
            # Try to detect Java
            if sys.platform == 'darwin':
                result = subprocess.run(
                    ['/usr/libexec/java_home', '-v', '11'],
                    capture_output=True,
                    text=True
                )
                if result.returncode == 0:
                    java_home = result.stdout.strip()
            else:
                java_home = '/usr/lib/jvm/java-11-openjdk-amd64'
        
        env['JAVA_HOME'] = java_home
        
        # Tungsten-specific flags
        env['TUNGSTEN_ENABLE_NOSTR'] = '1'
        env['TUNGSTEN_ENABLE_LOCAL_RELAY'] = '1'
        env['TUNGSTEN_ENABLE_BLOSSOM'] = '1'
        
        return env
    
    def generate_args_gn(self):
        """Generate args.gn for Android build"""
        config = self.build_config
        
        args_content = f"""
# Generated Android build configuration
target_os = "android"
target_cpu = "{config['target_cpu']}"
is_official_build = {str(not self.args.debug).lower()}
is_debug = {str(self.args.debug).lower()}
is_component_build = false

# Android-specific settings
android_channel = "stable"
android_default_version_name = "1.0.0"
android_default_version_code = "1"

# Architecture configuration
android_app_abi = "{config['android_app_abi']}"
"""
        
        if 'arm_version' in config:
            args_content += f'arm_version = {config["arm_version"]}\n'
        
        args_content += f"""
# Optimization
symbol_level = {1 if self.args.debug else 0}
enable_nacl = false
use_debug_fission = false

# Media codecs
proprietary_codecs = true
ffmpeg_branding = "Chrome"
enable_hevc_demuxing = true

# Tungsten features
enable_nostr = true
enable_local_relay = true
enable_blossom_server = true
enable_nsite_support = true

# Android features
use_default_android_studio_project = false
android_ndk_root = "{self.android_home}/ndk/25.2.9519653"
android_sdk_root = "{self.android_home}"
android_sdk_build_tools_version = "33.0.2"
android_sdk_version = 33
android_keystore_path = "{self.src_dir}/build/android/tungsten.keystore"
android_keystore_name = "tungsten"
android_keystore_password = "tungsten"

# WebView support
system_webview_package_name = "com.tungsten.webview"
"""
        
        # Write args.gn
        args_path = self.out_dir / "args.gn"
        args_path.parent.mkdir(parents=True, exist_ok=True)
        args_path.write_text(args_content)
        
        return args_path
    
    def create_keystore(self):
        """Create Android keystore for signing"""
        keystore_path = self.src_dir / "build" / "android" / "tungsten.keystore"
        
        if keystore_path.exists():
            return
        
        print("Creating Android keystore...")
        keystore_path.parent.mkdir(parents=True, exist_ok=True)
        
        subprocess.run([
            "keytool", "-genkey",
            "-v", "-keystore", str(keystore_path),
            "-alias", "tungsten",
            "-keyalg", "RSA",
            "-keysize", "2048",
            "-validity", "10000",
            "-storepass", "tungsten",
            "-keypass", "tungsten",
            "-dname", "CN=Tungsten Browser, OU=Engineering, O=Tungsten, L=City, S=State, C=US"
        ], check=True)
    
    def build(self):
        """Execute Android build"""
        print(f"Building for Android {self.arch}...")
        env = self.setup_environment()
        
        # Create keystore if needed
        self.create_keystore()
        
        # Configure build
        self.generate_args_gn()
        
        # Run gn gen
        subprocess.run([
            "gn", "gen", str(self.out_dir)
        ], env=env, cwd=self.src_dir, check=True)
        
        # Build targets
        targets = []
        
        if self.args.build_webview:
            targets.extend([
                "system_webview_apk",
                "system_webview_shell_apk"
            ])
        else:
            targets.extend([
                "chrome_public_apk",
                "chrome_modern_public_bundle",
                "monochrome_public_apk"
            ])
        
        # Use autoninja for build
        subprocess.run([
            "autoninja", "-C", str(self.out_dir)
        ] + targets, env=env, check=True)
    
    def sign_apk(self, apk_path):
        """Sign APK with release key"""
        if self.args.debug or self.args.skip_sign:
            return
        
        print(f"Signing {apk_path.name}...")
        
        # Use uber-apk-signer for v1 + v2 + v3 signing
        signed_path = apk_path.with_suffix('.signed.apk')
        
        subprocess.run([
            "jarsigner",
            "-verbose",
            "-sigalg", "SHA256withRSA",
            "-digestalg", "SHA256",
            "-keystore", str(self.src_dir / "build" / "android" / "tungsten.keystore"),
            "-storepass", "tungsten",
            str(apk_path),
            "tungsten"
        ], check=True)
        
        # Align APK
        aligned_path = apk_path.with_suffix('.aligned.apk')
        subprocess.run([
            f"{self.android_home}/build-tools/33.0.2/zipalign",
            "-v", "4",
            str(apk_path),
            str(aligned_path)
        ], check=True)
        
        # Move aligned to original
        import shutil
        shutil.move(str(aligned_path), str(apk_path))
    
    def create_bundle(self):
        """Create Android App Bundle (AAB)"""
        if self.args.skip_bundle:
            return
        
        print("Creating Android App Bundle...")
        
        bundle_path = self.out_dir / "tungsten.aab"
        
        # The bundle is already built by chrome_modern_public_bundle target
        source_bundle = self.out_dir / "apks" / "ChromeModernPublic.aab"
        
        if source_bundle.exists():
            import shutil
            shutil.copy2(source_bundle, bundle_path)
            print(f"Bundle created: {bundle_path}")
    
    def package_apks(self):
        """Package all APKs"""
        if self.args.skip_package:
            return
        
        print("Packaging APKs...")
        
        apks_dir = self.out_dir / "apks"
        package_dir = self.out_dir / "package"
        package_dir.mkdir(exist_ok=True)
        
        # Find all APKs
        apk_mapping = {
            "ChromePublic.apk": f"tungsten-{self.arch}.apk",
            "MonochromePublic.apk": f"tungsten-monochrome-{self.arch}.apk",
            "SystemWebView.apk": f"tungsten-webview-{self.arch}.apk",
            "SystemWebViewShell.apk": f"tungsten-webview-shell-{self.arch}.apk"
        }
        
        for src_name, dst_name in apk_mapping.items():
            src_path = apks_dir / src_name
            if src_path.exists():
                dst_path = package_dir / dst_name
                import shutil
                shutil.copy2(src_path, dst_path)
                
                # Sign release APKs
                if not self.args.debug:
                    self.sign_apk(dst_path)
                
                print(f"Packaged: {dst_name}")
    
    def run_tests(self):
        """Run Android tests"""
        if self.args.skip_tests:
            return
        
        print("Running Android tests...")
        env = self.setup_environment()
        
        # Start emulator if needed
        if self.args.use_emulator:
            self.start_emulator()
        
        # Run instrumentation tests
        subprocess.run([
            str(self.out_dir / "bin" / "run_chrome_public_test_apk"),
            "--release",
            f"--test-apk=ChromePublicTest",
            "--gtest_filter=Tungsten*"
        ], env=env, check=True)
        
        # Run unit tests on host
        subprocess.run([
            str(self.out_dir / "bin" / "run_chrome_junit_tests"),
            "--release"
        ], env=env, check=True)
    
    def start_emulator(self):
        """Start Android emulator for testing"""
        print("Starting Android emulator...")
        
        # Create AVD if not exists
        avd_name = "tungsten_test_avd"
        
        # Check if AVD exists
        result = subprocess.run([
            f"{self.android_home}/cmdline-tools/latest/bin/avdmanager",
            "list", "avd"
        ], capture_output=True, text=True)
        
        if avd_name not in result.stdout:
            # Create AVD
            subprocess.run([
                f"{self.android_home}/cmdline-tools/latest/bin/avdmanager",
                "create", "avd",
                "--name", avd_name,
                "--package", "system-images;android-33;google_apis;x86_64",
                "--device", "pixel_5"
            ], input="no\n", text=True, check=True)
        
        # Start emulator
        subprocess.Popen([
            f"{self.android_home}/emulator/emulator",
            "-avd", avd_name,
            "-no-window",
            "-no-boot-anim"
        ])
        
        # Wait for emulator
        subprocess.run([
            f"{self.android_home}/platform-tools/adb",
            "wait-for-device"
        ], check=True)
    
    def build_all(self):
        """Execute complete build pipeline"""
        self.build()
        self.package_apks()
        self.create_bundle()
        self.run_tests()
```

### 5. CI/CD Integration

#### 5.1 GitHub Actions Workflow
```yaml
# .github/workflows/build-android.yml
name: Android Build

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
        default: 'arm64-v8a'
        type: choice
        options:
          - armeabi-v7a
          - arm64-v8a
          - x86
          - x86_64
      build_webview:
        description: 'Build WebView'
        required: false
        default: false
        type: boolean

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        arch: [armeabi-v7a, arm64-v8a, x86_64]
        include:
          - arch: armeabi-v7a
            abi: armeabi-v7a
          - arch: arm64-v8a
            abi: arm64-v8a
          - arch: x86_64
            abi: x86_64
    
    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          fetch-depth: 0
      
      - name: Setup Java
        uses: actions/setup-java@v3
        with:
          distribution: 'temurin'
          java-version: '11'
      
      - name: Setup Android SDK
        uses: android-actions/setup-android@v3
        with:
          api-level: 33
          ndk-version: 25.2.9519653
          cmake-version: 3.22.1
      
      - name: Install Dependencies
        run: |
          ./scripts/install-build-deps-android.sh
          echo "$HOME/depot_tools" >> $GITHUB_PATH
      
      - name: Setup Build Cache
        uses: actions/cache@v3
        with:
          path: |
            ~/.ccache
            out/
            ~/.gradle/caches
          key: android-${{ matrix.arch }}-${{ github.sha }}
          restore-keys: |
            android-${{ matrix.arch }}-
      
      - name: Configure ccache
        run: |
          sudo apt-get install -y ccache
          ccache --max-size=10G
          ccache --zero-stats
      
      - name: Build
        env:
          CCACHE_DIR: ~/.ccache
        run: |
          python3 build.py \
            --platform=android \
            --arch=${{ matrix.arch }} \
            --jobs=4
      
      - name: Sign APKs
        if: github.ref == 'refs/heads/main'
        env:
          ANDROID_KEYSTORE_BASE64: ${{ secrets.ANDROID_KEYSTORE_BASE64 }}
          ANDROID_KEYSTORE_PASSWORD: ${{ secrets.ANDROID_KEYSTORE_PASSWORD }}
        run: |
          echo $ANDROID_KEYSTORE_BASE64 | base64 -d > tungsten-release.keystore
          python3 build.py \
            --sign-only \
            --arch=${{ matrix.arch }} \
            --keystore=tungsten-release.keystore \
            --keystore-password=$ANDROID_KEYSTORE_PASSWORD
      
      - name: Upload APKs
        uses: actions/upload-artifact@v3
        with:
          name: tungsten-android-${{ matrix.arch }}
          path: |
            out/android_${{ matrix.arch }}/package/*.apk
            out/android_${{ matrix.arch }}/*.aab
      
      - name: Upload to Play Store
        if: github.ref == 'refs/heads/main' && matrix.arch == 'arm64-v8a'
        uses: r0adkll/upload-google-play@v1
        with:
          serviceAccountJsonPlainText: ${{ secrets.PLAY_STORE_SERVICE_ACCOUNT }}
          packageName: com.tungsten.browser
          releaseFiles: out/android_${{ matrix.arch }}/tungsten.aab
          track: internal
```

### 6. Android-Specific Features

#### 6.1 Android Manifest Configuration
```xml
<!-- android/AndroidManifest.xml -->
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.tungsten.browser">
    
    <!-- Permissions -->
    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
    <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
    <uses-permission android:name="android.permission.CAMERA" />
    <uses-permission android:name="android.permission.RECORD_AUDIO" />
    
    <!-- Features -->
    <uses-feature android:name="android.hardware.camera" android:required="false" />
    <uses-feature android:name="android.hardware.camera.autofocus" android:required="false" />
    
    <application
        android:name=".TungstenApplication"
        android:label="@string/app_name"
        android:icon="@mipmap/app_icon"
        android:theme="@style/Theme.Chromium.Main"
        android:hardwareAccelerated="true"
        android:largeHeap="true"
        android:networkSecurityConfig="@xml/network_security_config">
        
        <!-- Main Activity -->
        <activity
            android:name=".MainActivity"
            android:configChanges="orientation|keyboardHidden|keyboard|screenSize|mcc|mnc|density|screenLayout|smallestScreenSize|uiMode"
            android:windowSoftInputMode="adjustResize"
            android:launchMode="singleTask"
            android:exported="true">
            
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
            
            <!-- nostr:// protocol handler -->
            <intent-filter>
                <action android:name="android.intent.action.VIEW" />
                <category android:name="android.intent.category.DEFAULT" />
                <category android:name="android.intent.category.BROWSABLE" />
                <data android:scheme="nostr" />
            </intent-filter>
            
            <!-- Web URLs -->
            <intent-filter>
                <action android:name="android.intent.action.VIEW" />
                <category android:name="android.intent.category.DEFAULT" />
                <category android:name="android.intent.category.BROWSABLE" />
                <data android:scheme="http" />
                <data android:scheme="https" />
            </intent-filter>
        </activity>
        
        <!-- Nostr Service -->
        <service
            android:name=".services.NostrService"
            android:exported="false" />
        
        <!-- Local Relay Service -->
        <service
            android:name=".services.LocalRelayService"
            android:exported="false" />
        
        <!-- Content Provider for Blossom -->
        <provider
            android:name=".providers.BlossomProvider"
            android:authorities="com.tungsten.browser.blossom"
            android:exported="false" />
            
    </application>
</manifest>
```

#### 6.2 Java/Kotlin Integration
```java
// android/java/src/com/tungsten/browser/TungstenApplication.java
package com.tungsten.browser;

import android.app.Application;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;

public class TungstenApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        
        // Initialize Chromium
        ApplicationStatus.initialize(this);
        
        // Enable Tungsten features
        CommandLine.getInstance().appendSwitch("enable-features", 
            "NostrSupport,LocalRelay,BlossomServer");
        
        // Initialize Nostr components
        NostrManager.initialize(this);
        LocalRelayManager.initialize(this);
        BlossomManager.initialize(this);
    }
}

// android/java/src/com/tungsten/browser/NostrManager.java
package com.tungsten.browser;

import android.content.Context;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

@JNINamespace("tungsten")
public class NostrManager {
    private static NostrManager sInstance;
    private final Context mContext;
    
    public static void initialize(Context context) {
        if (sInstance == null) {
            sInstance = new NostrManager(context);
        }
    }
    
    private NostrManager(Context context) {
        mContext = context.getApplicationContext();
        nativeInit();
    }
    
    @CalledByNative
    public void onNostrEvent(String eventJson) {
        // Handle Nostr event from native
    }
    
    @CalledByNative
    public String getNostrPublicKey() {
        // Return stored public key
        return PreferenceManager.getDefaultSharedPreferences(mContext)
            .getString("nostr_public_key", null);
    }
    
    private native void nativeInit();
    private native void nativeSendEvent(String eventJson);
}
```

### 7. WebView Implementation

#### 7.1 System WebView Build
```python
def build_system_webview(self):
    """Build Tungsten as System WebView provider"""
    print("Building System WebView...")
    
    # Additional WebView-specific args
    webview_args = """
# System WebView configuration
android_system_webview = true
system_webview_package_name = "com.tungsten.webview"
chrome_public_manifest_package = "com.tungsten.browser"

# WebView features
enable_webview_bundles = true
webview_includes_weblayer = false
"""
    
    # Append to existing args.gn
    args_path = self.out_dir / "args.gn"
    with open(args_path, 'a') as f:
        f.write(webview_args)
    
    # Build WebView targets
    targets = [
        "system_webview_apk",
        "system_webview_shell_apk",
        "system_webview_unittests"
    ]
    
    subprocess.run([
        "autoninja", "-C", str(self.out_dir)
    ] + targets, check=True)
```

### 8. Performance Optimization

#### 8.1 Android-Specific Optimizations
```python
def get_android_optimization_flags(arch):
    """Get Android-specific optimization flags"""
    flags = {
        'use_thin_lto': True,
        'enable_profiling': False,
        'android_full_debug': False,
        'is_cfi': True,  # Control Flow Integrity
        'use_cfi_cast': True,
        'use_relative_vtables': True,
    }
    
    # Architecture-specific optimizations
    if arch == 'arm64-v8a':
        flags['arm_use_neon'] = True
        flags['arm_optionally_use_neon'] = False
    elif arch == 'armeabi-v7a':
        flags['arm_use_neon'] = True
        flags['arm_optionally_use_neon'] = True
        flags['arm_float_abi'] = 'softfp'
    
    # APK size optimizations
    flags['enable_resource_allowlist_generation'] = True
    flags['strip_debug_info'] = True
    
    return flags
```

### 9. Testing Framework

#### 9.1 Android Test Runner
```bash
#!/bin/bash
# scripts/run_android_tests.sh

set -e

ARCH="${1:-arm64-v8a}"
BUILD_DIR="out/android_$ARCH"

# Function to wait for device
wait_for_device() {
    echo "Waiting for device..."
    adb wait-for-device
    while [[ $(adb shell getprop sys.boot_completed | tr -d '\r') != "1" ]]; do
        sleep 1
    done
    echo "Device ready"
}

# Install test APKs
echo "Installing test APKs..."
adb install -r "$BUILD_DIR/apks/ChromePublic.apk"
adb install -r "$BUILD_DIR/apks/ChromePublicTest.apk"

# Wait for device to be ready
wait_for_device

# Run instrumentation tests
echo "Running instrumentation tests..."
"$BUILD_DIR/bin/run_chrome_public_test_apk" \
    --test-apk=ChromePublicTest \
    --gtest_filter="Tungsten*:Nostr*" \
    --verbose

# Run unit tests
echo "Running unit tests..."
"$BUILD_DIR/bin/run_chrome_junit_tests" \
    --verbose

# Run WebView tests if built
if [ -f "$BUILD_DIR/apks/SystemWebView.apk" ]; then
    echo "Running WebView tests..."
    adb install -r "$BUILD_DIR/apks/SystemWebView.apk"
    "$BUILD_DIR/bin/run_system_webview_shell_apk"
fi

echo "All Android tests passed!"
```

### 10. Troubleshooting Guide

#### 10.1 Common Android Build Issues

| Issue | Solution |
|-------|----------|
| NDK not found | Set ANDROID_NDK_ROOT environment variable |
| Java version mismatch | Use Java 11, set JAVA_HOME |
| SDK licenses not accepted | Run `sdkmanager --licenses` |
| Out of memory | Increase heap: `-Xmx4g` |
| APK signing fails | Check keystore path and password |
| Emulator won't start | Enable virtualization in BIOS |

#### 10.2 Debug Commands
```bash
# Check Android SDK installation
sdkmanager --list

# Verify NDK installation
ls $ANDROID_HOME/ndk/

# Check Java version
java -version

# List connected devices
adb devices

# Check APK contents
aapt dump badging out/android_arm64-v8a/apks/ChromePublic.apk

# Analyze APK size
$ANDROID_HOME/cmdline-tools/latest/bin/apkanalyzer apk summary out/android_arm64-v8a/apks/ChromePublic.apk

# Check native libraries
unzip -l out/android_arm64-v8a/apks/ChromePublic.apk | grep "\.so$"

# Monitor build progress
tail -f out/android_arm64-v8a/.ninja_log

# Debug app crashes
adb logcat chromium:V *:S
```

This comprehensive Android build implementation provides full support for multiple architectures, APK/AAB generation, WebView builds, and Play Store distribution while maintaining compatibility with CI/CD automation.