#!/bin/bash

# Script to fetch Chromium source for dryft/Thorium builds
# This handles the full Chromium checkout needed before applying patches

set -e

YEL='\033[1;33m' # Yellow
RED='\033[1;31m' # Red
GRE='\033[1;32m' # Green
c0='\033[0m' # Reset Text

# Error handling
yell() { echo "$0: $*" >&2; }
die() { yell "$*"; exit 111; }
try() { "$@" || die "${RED}Failed $*"; }

# Check if we're on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    die "This script is for macOS. Use get_repo.sh for Linux."
fi

# Set default Chromium location
CHROMIUM_DIR="${CHROMIUM_DIR:-$HOME/chromium}"
DRYFT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

printf "\n${GRE}dryft/Thorium Chromium Fetcher for macOS${c0}\n\n"

# Check if chromium directory already exists
if [ -d "$CHROMIUM_DIR/src" ]; then
    printf "${YEL}Chromium source already exists at $CHROMIUM_DIR/src${c0}\n"
    read -p "Do you want to update it? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        printf "${GRE}Using existing Chromium checkout${c0}\n"
        exit 0
    fi
    cd "$CHROMIUM_DIR"
    printf "${YEL}Updating existing Chromium checkout...${c0}\n"
    gclient sync --with_branch_heads --with_tags
    exit 0
elif [ -f "$CHROMIUM_DIR/.gclient" ]; then
    # Partial checkout exists
    printf "${YEL}Found partial Chromium checkout at $CHROMIUM_DIR${c0}\n"
    printf "${YEL}This appears to be an incomplete fetch.${c0}\n"
    read -p "Do you want to (c)ontinue the fetch or (d)elete and start fresh? (c/d) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Dd]$ ]]; then
        printf "${YEL}Removing partial checkout...${c0}\n"
        rm -rf "$CHROMIUM_DIR"
        mkdir -p "$CHROMIUM_DIR"
    else
        printf "${YEL}Continuing partial checkout...${c0}\n"
        cd "$CHROMIUM_DIR"
        caffeinate -i gclient sync --nohooks
        cd src
        printf "${YEL}Running gclient hooks...${c0}\n"
        gclient runhooks
        printf "\n${GRE}Chromium source fetched successfully!${c0}\n"
        exit 0
    fi
fi

# Check prerequisites
printf "${YEL}Checking prerequisites...${c0}\n"

if ! command -v git &> /dev/null; then
    die "git is not installed. Please install Xcode Command Line Tools."
fi

if ! command -v python3 &> /dev/null; then
    die "python3 is not installed. Please install it first."
fi

# Check depot_tools
if ! command -v gclient &> /dev/null; then
    if [ ! -d "$HOME/depot_tools" ]; then
        printf "${YEL}depot_tools not found. Installing...${c0}\n"
        cd "$HOME"
        git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
        export PATH="$PATH:$HOME/depot_tools"
        printf "${YEL}Add this to your shell config:${c0}\n"
        printf "export PATH=\"\$PATH:\$HOME/depot_tools\"\n"
    else
        export PATH="$PATH:$HOME/depot_tools"
    fi
fi

# Create chromium directory
printf "${YEL}Creating Chromium directory at $CHROMIUM_DIR...${c0}\n"
mkdir -p "$CHROMIUM_DIR"
cd "$CHROMIUM_DIR"

# Fetch Chromium
printf "${YEL}Fetching Chromium source (this will download ~30GB)...${c0}\n"
printf "${RED}This will take a while. Using caffeinate to prevent sleep.${c0}\n"

caffeinate -i fetch --nohooks chromium

# Check if fetch was successful
if [ ! -d "src" ]; then
    die "Chromium fetch failed!"
fi

cd src

# Install macOS specific build deps (no install-build-deps.sh for mac)
printf "${YEL}Installing build dependencies...${c0}\n"
printf "${YEL}Make sure you have Xcode installed via the App Store${c0}\n"

# Run hooks
printf "${YEL}Running gclient hooks...${c0}\n"
gclient runhooks

# Update .gclient for macOS
cd "$CHROMIUM_DIR"
if ! grep -q "mac" .gclient; then
    printf "${YEL}Updating .gclient for macOS builds...${c0}\n"
    # Backup original
    cp .gclient .gclient.bak
    # Update target_os
    if grep -q "target_os" .gclient; then
        # Update existing target_os
        perl -i -pe 's/target_os\s*=\s*\[.*?\]/target_os = ["mac", "linux", "win"]/s' .gclient
    else
        # Add target_os
        echo 'target_os = ["mac", "linux", "win"]' >> .gclient
    fi
fi

printf "\n${GRE}Chromium source fetched successfully!${c0}\n"
printf "${YEL}Chromium is at: $CHROMIUM_DIR/src${c0}\n"
printf "${YEL}Next steps:${c0}\n"
printf "1. Set CR_DIR environment variable: export CR_DIR=$CHROMIUM_DIR/src\n"
printf "2. Run: ./trunk.sh (from dryft dir)\n"
printf "3. Run: ./version.sh (from dryft dir)\n"
printf "4. Run: ./setup.sh --mac (from dryft dir)\n"
printf "5. Build with: make macos\n"