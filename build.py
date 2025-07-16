#!/usr/bin/env python3
"""
Tungsten Browser Unified Build System
Main entry point for cross-platform builds
"""

import argparse
import json
import os
import platform
import subprocess
import sys
import shutil
import tempfile
from pathlib import Path
from typing import Dict, List, Optional, Tuple
import multiprocessing


class BuildConfig:
    """Build configuration management"""
    
    def __init__(self):
        self.platform = self._detect_platform()
        self.arch = self._detect_arch()
        self.script_dir = Path(__file__).parent
        
        # Check for CR_DIR environment variable first (Thorium/Tungsten standard)
        if os.environ.get("CR_DIR"):
            self.src_dir = Path(os.environ["CR_DIR"])
        # Check if we're in CI with cached Chromium
        elif os.environ.get("CI") and (self.script_dir / "chromium" / "src").exists():
            self.src_dir = self.script_dir / "chromium" / "src"
        # Check standard Chromium location
        elif (Path.home() / "chromium" / "src").exists():
            self.src_dir = Path.home() / "chromium" / "src"
        else:
            self.src_dir = self.script_dir / "src"
            
        self.out_dir = self.src_dir / "out"
        self.depot_tools_path = None
        
    def _detect_platform(self) -> str:
        """Detect the current platform"""
        system = platform.system().lower()
        if system == "darwin":
            return "macos"
        elif system == "windows":
            return "windows"
        elif system == "linux":
            # Check if running on Raspberry Pi
            try:
                with open("/proc/device-tree/model", "r") as f:
                    if "raspberry pi" in f.read().lower():
                        return "raspi"
            except:
                pass
            return "linux"
        else:
            raise ValueError(f"Unsupported platform: {system}")
    
    def _detect_arch(self) -> str:
        """Detect the current architecture"""
        machine = platform.machine().lower()
        if machine in ["x86_64", "amd64"]:
            return "x64"
        elif machine in ["aarch64", "arm64"]:
            return "arm64"
        elif machine in ["armv7l", "armv7"]:
            return "arm32"
        else:
            return machine


