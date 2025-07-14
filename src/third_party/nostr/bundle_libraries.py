#!/usr/bin/env python3
# Copyright 2024 The Tungsten Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Downloads and bundles Nostr JavaScript libraries for Tungsten browser."""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

# Library definitions with their CDN URLs and expected hashes
LIBRARIES = {
    'ndk': {
        'version': '2.0.0',
        'url': 'https://unpkg.com/@nostr-dev-kit/ndk@2.0.0/dist/ndk.browser.min.js',
        'output': 'ndk-2.0.0.min.js',
        'hash': None,  # Will be populated after first download
    },
    'nostr-tools': {
        'version': '1.17.0',
        'url': 'https://unpkg.com/nostr-tools@1.17.0/lib/nostr.min.js',
        'output': 'nostr-tools-1.17.0.min.js',
        'hash': None,
    },
    'applesauce': {
        'version': '0.5.0',
        'url': 'https://unpkg.com/@coracle/applesauce@0.5.0/dist/index.min.js',
        'output': 'applesauce-0.5.0.min.js',
        'hash': None,
    },
    'nostrify': {
        'version': '1.2.0',
        'url': 'https://unpkg.com/@soapbox/nostrify@1.2.0/dist/index.min.js',
        'output': 'nostrify-1.2.0.min.js',
        'hash': None,
    },
    'alby-sdk': {
        'version': '3.0.0',
        'url': 'https://unpkg.com/@getalby/sdk@3.0.0/dist/index.browser.min.js',
        'output': 'alby-sdk-3.0.0.min.js',
        'hash': None,
    },
}

# Hash file to verify integrity
HASH_FILE = 'library_hashes.json'


def calculate_sha256(file_path):
    """Calculate SHA-256 hash of a file."""
    sha256_hash = hashlib.sha256()
    with open(file_path, 'rb') as f:
        for byte_block in iter(lambda: f.read(4096), b''):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()


def download_file(url, output_path):
    """Download a file from URL to the specified path."""
    print(f'Downloading {url}...')
    try:
        with urllib.request.urlopen(url) as response:
            with open(output_path, 'wb') as out_file:
                shutil.copyfileobj(response, out_file)
        print(f'  -> Saved to {output_path}')
        return True
    except Exception as e:
        print(f'  -> ERROR: {e}')
        return False


def load_hashes():
    """Load expected hashes from the hash file."""
    if os.path.exists(HASH_FILE):
        with open(HASH_FILE, 'r') as f:
            return json.load(f)
    return {}


def save_hashes(hashes):
    """Save hashes to the hash file."""
    with open(HASH_FILE, 'w') as f:
        json.dump(hashes, f, indent=2, sort_keys=True)


def verify_library(lib_name, lib_info, output_dir):
    """Download and verify a library."""
    output_path = os.path.join(output_dir, lib_info['output'])
    
    # Check if file already exists and matches hash
    if os.path.exists(output_path):
        current_hash = calculate_sha256(output_path)
        if lib_info.get('hash') == current_hash:
            print(f'{lib_name}: Already up to date')
            return True
        else:
            print(f'{lib_name}: Hash mismatch, re-downloading...')
    
    # Download the library
    if not download_file(lib_info['url'], output_path):
        return False
    
    # Calculate and verify hash
    actual_hash = calculate_sha256(output_path)
    
    if lib_info.get('hash') is None:
        # First time download, save the hash
        print(f'  -> Hash: {actual_hash} (saved for future verification)')
        return actual_hash
    elif lib_info['hash'] != actual_hash:
        print(f'  -> ERROR: Hash mismatch!')
        print(f'     Expected: {lib_info["hash"]}')
        print(f'     Actual:   {actual_hash}')
        os.remove(output_path)
        return False
    else:
        print(f'  -> Hash verified: {actual_hash}')
        return True


def create_wrapper(lib_name, lib_info, output_dir):
    """Create a wrapper file that exports the library properly."""
    wrapper_name = lib_name.replace('-', '_') + '_wrapper.js'
    wrapper_path = os.path.join(output_dir, wrapper_name)
    
    # Simple wrapper that ensures the library is available as a module
    wrapper_content = f"""// Auto-generated wrapper for {lib_name} v{lib_info['version']}
// This ensures the library can be imported as an ES module

(function() {{
  const originalExports = window.{lib_name.replace('-', '_')}_exports || {{}};
  
  // Load the library
  {open(os.path.join(output_dir, lib_info['output']), 'r').read()}
  
  // Export for ES module usage
  if (typeof module !== 'undefined' && module.exports) {{
    module.exports = window.{lib_name.replace('-', '_')} || originalExports;
  }}
}})();
"""
    
    with open(wrapper_path, 'w') as f:
        f.write(wrapper_content)
    
    print(f'  -> Created wrapper: {wrapper_name}')


def main():
    parser = argparse.ArgumentParser(description='Bundle Nostr JavaScript libraries')
    parser.add_argument('--output-dir', default='.',
                        help='Output directory for bundled libraries')
    parser.add_argument('--verify-only', action='store_true',
                        help='Only verify existing files, do not download')
    parser.add_argument('--update-hashes', action='store_true',
                        help='Update the hash file with current hashes')
    args = parser.parse_args()
    
    # Create output directory if it doesn't exist
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Load existing hashes
    stored_hashes = load_hashes()
    new_hashes = {}
    
    # Update library definitions with stored hashes
    for lib_name, lib_info in LIBRARIES.items():
        if lib_name in stored_hashes:
            lib_info['hash'] = stored_hashes[lib_name]
    
    # Process each library
    success = True
    for lib_name, lib_info in LIBRARIES.items():
        print(f'\nProcessing {lib_name} v{lib_info["version"]}...')
        
        if args.verify_only:
            output_path = os.path.join(args.output_dir, lib_info['output'])
            if os.path.exists(output_path):
                actual_hash = calculate_sha256(output_path)
                new_hashes[lib_name] = actual_hash
                if lib_info.get('hash') == actual_hash:
                    print(f'  -> Verified: {actual_hash}')
                else:
                    print(f'  -> Hash mismatch!')
                    success = False
            else:
                print(f'  -> File not found: {output_path}')
                success = False
        else:
            result = verify_library(lib_name, lib_info, args.output_dir)
            if isinstance(result, str):
                # Got a hash for first-time download
                new_hashes[lib_name] = result
            elif result:
                new_hashes[lib_name] = lib_info['hash']
            else:
                success = False
    
    # Save updated hashes if requested or if this was first download
    if args.update_hashes or not stored_hashes:
        save_hashes(new_hashes)
        print(f'\nUpdated {HASH_FILE}')
    
    if success:
        print('\nAll libraries bundled successfully!')
        return 0
    else:
        print('\nERROR: Some libraries failed to bundle')
        return 1


if __name__ == '__main__':
    sys.exit(main())