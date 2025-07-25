#!/usr/bin/env python3
# Copyright 2024 The dryft Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Download actual Nostr JavaScript libraries for dryft."""

import hashlib
import json
import os
import sys
import urllib.request

# Import shared library configuration
from library_config import get_download_urls


def calculate_sha256(file_path):
    """Calculate SHA-256 hash of a file."""
    sha256_hash = hashlib.sha256()
    with open(file_path, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()


def download_file(url, output_path):
    """Download a file from URL to output path."""
    try:
        print(f"Downloading {url}...")
        with urllib.request.urlopen(url) as response:
            data = response.read()
            with open(output_path, 'wb') as f:
                f.write(data)
        print(f"Downloaded to {output_path}")
        return True
    except Exception as e:
        print(f"Error downloading {url}: {e}")
        return False


def main():
    # Change to the third_party directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    third_party_dir = os.path.join(script_dir, 'third_party')
    
    if not os.path.exists(third_party_dir):
        os.makedirs(third_party_dir)
    
    os.chdir(third_party_dir)
    
    # Load existing hashes if available
    hash_file = 'library_hashes.json'
    if os.path.exists(hash_file):
        with open(hash_file, 'r') as f:
            saved_hashes = json.load(f)
    else:
        saved_hashes = {}
    
    # Get library download URLs from shared configuration
    libraries = get_download_urls()
    
    # Download and verify each library
    updated_hashes = {}
    for filename, info in libraries.items():
        output_path = filename
        
        # Check if file exists and matches hash
        if os.path.exists(output_path) and filename in saved_hashes:
            current_hash = calculate_sha256(output_path)
            if current_hash == saved_hashes[filename]:
                print(f"{filename} already exists and hash matches, skipping...")
                updated_hashes[filename] = current_hash
                continue
        
        # Download the file
        if not download_file(info['url'], output_path):
            print(f"Failed to download {filename}")
            return 1
        
        # Calculate and save hash
        file_hash = calculate_sha256(output_path)
        updated_hashes[filename] = file_hash
        print(f"SHA-256: {file_hash}")
    
    # Save updated hashes
    with open(hash_file, 'w') as f:
        json.dump(updated_hashes, f, indent=2)
    
    print("\nAll libraries downloaded successfully!")
    return 0


if __name__ == '__main__':
    sys.exit(main())