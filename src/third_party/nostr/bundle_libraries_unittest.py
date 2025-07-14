#!/usr/bin/env python3
# Copyright 2024 The Tungsten Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for bundle_libraries.py"""

import json
import os
import shutil
import tempfile
import unittest
from unittest.mock import patch, mock_open, MagicMock

import bundle_libraries


class TestBundleLibraries(unittest.TestCase):
    def setUp(self):
        # Create a temporary directory for test outputs
        self.test_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.test_dir)
        
    def test_calculate_sha256(self):
        """Test SHA-256 calculation."""
        # Create a test file
        test_file = os.path.join(self.test_dir, 'test.js')
        test_content = b'console.log("hello world");'
        with open(test_file, 'wb') as f:
            f.write(test_content)
        
        # Calculate hash
        hash_value = bundle_libraries.calculate_sha256(test_file)
        
        # Expected hash for the test content
        expected_hash = '8c7dd922ad47494fc02c388e12ce6d08d1f1c06b3cfbe75e1e09395ff27b9c7d'
        self.assertEqual(hash_value, expected_hash)
    
    def test_load_hashes(self):
        """Test loading hashes from file."""
        # Create a test hash file
        test_hashes = {
            'ndk': 'abc123',
            'nostr-tools': 'def456'
        }
        hash_file = os.path.join(self.test_dir, 'library_hashes.json')
        with open(hash_file, 'w') as f:
            json.dump(test_hashes, f)
        
        # Change to test directory and load hashes
        original_cwd = os.getcwd()
        try:
            os.chdir(self.test_dir)
            loaded_hashes = bundle_libraries.load_hashes()
            self.assertEqual(loaded_hashes, test_hashes)
        finally:
            os.chdir(original_cwd)
    
    def test_save_hashes(self):
        """Test saving hashes to file."""
        test_hashes = {
            'ndk': 'abc123',
            'nostr-tools': 'def456'
        }
        
        # Change to test directory and save hashes
        original_cwd = os.getcwd()
        try:
            os.chdir(self.test_dir)
            bundle_libraries.save_hashes(test_hashes)
            
            # Verify the file was created
            hash_file = os.path.join(self.test_dir, 'library_hashes.json')
            self.assertTrue(os.path.exists(hash_file))
            
            # Verify the content
            with open(hash_file, 'r') as f:
                loaded = json.load(f)
            self.assertEqual(loaded, test_hashes)
        finally:
            os.chdir(original_cwd)
    
    @patch('urllib.request.urlopen')
    def test_download_file_success(self, mock_urlopen):
        """Test successful file download."""
        # Mock the response
        mock_response = MagicMock()
        mock_response.__enter__.return_value = mock_response
        mock_response.__exit__.return_value = None
        mock_response.read.return_value = b'test content'
        mock_urlopen.return_value = mock_response
        
        # Download file
        output_path = os.path.join(self.test_dir, 'test.js')
        result = bundle_libraries.download_file('http://example.com/test.js', output_path)
        
        self.assertTrue(result)
        self.assertTrue(os.path.exists(output_path))
        with open(output_path, 'rb') as f:
            self.assertEqual(f.read(), b'test content')
    
    @patch('urllib.request.urlopen')
    def test_download_file_failure(self, mock_urlopen):
        """Test failed file download."""
        # Mock the response to raise an exception
        mock_urlopen.side_effect = Exception('Network error')
        
        # Download file
        output_path = os.path.join(self.test_dir, 'test.js')
        result = bundle_libraries.download_file('http://example.com/test.js', output_path)
        
        self.assertFalse(result)
        self.assertFalse(os.path.exists(output_path))
    
    def test_verify_library_first_time(self):
        """Test verifying a library for the first time."""
        # Create a mock library file
        lib_info = {
            'output': 'test-lib.js',
            'url': 'http://example.com/lib.js',
            'hash': None  # First time, no hash yet
        }
        
        lib_file = os.path.join(self.test_dir, lib_info['output'])
        with open(lib_file, 'w') as f:
            f.write('test library content')
        
        # Mock download_file to return True
        with patch('bundle_libraries.download_file', return_value=True):
            result = bundle_libraries.verify_library('test-lib', lib_info, self.test_dir)
        
        # Should return the calculated hash
        self.assertIsInstance(result, str)
        self.assertEqual(len(result), 64)  # SHA-256 hash length
    
    def test_verify_library_hash_match(self):
        """Test verifying a library with matching hash."""
        # Create a mock library file
        content = 'test library content'
        lib_file = os.path.join(self.test_dir, 'test-lib.js')
        with open(lib_file, 'w') as f:
            f.write(content)
        
        # Calculate the actual hash
        actual_hash = bundle_libraries.calculate_sha256(lib_file)
        
        lib_info = {
            'output': 'test-lib.js',
            'url': 'http://example.com/lib.js',
            'hash': actual_hash
        }
        
        result = bundle_libraries.verify_library('test-lib', lib_info, self.test_dir)
        self.assertTrue(result)
    
    def test_verify_library_hash_mismatch(self):
        """Test verifying a library with mismatched hash."""
        # Create a mock library file
        lib_file = os.path.join(self.test_dir, 'test-lib.js')
        with open(lib_file, 'w') as f:
            f.write('test library content')
        
        lib_info = {
            'output': 'test-lib.js',
            'url': 'http://example.com/lib.js',
            'hash': 'wrong_hash'
        }
        
        # Mock download_file to create a file with different content
        def mock_download(url, path):
            with open(path, 'w') as f:
                f.write('different content')
            return True
        
        with patch('bundle_libraries.download_file', side_effect=mock_download):
            result = bundle_libraries.verify_library('test-lib', lib_info, self.test_dir)
        
        self.assertFalse(result)
        # File should be removed after hash mismatch
        self.assertFalse(os.path.exists(lib_file))


if __name__ == '__main__':
    unittest.main()