class TungstenBuilder:
    """Main builder class for Tungsten browser"""
    
    def __init__(self, config: BuildConfig):
        self.config = config
        self.build_args = {}
        
    def setup_environment(self) -> bool:
        """Setup build environment"""
        print(f"Setting up build environment for {self.config.platform}/{self.config.arch}")
        
        # Check if source directory exists
        if not self.config.src_dir.exists():
            print(f"ERROR: Chromium source not found at {self.config.src_dir}")
            print("Please run one of the following:")
            print("  1. make fetch-chromium  # Download full Chromium source")
            print("  2. export CR_DIR=/path/to/chromium/src  # Use existing checkout")
            return False
            
        # Check for depot_tools
        if not self._find_depot_tools():
            print("ERROR: depot_tools not found. Please install depot_tools and add to PATH")
            return False
            
        # Platform-specific environment setup
        if self.config.platform == "windows":
            return self._setup_windows_env()
        elif self.config.platform == "macos":
            return self._setup_macos_env()
        elif self.config.platform == "linux":
            return self._setup_linux_env()
        elif self.config.platform == "raspi":
            return self._setup_raspi_env()
        elif self.config.platform == "android":
            return self._setup_android_env()
        
        return True
    
    def _find_depot_tools(self) -> bool:
        """Find depot_tools in PATH or common locations"""
        # Check PATH first
        if shutil.which("gclient"):
            self.config.depot_tools_path = Path(shutil.which("gclient")).parent
            return True
            
        # Check common locations
        common_paths = [
            Path.home() / "depot_tools",
            Path("/opt/depot_tools"),
            self.config.script_dir.parent / "depot_tools"
        ]
        
        for path in common_paths:
            if path.exists() and (path / "gclient").exists():
                self.config.depot_tools_path = path
                # Add to PATH for this session
                os.environ["PATH"] = str(path) + os.pathsep + os.environ.get("PATH", "")
                return True
                
        return False
    
    def _setup_windows_env(self) -> bool:
        """Setup Windows build environment"""
        # Check for Visual Studio
        vs_path = self._find_visual_studio()
        if not vs_path:
            print("ERROR: Visual Studio 2022 not found")
            return False
            
        # Set Windows SDK environment
        os.environ["DEPOT_TOOLS_WIN_TOOLCHAIN"] = "0"
        return True
    
    def _setup_macos_env(self) -> bool:
        """Setup macOS build environment"""
        # Check for Xcode
        result = subprocess.run(["xcode-select", "-p"], capture_output=True, text=True)
        if result.returncode != 0:
            print("ERROR: Xcode not found. Please install Xcode and run xcode-select --install")
            return False
        return True
    
    def _setup_linux_env(self) -> bool:
        """Setup Linux build environment"""
        # Check for required packages (basic check)
        required_commands = ["python3", "ninja", "pkg-config"]
        for cmd in required_commands:
            if not shutil.which(cmd):
                print(f"ERROR: {cmd} not found. Please run ./scripts/install-build-deps.sh")
                return False
        return True
    
    def _setup_raspi_env(self) -> bool:
        """Setup Raspberry Pi build environment"""
        # Similar to Linux but with ARM-specific checks
        return self._setup_linux_env()
    
    def _setup_android_env(self) -> bool:
        """Setup Android build environment"""
        # Check for Android SDK/NDK
        if not os.environ.get("ANDROID_HOME"):
            print("ERROR: ANDROID_HOME not set. Please install Android SDK")
            return False
        return True
    
    def _find_visual_studio(self) -> Optional[Path]:
        """Find Visual Studio installation on Windows"""
        # Common VS2022 paths
        vs_paths = [
            Path("C:/Program Files/Microsoft Visual Studio/2022/Enterprise"),
            Path("C:/Program Files/Microsoft Visual Studio/2022/Professional"),
            Path("C:/Program Files/Microsoft Visual Studio/2022/Community"),
        ]
        
        for path in vs_paths:
            if path.exists():
                return path
        return None
    
    def _setup_build_cache(self) -> None:
        """Setup build caching with ccache/sccache"""
        cache_dir = Path.home() / ".cache" / "tungsten-build"
        cache_dir.mkdir(parents=True, exist_ok=True)
        
        # Setup ccache
        if shutil.which("ccache"):
            os.environ["CCACHE_DIR"] = str(cache_dir / "ccache")
            os.environ["CCACHE_COMPRESS"] = "1"
            os.environ["CCACHE_COMPRESSLEVEL"] = "6"
            os.environ["CCACHE_MAXSIZE"] = "20G"
            print("Build caching enabled with ccache")
        
        # Setup sccache for Windows
        if self.config.platform == "windows" and shutil.which("sccache"):
            os.environ["SCCACHE_DIR"] = str(cache_dir / "sccache")
            os.environ["SCCACHE_CACHE_SIZE"] = "20G"
            subprocess.run(["sccache", "--start-server"], capture_output=True)
            print("Build caching enabled with sccache")
    
    def prepare_build_args(self, args: argparse.Namespace) -> Dict[str, str]:
        """Prepare GN build arguments"""
        # Base arguments for all platforms
        base_args = {
            "is_official_build": "true" if args.release else "false",
            "is_debug": "true" if args.debug else "false",
            "symbol_level": "2" if args.debug else "0",
            "enable_nacl": "false",
            "proprietary_codecs": "true",
            "ffmpeg_branding": "Chrome",
            
            # Tungsten-specific features
            "enable_nostr": "true",
            "enable_local_relay": "true",
            "enable_blossom_server": "true",
            "enable_nsite": "true",
        }
        
        # Add caching support
        if args.use_cache:
            self._setup_build_cache()
            base_args["cc_wrapper"] = "ccache"
        
        # Platform-specific arguments
        platform_args = self._get_platform_args(args)
        base_args.update(platform_args)
        
        # Custom args from command line
        if args.gn_args:
            for arg in args.gn_args:
                key, value = arg.split("=", 1)
                base_args[key] = value
        
        return base_args
    
    def _get_platform_args(self, args: argparse.Namespace) -> Dict[str, str]:
        """Get platform-specific GN arguments"""
        platform_args = {}
        
        if self.config.platform == "windows":
            platform_args.update({
                "target_os": "win",
                "target_cpu": self.config.arch,
            })
        elif self.config.platform == "macos":
            platform_args.update({
                "target_os": "mac",
                "target_cpu": self.config.arch,
                "use_system_xcode": "true",
            })
        elif self.config.platform == "linux":
            platform_args.update({
                "target_os": "linux",
                "target_cpu": self.config.arch,
                "use_sysroot": "true",
            })
        elif self.config.platform == "raspi":
            platform_args.update({
                "target_os": "linux",
                "target_cpu": "arm64",
                "arm_version": "8",
                "arm_arch": "armv8-a",
                "use_sysroot": "true",
            })
        elif self.config.platform == "android":
            platform_args.update({
                "target_os": "android",
                "target_cpu": args.android_arch or "arm64",
                "android_channel": "stable",
            })
        
        # Optimization flags
        if args.pgo:
            platform_args["chrome_pgo_phase"] = "2"
            
        if args.lto:
            platform_args["use_thin_lto"] = "true"
            
        return platform_args
    
    def run_gn_gen(self, build_dir: Path, gn_args: Dict[str, str]) -> bool:
        """Run GN generation"""
        print(f"Generating build files in {build_dir}")
        
        # Create args.gn content
        args_content = "\n".join([f'{k} = {v}' for k, v in gn_args.items()])
        
        # Ensure build directory exists
        build_dir.mkdir(parents=True, exist_ok=True)
        
        # Write args.gn
        args_file = build_dir / "args.gn"
        args_file.write_text(args_content)
        
        # Run gn gen
        cmd = ["gn", "gen", str(build_dir)]
        result = subprocess.run(cmd, cwd=self.config.src_dir)
        
        return result.returncode == 0
    
    def run_ninja_build(self, build_dir: Path, targets: List[str], jobs: Optional[int] = None) -> bool:
        """Run Ninja build"""
        if not jobs:
            jobs = multiprocessing.cpu_count()
            
        print(f"Building targets: {', '.join(targets)} with {jobs} jobs")
        
        cmd = ["autoninja", "-C", str(build_dir), f"-j{jobs}"] + targets
        
        # Use subprocess with real-time output
        process = subprocess.Popen(
            cmd,
            cwd=self.config.src_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True
        )
        
        # Stream output
        for line in process.stdout:
            print(line, end='')
        
        process.wait()
        return process.returncode == 0
    
    def build(self, args: argparse.Namespace) -> bool:
        """Main build orchestration"""
        # Setup environment
        if not self.setup_environment():
            return False
        
        # Prepare build directory
        build_type = "Release" if args.release else "Debug"
        build_name = f"{build_type}-{self.config.platform}-{self.config.arch}"
        build_dir = self.config.out_dir / build_name
        
        # Prepare GN args
        gn_args = self.prepare_build_args(args)
        
        # Run GN generation
        if not self.run_gn_gen(build_dir, gn_args):
            print("ERROR: GN generation failed")
            return False
        
        # Determine build targets
        targets = args.targets or ["chrome"]
        
        # Run build
        if not self.run_ninja_build(build_dir, targets, args.jobs):
            print("ERROR: Build failed")
            return False
        
        print(f"\nBuild completed successfully!")
        print(f"Output directory: {build_dir}")
        
        # Run post-build steps if needed
        if args.package:
            return self.package_build(build_dir, args)
            
        return True
    
    def package_build(self, build_dir: Path, args: argparse.Namespace) -> bool:
        """Package the build outputs"""
        print(f"Packaging build from {build_dir}")
        
        # Platform-specific packaging
        if self.config.platform == "linux":
            return self._package_linux(build_dir, args)
        elif self.config.platform == "windows":
            return self._package_windows(build_dir, args)
        elif self.config.platform == "macos":
            return self._package_macos(build_dir, args)
        elif self.config.platform == "android":
            return self._package_android(build_dir, args)
        elif self.config.platform == "raspi":
            return self._package_raspi(build_dir, args)
            
        return True
    
    def _package_linux(self, build_dir: Path, args: argparse.Namespace) -> bool:
        """Package Linux build"""
        print(f"Packaging Linux build from {build_dir}")
        
        # Prepare artifact directory
        artifact_dir = self.config.script_dir / "artifacts" / "linux" / self.config.arch
        artifact_dir.mkdir(parents=True, exist_ok=True)
        
        # Get version info
        version = self._get_version()
        
        # Create tar.xz package
        package_name = f"tungsten-browser-{version}-{self.config.arch}"
        tar_path = artifact_dir / f"{package_name}.tar.xz"
        
        # Create temporary directory for packaging
        with tempfile.TemporaryDirectory() as temp_dir:
            package_dir = Path(temp_dir) / package_name
            package_dir.mkdir()
            
            # Copy browser files
            chrome_dir = build_dir / "chrome"
            if chrome_dir.exists():
                shutil.copytree(chrome_dir, package_dir / "tungsten", dirs_exist_ok=True)
            
            # Create launcher script
            launcher_script = package_dir / "tungsten"
            launcher_script.write_text(f"""#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${{BASH_SOURCE[0]}}" )" && pwd )"
exec "$SCRIPT_DIR/tungsten/chrome" "$@"
""")
            launcher_script.chmod(0o755)
            
            # Create tar.xz
            cmd = ["tar", "-cJf", str(tar_path), "-C", temp_dir, package_name]
            subprocess.run(cmd, check=True)
            
        print(f"Created {tar_path}")
        
        # Create .deb package
        if shutil.which("dpkg-deb"):
            self._create_deb_package(build_dir, artifact_dir, version)
        
        # Create .rpm package  
        if shutil.which("rpmbuild"):
            self._create_rpm_package(build_dir, artifact_dir, version)
            
        return True
    
    def _package_windows(self, build_dir: Path, args: argparse.Namespace) -> bool:
        """Package Windows build"""
        print(f"Packaging Windows build from {build_dir}")
        
        # Prepare artifact directory
        artifact_dir = self.config.script_dir / "artifacts" / "windows" / self.config.arch
        artifact_dir.mkdir(parents=True, exist_ok=True)
        
        # Get version info
        version = self._get_version()
        
        # Create portable ZIP package
        package_name = f"tungsten-browser-{version}-{self.config.arch}-portable"
        zip_path = artifact_dir / f"{package_name}.zip"
        
        # Create zip archive
        import zipfile
        with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            chrome_dir = build_dir / "chrome.exe"
            if chrome_dir.parent.exists():
                for file_path in chrome_dir.parent.rglob('*'):
                    if file_path.is_file():
                        arcname = file_path.relative_to(chrome_dir.parent)
                        zipf.write(file_path, arcname)
        
        print(f"Created {zip_path}")
        
        # Note: Full installer would require additional tools like NSIS or WiX
        print("Note: .exe installer requires NSIS or WiX toolchain")
        
        return True
    
    def _package_macos(self, build_dir: Path, args: argparse.Namespace) -> bool:
        """Package macOS build"""
        print(f"Packaging macOS build from {build_dir}")
        
        # Prepare artifact directory
        artifact_dir = self.config.script_dir / "artifacts" / "macos" / self.config.arch
        artifact_dir.mkdir(parents=True, exist_ok=True)
        
        # Get version info
        version = self._get_version()
        
        # Look for the app bundle
        app_bundle = build_dir / "Tungsten.app"
        if not app_bundle.exists():
            # Try alternate naming
            app_bundle = build_dir / "Chromium.app"
            if app_bundle.exists():
                # Rename to Tungsten
                new_app_bundle = build_dir / "Tungsten.app"
                shutil.move(str(app_bundle), str(new_app_bundle))
                app_bundle = new_app_bundle
        
        if not app_bundle.exists():
            print(f"ERROR: App bundle not found in {build_dir}")
            return False
        
        # Create DMG
        dmg_name = f"Tungsten-{version}-{self.config.arch}.dmg"
        dmg_path = artifact_dir / dmg_name
        
        # Simple DMG creation (for full featured DMG, use create-dmg tool)
        cmd = [
            "hdiutil", "create",
            "-volname", "Tungsten Browser",
            "-srcfolder", str(app_bundle),
            "-ov",
            "-format", "UDZO",
            str(dmg_path)
        ]
        
        subprocess.run(cmd, check=True)
        print(f"Created {dmg_path}")
        
        return True
    
    def _package_android(self, build_dir: Path, args: argparse.Namespace) -> bool:
        """Package Android build"""
        # TODO: Implement .apk packaging
        print("Android packaging not yet implemented")
        return True
    
    def _package_raspi(self, build_dir: Path, args: argparse.Namespace) -> bool:
        """Package Raspberry Pi build"""
        # TODO: Implement .deb packaging for ARM
        print("Raspberry Pi packaging not yet implemented")
        return True
    
    def _get_version(self) -> str:
        """Get version string from git or version file"""
        # Try git describe first
        try:
            result = subprocess.run(
                ["git", "describe", "--tags", "--always", "--dirty"],
                capture_output=True,
                text=True,
                check=True
            )
            return result.stdout.strip()
        except:
            pass
        
        # Fall back to version file
        version_file = self.config.src_dir / "chrome" / "VERSION"
        if version_file.exists():
            version_data = {}
            for line in version_file.read_text().splitlines():
                if "=" in line:
                    key, value = line.split("=", 1)
                    version_data[key.strip()] = value.strip()
            
            major = version_data.get("MAJOR", "0")
            minor = version_data.get("MINOR", "0")
            build = version_data.get("BUILD", "0")
            patch = version_data.get("PATCH", "0")
            
            return f"{major}.{minor}.{build}.{patch}"
        
        return "0.0.0.0"
    
    def _create_deb_package(self, build_dir: Path, artifact_dir: Path, version: str) -> None:
        """Create Debian package"""
        print("Creating .deb package...")
        
        with tempfile.TemporaryDirectory() as temp_dir:
            deb_root = Path(temp_dir) / "tungsten-browser"
            
            # Create debian control structure
            debian_dir = deb_root / "DEBIAN"
            debian_dir.mkdir(parents=True)
            
            # Create control file
            control_file = debian_dir / "control"
            control_file.write_text(f"""Package: tungsten-browser
Version: {version}
Section: web
Priority: optional
Architecture: {self.config.arch if self.config.arch != "x64" else "amd64"}
Depends: libc6, libgcc1, libgtk-3-0, libnotify4, libnss3, libxss1, libasound2, libxtst6
Maintainer: Tungsten Browser Team <noreply@tungsten.browser>
Description: Tungsten Browser - Chromium with native Nostr support
 Tungsten is a web browser based on Chromium/Thorium that includes
 native support for the Nostr protocol, including NIP-07 interface,
 local relay, Blossom server, and nostr:// URL scheme support.
""")
            
            # Create directory structure
            opt_dir = deb_root / "opt" / "tungsten"
            opt_dir.mkdir(parents=True)
            
            # Copy browser files
            chrome_dir = build_dir / "chrome"
            if chrome_dir.exists():
                shutil.copytree(chrome_dir, opt_dir, dirs_exist_ok=True)
            
            # Create desktop entry
            applications_dir = deb_root / "usr" / "share" / "applications"
            applications_dir.mkdir(parents=True)
            
            desktop_file = applications_dir / "tungsten-browser.desktop"
            desktop_file.write_text("""[Desktop Entry]
Name=Tungsten Browser
Comment=Browse the Web with Nostr support
GenericName=Web Browser
Exec=/opt/tungsten/chrome %U
Icon=/opt/tungsten/product_logo_256.png
Type=Application
Categories=Network;WebBrowser;
MimeType=text/html;text/xml;application/xhtml+xml;application/xml;application/rss+xml;application/rdf+xml;image/gif;image/jpeg;image/png;x-scheme-handler/http;x-scheme-handler/https;x-scheme-handler/nostr;
StartupNotify=true
""")
            
            # Build the deb
            deb_path = artifact_dir / f"tungsten-browser-{version}-{self.config.arch}.deb"
            cmd = ["dpkg-deb", "--build", str(deb_root), str(deb_path)]
            subprocess.run(cmd, check=True)
            
            print(f"Created {deb_path}")
    
    def _create_rpm_package(self, build_dir: Path, artifact_dir: Path, version: str) -> None:
        """Create RPM package"""
        print("Creating .rpm package...")
        # RPM creation is more complex and requires rpmbuild setup
        # For now, just note that it's not implemented
        print("RPM packaging requires rpmbuild setup - skipping")


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Tungsten Browser Unified Build System",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Auto-detect platform and build release
  ./build.py --release
  
  # Build debug version with specific platform
  ./build.py --debug --platform=linux --arch=x64
  
  # Build with custom GN args
  ./build.py --gn-args "use_vaapi=true" --gn-args "use_pulseaudio=true"
  
  # Build and package
  ./build.py --release --package
  
  # Build specific targets
  ./build.py --targets chrome chrome_sandbox
