#!/bin/bash

# Script to resume Chromium sync after interruption

set -e

YEL='\033[1;33m' # Yellow
RED='\033[1;31m' # Red
GRE='\033[1;32m' # Green
c0='\033[0m' # Reset Text

CHROMIUM_DIR="${CHROMIUM_DIR:-$HOME/chromium}"

printf "\n${GRE}Resuming Chromium sync...${c0}\n\n"

# Check if chromium directory exists
if [ ! -d "$CHROMIUM_DIR" ]; then
    printf "${RED}Chromium directory not found at $CHROMIUM_DIR${c0}\n"
    exit 1
fi

cd "$CHROMIUM_DIR"

# First, check disk space
AVAILABLE_SPACE=$(df -k . | tail -1 | awk '{print $4}')
REQUIRED_SPACE=$((10 * 1024 * 1024))  # 10GB in KB

if [ "$AVAILABLE_SPACE" -lt "$REQUIRED_SPACE" ]; then
    printf "${RED}WARNING: Low disk space!${c0}\n"
    printf "Available: $(($AVAILABLE_SPACE / 1024 / 1024))GB\n"
    printf "Recommended: At least 10GB free\n"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

printf "${YEL}Cleaning up any failed partial clones...${c0}\n"
# Clean up any _gclient_* temporary directories
find . -name "_gclient_*" -type d -exec rm -rf {} + 2>/dev/null || true
# Clean up _bad_scm directory if it exists
rm -rf _bad_scm 2>/dev/null || true

printf "${YEL}Resuming sync (this will continue from where it left off)...${c0}\n"
printf "${YEL}Using caffeinate to prevent sleep...${c0}\n"

# Run gclient sync with retry logic
caffeinate -i gclient sync --nohooks --reset --force --delete_unversioned_trees

if [ -d "src" ]; then
    cd src
    printf "\n${YEL}Running gclient hooks...${c0}\n"
    gclient runhooks
    
    printf "\n${GRE}Chromium sync completed successfully!${c0}\n"
    printf "${YEL}Chromium is ready at: $CHROMIUM_DIR/src${c0}\n"
else
    printf "${RED}Error: src directory not found after sync${c0}\n"
    exit 1
fi