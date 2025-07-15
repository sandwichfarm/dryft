#!/usr/bin/env python3
"""
Test suite for Tungsten Browser Build System
"""

import unittest
import tempfile
import shutil
import sys
import os
from pathlib import Path
from unittest.mock import Mock, patch, MagicMock

# Add parent directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import build


class TestBuildConfig(unittest.TestCase):
    """Test BuildConfig class"""
    
    def setUp(self):
        self.config = build.BuildConfig()
    
    def test_platform_detection(self):
        """Test platform detection"""
        self.assertIn(self.config.platform, ["linux", "windows", "macos", "raspi"])
    
    def test_arch_detection(self):
        """Test architecture detection"""
        self.assertIn(self.config.arch, ["x64", "arm64", "arm32", "x86_64", "aarch64"])
    
    def test_paths(self):
        """Test path configuration"""
        self.assertTrue(self.config.script_dir.exists())
        self.assertEqual(self.config.src_dir, self.config.script_dir / "src")
        self.assertEqual(self.config.out_dir, self.config.src_dir / "out")


class TestTungstenBuilder(unittest.TestCase):
    """Test TungstenBuilder class"""
    
    def setUp(self):
        self.config = build.BuildConfig()
        self.builder = build.TungstenBuilder(self.config)
        
    @patch('shutil.which')
    def test_find_depot_tools(self, mock_which):
        """Test depot_tools detection"""
        # Test when gclient is in PATH
        mock_which.return_value = "/usr/local/bin/gclient"
        self.assertTrue(self.builder._find_depot_tools())
        self.assertEqual(self.config.depot_tools_path, Path("/usr/local/bin"))
        
        # Test when gclient is not in PATH
        mock_which.return_value = None
        with patch('pathlib.Path.exists') as mock_exists:
            mock_exists.return_value = False
            self.assertFalse(self.builder._find_depot_tools())
    
    def test_prepare_build_args(self):
        """Test build arguments preparation"""
        args = Mock()
        args.release = True
        args.debug = False
        args.gn_args = ["use_vaapi=true", "use_pulseaudio=true"]
        args.pgo = False
        args.lto = True
        args.use_cache = False
        
        build_args = self.builder.prepare_build_args(args)
        
        # Check base arguments
        self.assertEqual(build_args["is_official_build"], "true")
        self.assertEqual(build_args["is_debug"], "false")
        self.assertEqual(build_args["enable_nostr"], "true")
        self.assertEqual(build_args["enable_local_relay"], "true")
        self.assertEqual(build_args["enable_blossom_server"], "true")
        
        # Check custom arguments
        self.assertEqual(build_args["use_vaapi"], "true")
        self.assertEqual(build_args["use_pulseaudio"], "true")
        
        # Check LTO
        self.assertEqual(build_args["use_thin_lto"], "true")
    
    @patch('subprocess.run')
    def test_run_gn_gen(self, mock_run):
        """Test GN generation"""
        mock_run.return_value.returncode = 0
        
        with tempfile.TemporaryDirectory() as temp_dir:
            build_dir = Path(temp_dir) / "out" / "Release"
            gn_args = {"is_debug": "false", "target_os": "linux"}
            
            result = self.builder.run_gn_gen(build_dir, gn_args)
            
            self.assertTrue(result)
            self.assertTrue(build_dir.exists())
            self.assertTrue((build_dir / "args.gn").exists())
            
            # Check args.gn content
            args_content = (build_dir / "args.gn").read_text()
            self.assertIn("is_debug = false", args_content)
            self.assertIn("target_os = linux", args_content)
    
    @patch('build.TungstenBuilder._get_version')
    def test_package_linux(self, mock_version):
        """Test Linux packaging"""
        mock_version.return_value = "1.0.0"
        
        with tempfile.TemporaryDirectory() as temp_dir:
            build_dir = Path(temp_dir) / "build"
            chrome_dir = build_dir / "chrome"
            chrome_dir.mkdir(parents=True)
            
            # Create dummy chrome binary
            (chrome_dir / "chrome").touch()
            (chrome_dir / "chrome").chmod(0o755)
            
            args = Mock()
            
            # Patch subprocess to avoid actual tar execution
            with patch('subprocess.run') as mock_run:
                mock_run.return_value.returncode = 0
                result = self.builder._package_linux(build_dir, args)
            
            self.assertTrue(result)
            
            # Check artifact directory was created
            artifact_dir = self.config.script_dir / "artifacts" / "linux" / self.config.arch
            self.assertTrue(artifact_dir.exists())


