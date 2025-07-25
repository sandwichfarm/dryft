#!/usr/bin/env python3
# Copyright 2024 The dryft Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Shared configuration for Nostr JavaScript libraries."""

# Library definitions with versions and CDN URLs
LIBRARY_CONFIG = {
    'ndk': {
        'version': '2.0.0',
        'input_file': 'ndk-2.0.0.js',
        'output_file': 'ndk.js',
        'cdn_url': 'https://cdn.jsdelivr.net/npm/@nostr-dev-kit/ndk@2.0.0/dist/index.js',
        'sha256': None  # Will be calculated on first download
    },
    'nostr-tools': {
        'version': '1.17.0',
        'input_file': 'nostr-tools-1.17.0.js',
        'output_file': 'nostr-tools.js',
        'cdn_url': 'https://cdn.jsdelivr.net/npm/nostr-tools@1.17.0/lib/nostr.js',
        'sha256': None
    },
    'applesauce-core': {
        'version': '0.3.4',
        'input_file': 'applesauce-core-0.3.4.js',
        'output_file': 'applesauce-core.js',
        'cdn_url': 'https://cdn.jsdelivr.net/npm/@hzrd149/applesauce-core@0.3.4/dist/index.js',
        'sha256': None
    },
    'applesauce-content': {
        'version': '0.3.4',
        'input_file': 'applesauce-content-0.3.4.js',
        'output_file': 'applesauce-content.js',
        'cdn_url': 'https://cdn.jsdelivr.net/npm/@hzrd149/applesauce-content@0.3.4/dist/index.js',
        'sha256': None
    },
    'applesauce-lists': {
        'version': '0.3.4',
        'input_file': 'applesauce-lists-0.3.4.js',
        'output_file': 'applesauce-lists.js',
        'cdn_url': 'https://cdn.jsdelivr.net/npm/@hzrd149/applesauce-lists@0.3.4/dist/index.js',
        'sha256': None
    },
    'alby-sdk': {
        'version': '3.0.0',
        'input_file': 'alby-sdk-3.0.0.js',
        'output_file': 'alby-sdk.js',
        'cdn_url': 'https://cdn.jsdelivr.net/npm/@getalby/sdk@3.0.0/dist/index.js',
        'sha256': None
    }
}

def get_input_to_output_mapping():
    """Get a dictionary mapping input filenames to output filenames."""
    return {config['input_file']: config['output_file'] 
            for config in LIBRARY_CONFIG.values()}

def get_download_urls():
    """Get a dictionary mapping filenames to their CDN URLs."""
    return {config['input_file']: {
        'url': config['cdn_url'],
        'sha256': config['sha256']
    } for config in LIBRARY_CONFIG.values()}