#!/usr/bin/env python3
# Copyright 2024 The dryft Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Bundle Nostr JavaScript libraries for inclusion in Tungsten."""

import argparse
import os
import shutil
import sys

# Import shared library configuration
from library_config import get_input_to_output_mapping


def main():
    parser = argparse.ArgumentParser(description='Bundle Nostr libraries')
    parser.add_argument('--optimize', action='store_true',
                        help='Optimize the output files')
    parser.add_argument('--source-map', action='store_true',
                        help='Generate source maps')
    parser.add_argument('--input-dir', required=True,
                        help='Input directory containing library files')
    parser.add_argument('--output-dir', required=True,
                        help='Output directory for bundled files')
    
    args = parser.parse_args()
    
    # Create output directory if it doesn't exist
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Get library mappings from shared configuration
    library_map = get_input_to_output_mapping()
    
    # Process each library
    for input_name, output_name in library_map.items():
        input_path = os.path.join(args.input_dir, input_name)
        output_path = os.path.join(args.output_dir, output_name)
        
        if not os.path.exists(input_path):
            print(f"Error: Input file {input_path} not found", file=sys.stderr)
            return 1
        
        # For now, just copy the files
        # In a real implementation, we would:
        # 1. Minify if --optimize is set
        # 2. Generate source maps if --source-map is set
        # 3. Add any necessary wrappers or modifications
        
        try:
            shutil.copy2(input_path, output_path)
            print(f"Bundled {input_name} -> {output_name}")
        except Exception as e:
            print(f"Error bundling {input_name}: {e}", file=sys.stderr)
            return 1
    
    return 0


if __name__ == '__main__':
    sys.exit(main())