class TestPlatformDetection(unittest.TestCase):
    """Test platform-specific detection"""
    
    @patch('platform.system')
    @patch('platform.machine')
    def test_linux_x64_detection(self, mock_machine, mock_system):
        """Test Linux x64 detection"""
        mock_system.return_value = "Linux"
        mock_machine.return_value = "x86_64"
        
        config = build.BuildConfig()
        self.assertEqual(config.platform, "linux")
        self.assertEqual(config.arch, "x64")
    
    @patch('platform.system')
    @patch('platform.machine')
    def test_macos_arm64_detection(self, mock_machine, mock_system):
        """Test macOS ARM64 detection"""
        mock_system.return_value = "Darwin"
        mock_machine.return_value = "arm64"
        
        config = build.BuildConfig()
        self.assertEqual(config.platform, "macos")
        self.assertEqual(config.arch, "arm64")
    
    @patch('platform.system')
    def test_windows_detection(self, mock_system):
        """Test Windows detection"""
        mock_system.return_value = "Windows"
        
        config = build.BuildConfig()
        self.assertEqual(config.platform, "windows")


class TestBuildCache(unittest.TestCase):
    """Test build cache functionality"""
    
    def setUp(self):
        self.config = build.BuildConfig()
        self.builder = build.TungstenBuilder(self.config)
    
    @patch('shutil.which')
    @patch('os.environ')
    def test_ccache_setup(self, mock_environ, mock_which):
        """Test ccache setup"""
        mock_which.return_value = "/usr/bin/ccache"
        mock_environ.__setitem__ = Mock()
        
        self.builder._setup_build_cache()
        
        # Check environment variables were set
        mock_environ.__setitem__.assert_any_call("CCACHE_DIR", 
            str(Path.home() / ".cache" / "tungsten-build" / "ccache"))
        mock_environ.__setitem__.assert_any_call("CCACHE_COMPRESS", "1")
        mock_environ.__setitem__.assert_any_call("CCACHE_MAXSIZE", "20G")
    
    @patch('shutil.which')
    @patch('subprocess.run')
    def test_sccache_setup_windows(self, mock_run, mock_which):
        """Test sccache setup on Windows"""
        self.config.platform = "windows"
        
        def which_side_effect(cmd):
            if cmd == "ccache":
                return None
            elif cmd == "sccache":
                return "C:/tools/sccache.exe"
            return None
        
        mock_which.side_effect = which_side_effect
        mock_run.return_value.returncode = 0
        
        self.builder._setup_build_cache()
        
        # Check sccache was started
        mock_run.assert_called_with(["sccache", "--start-server"], capture_output=True)


class TestVersionDetection(unittest.TestCase):
    """Test version detection"""
    
    def setUp(self):
        self.config = build.BuildConfig()
        self.builder = build.TungstenBuilder(self.config)
    
    @patch('subprocess.run')
    def test_git_version(self, mock_run):
        """Test version from git describe"""
        mock_run.return_value.stdout = "v1.2.3-45-gabcdef\n"
        mock_run.return_value.returncode = 0
        
        version = self.builder._get_version()
        self.assertEqual(version, "v1.2.3-45-gabcdef")
    
    @patch('subprocess.run')
    def test_version_file_fallback(self, mock_run):
        """Test version from VERSION file"""
        mock_run.side_effect = Exception("Git not available")
        
        with tempfile.TemporaryDirectory() as temp_dir:
            # Create mock VERSION file
            version_dir = Path(temp_dir) / "src" / "chrome"
            version_dir.mkdir(parents=True)
            version_file = version_dir / "VERSION"
            version_file.write_text("""MAJOR=120
MINOR=0
BUILD=6099
PATCH=109""")
            
            # Temporarily override src_dir
            original_src_dir = self.config.src_dir
            self.config.src_dir = Path(temp_dir) / "src"
            
            version = self.builder._get_version()
            
            self.config.src_dir = original_src_dir
            
        self.assertEqual(version, "120.0.6099.109")


class TestArgumentParsing(unittest.TestCase):
    """Test command line argument parsing"""
    
    def test_default_args(self):
        """Test default arguments"""
        with patch('sys.argv', ['build.py']):
            args = build.main.__wrapped__()
            self.assertTrue(args.release)
            self.assertFalse(args.debug)
            self.assertTrue(args.use_cache)
    
    def test_platform_override(self):
        """Test platform override"""
        with patch('sys.argv', ['build.py', '--platform=linux', '--arch=arm64']):
            args = build.main.__wrapped__()
            # Would need to refactor main() to return args for proper testing


if __name__ == '__main__':
    # Run tests with verbosity
    unittest.main(verbosity=2)