#!/usr/bin/env python3
# Copyright 2024 The Tungsten Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to copy Nostr JavaScript libraries to generated output directory."""

import os
import shutil
import sys
import argparse


def main():
    parser = argparse.ArgumentParser(description='Copy Nostr libraries')
    parser.add_argument('--input-dir', required=True,
                       help='Input directory containing libraries')
    parser.add_argument('--output-dir', required=True,
                       help='Output directory for processed libraries')
    
    args = parser.parse_args()
    
    # Ensure output directory exists
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Library mapping from input to output names
    library_map = {
        'ndk-1.6.0.js': 'ndk.js',
        'nostr-tools-1.17.0.js': 'nostr-tools.js',
        'applesauce-1.0.0.js': 'applesauce.js',
        'nostrify-1.2.0.js': 'nostrify.js',
        'alby-sdk-2.0.0.js': 'alby-sdk.js',
    }
    
    for input_name, output_name in library_map.items():
        input_path = os.path.join(args.input_dir, input_name)
        output_path = os.path.join(args.output_dir, output_name)
        
        if os.path.exists(input_path):
            print(f'Copying {input_name} -> {output_name}')
            shutil.copy2(input_path, output_path)
        else:
            print(f'Warning: {input_path} not found, creating empty file')
            # Create empty file for build to succeed
            with open(output_path, 'w') as f:
                f.write('// Library not found during build\n')
    
    return 0


if __name__ == '__main__':
    sys.exit(main())