"""
    )
    
    # Platform options
    parser.add_argument(
        "--platform",
        choices=["auto", "linux", "windows", "macos", "android", "raspi"],
        default="auto",
        help="Target platform (default: auto-detect)"
    )
    parser.add_argument(
        "--arch",
        choices=["auto", "x64", "arm64", "arm32"],
        default="auto",
        help="Target architecture (default: auto-detect)"
    )
    
    # Build type options
    build_type = parser.add_mutually_exclusive_group()
    build_type.add_argument(
        "--release",
        action="store_true",
        help="Build release version (optimized)"
    )
    build_type.add_argument(
        "--debug",
        action="store_true",
        help="Build debug version"
    )
    
    # Build options
    parser.add_argument(
        "--jobs", "-j",
        type=int,
        help="Number of parallel build jobs (default: auto)"
    )
    parser.add_argument(
        "--targets",
        nargs="+",
        help="Specific build targets (default: chrome)"
    )
    parser.add_argument(
        "--gn-args",
        action="append",
        help="Additional GN arguments (can be specified multiple times)"
    )
    
    # Optimization options
    parser.add_argument(
        "--pgo",
        action="store_true",
        help="Enable Profile-Guided Optimization"
    )
    parser.add_argument(
        "--lto",
        action="store_true",
        help="Enable Link-Time Optimization"
    )
    
    # Output options
    parser.add_argument(
        "--package",
        action="store_true",
        help="Package the build after completion"
    )
    
    # Android-specific options
    parser.add_argument(
        "--android-arch",
        choices=["arm32", "arm64", "x86", "x64"],
        help="Android target architecture"
    )
    
    # Component build option
    parser.add_argument(
        "--component-build",
        action="store_true",
        help="Enable component build (faster linking, debug only)"
    )
    
    # Clean build option
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean build directory before building"
    )
    
    # Cache option
    parser.add_argument(
        "--use-cache",
        action="store_true",
        default=True,
        help="Use ccache/sccache for faster builds (default: enabled)"
    )
    parser.add_argument(
        "--no-cache",
        dest="use_cache",
        action="store_false",
        help="Disable build caching"
    )
    
    args = parser.parse_args()
    
    # Default to release if not specified
    if not args.release and not args.debug:
        args.release = True
    
    # Create build configuration
    config = BuildConfig()
    
    # Override auto-detected values if specified
    if args.platform != "auto":
        config.platform = args.platform
    if args.arch != "auto":
        config.arch = args.arch
    
    # Create builder and run build
    builder = TungstenBuilder(config)
    
    try:
        if builder.build(args):
            return 0
        else:
            return 1
    except KeyboardInterrupt:
        print("\nBuild interrupted by user")
        return 1
    except Exception as e:
        print(f"\nBuild failed with error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())