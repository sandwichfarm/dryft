#!/bin/bash
# Tungsten Browser Build Dependencies Installer
# Installs required dependencies for building Tungsten on various platforms

set -e

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Detect OS
OS="unknown"
ARCH=$(uname -m)

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
        OS_VERSION=$VERSION_ID
    fi
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
    OS_VERSION=$(sw_vers -productVersion)
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
    OS="windows"
fi

# Parse command line arguments
NO_PROMPT=false
INSTALL_ANDROID=false
INSTALL_ARM=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-prompt)
            NO_PROMPT=true
            shift
            ;;
        --android)
            INSTALL_ANDROID=true
            shift
            ;;
        --arm)
            INSTALL_ARM=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --no-prompt    Don't prompt for confirmation"
            echo "  --android      Install Android build dependencies"
            echo "  --arm          Install ARM cross-compilation tools"
            echo "  -h, --help     Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Function to check if running with sudo
check_sudo() {
    if [ "$EUID" -ne 0 ] && [ "$OS" != "macos" ]; then
        echo -e "${RED}This script needs to be run with sudo on Linux${NC}"
        exit 1
    fi
}

# Function to prompt for confirmation
confirm() {
    if [ "$NO_PROMPT" = false ]; then
        read -p "$1 [Y/n] " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]] && [ -n "$REPLY" ]; then
            return 1
        fi
    fi
    return 0
}

# Function to install Linux dependencies
install_linux_deps() {
    echo -e "${GREEN}Installing Linux build dependencies...${NC}"
    
    # Common packages for all Linux distros
    COMMON_PACKAGES="
        build-essential
        curl
        git
        cmake
        ninja-build
        python3
        python3-pip
        pkg-config
        libgtk-3-dev
        libnotify-dev
        libnss3-dev
        libxss1
        libasound2-dev
        libxtst6
        xvfb
        zip
        unzip
        locales-all
        libatspi2.0-0
        libdrm-dev
        libxcomposite-dev
        libxdamage-dev
        libxrandr-dev
        libgbm-dev
        libxkbcommon-dev
        libpci-dev
        libffi-dev
        nodejs
        npm
    "
    
    # Distro-specific installation
    case $OS in
        ubuntu|debian)
            apt-get update
            apt-get install -y $COMMON_PACKAGES
            
            # Additional packages for Ubuntu/Debian
            # Note: libjxl-dev is not available in Ubuntu 22.04, will be built from source if needed
            apt-get install -y \
                libssl-dev \
                libxshmfence-dev \
                libglu1-mesa-dev \
                libxi-dev \
                libgconf-2-4 \
                libpulse-dev \
                libcups2-dev \
                libjpeg-dev \
                libwebp-dev \
                libpng-dev \
                libfreetype6-dev \
                libfontconfig1-dev \
                libharfbuzz-dev \
                libexpat1-dev \
                libflac-dev \
                libopus-dev \
                libmodplug-dev \
                libvpx-dev \
                libspeechd-dev \
                libavutil-dev \
                libavcodec-dev \
                libavformat-dev \
                libva-dev \
                libpipewire-0.3-dev \
                libre2-dev \
                libsnappy-dev \
                libjsoncpp-dev \
                libhwy-dev \
                libusb-1.0-0-dev \
                libopenjp2-7-dev \
                libevent-dev
            ;;
            
        fedora|rhel|centos)
            dnf install -y $COMMON_PACKAGES
            
            # Additional packages for Fedora/RHEL
            dnf install -y \
                libstdc++-static \
                libatomic \
                openssl-devel \
                mesa-libGL-devel \
                libXi-devel \
                pulseaudio-libs-devel \
                cups-devel \
                libjpeg-turbo-devel \
                libwebp-devel \
                libpng-devel \
                freetype-devel \
                fontconfig-devel \
                harfbuzz-devel \
                expat-devel \
                flac-devel \
                opus-devel \
                libmodplug-devel \
                libvpx-devel \
                speech-dispatcher-devel \
                ffmpeg-devel \
                libva-devel \
                pipewire-devel \
                re2-devel \
                snappy-devel \
                jsoncpp-devel \
                highway-devel \
                libjxl-devel \
                libusb1-devel \
                openjpeg2-devel \
                libevent-devel
            ;;
            
        arch)
            pacman -Syu --noconfirm
            pacman -S --noconfirm \
                base-devel \
                git \
                cmake \
                ninja \
                python \
                python-pip \
                pkgconf \
                gtk3 \
                libnotify \
                nss \
                alsa-lib \
                xorg-server-xvfb \
                zip \
                unzip \
                nodejs \
                npm \
                libva \
                libpipewire \
                re2 \
                snappy \
                jsoncpp \
                highway \
                libjxl \
                libusb \
                openjpeg2 \
                libevent
            ;;
            
        *)
            echo -e "${YELLOW}Unsupported Linux distribution: $OS${NC}"
            echo "Please install build dependencies manually"
            exit 1
            ;;
    esac
    
    # Install clang and lld
    echo -e "${GREEN}Installing clang and lld...${NC}"
    case $OS in
        ubuntu|debian)
            apt-get install -y clang lld
            ;;
        fedora|rhel|centos)
            dnf install -y clang lld
            ;;
        arch)
            pacman -S --noconfirm clang lld
            ;;
    esac
}

# Function to install macOS dependencies
install_macos_deps() {
    echo -e "${GREEN}Installing macOS build dependencies...${NC}"
    
    # Check if Homebrew is installed
    if ! command -v brew &> /dev/null; then
        echo -e "${YELLOW}Homebrew not found. Installing Homebrew...${NC}"
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    
    # Install required packages
    brew install \
        ninja \
        node \
        pkg-config
    
    # Check for Xcode
    if ! xcode-select -p &> /dev/null; then
        echo -e "${RED}Xcode not found. Please install Xcode from the App Store${NC}"
        echo "After installing Xcode, run: xcode-select --install"
        exit 1
    fi
    
    # Check for macOS SDK
    echo -e "${GREEN}Checking macOS SDK...${NC}"
    xcrun --show-sdk-path
}

# Function to install Android dependencies
install_android_deps() {
    echo -e "${GREEN}Installing Android build dependencies...${NC}"
    
    # Install Java
    case $OS in
        ubuntu|debian)
            apt-get install -y openjdk-11-jdk
            ;;
        fedora|rhel|centos)
            dnf install -y java-11-openjdk-devel
            ;;
        arch)
            pacman -S --noconfirm jdk11-openjdk
            ;;
        macos)
            brew install openjdk@11
            ;;
    esac
    
    # Set JAVA_HOME
    if [ "$OS" = "macos" ]; then
        export JAVA_HOME=$(/usr/libexec/java_home -v 11)
    else
        export JAVA_HOME=$(dirname $(dirname $(readlink -f $(which java))))
    fi
    
    echo -e "${YELLOW}Android SDK should be installed separately${NC}"
    echo "Please download and install Android Studio or the command line tools"
    echo "Set ANDROID_HOME environment variable to the SDK location"
}

# Function to install ARM cross-compilation tools
install_arm_deps() {
    echo -e "${GREEN}Installing ARM cross-compilation tools...${NC}"
    
    case $OS in
        ubuntu|debian)
            apt-get install -y \
                gcc-aarch64-linux-gnu \
                g++-aarch64-linux-gnu \
                gcc-arm-linux-gnueabihf \
                g++-arm-linux-gnueabihf
            ;;
        fedora|rhel|centos)
            dnf install -y \
                gcc-aarch64-linux-gnu \
                gcc-c++-aarch64-linux-gnu \
                gcc-arm-linux-gnu \
                gcc-c++-arm-linux-gnu
            ;;
        *)
            echo -e "${YELLOW}ARM cross-compilation tools not available for $OS${NC}"
            ;;
    esac
}

# Function to install Python packages
install_python_deps() {
    echo -e "${GREEN}Installing Python dependencies...${NC}"
    
    # Ensure pip is up to date
    python3 -m pip install --upgrade pip
    
    # Install required Python packages
    python3 -m pip install \
        psutil \
        httplib2 \
        pyparsing \
        six
}

# Function to setup depot_tools
setup_depot_tools() {
    echo -e "${GREEN}Setting up depot_tools...${NC}"
    
    DEPOT_TOOLS_PATH="$HOME/depot_tools"
    
    if [ ! -d "$DEPOT_TOOLS_PATH" ]; then
        echo "Cloning depot_tools..."
        git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS_PATH"
    else
        echo "depot_tools already exists at $DEPOT_TOOLS_PATH"
    fi
    
    echo -e "${YELLOW}Add the following line to your shell configuration:${NC}"
    echo "export PATH=\"\$PATH:$DEPOT_TOOLS_PATH\""
}

# Function to install build performance tools
install_perf_tools() {
    echo -e "${GREEN}Installing build performance tools...${NC}"
    
    case $OS in
        ubuntu|debian)
            apt-get install -y ccache
            ;;
        fedora|rhel|centos)
            dnf install -y ccache
            ;;
        arch)
            pacman -S --noconfirm ccache
            ;;
        macos)
            brew install ccache sccache
            ;;
    esac
    
    # Configure ccache
    if command -v ccache &> /dev/null; then
        ccache --max-size=10G
        ccache --set-config=compression=true
        ccache --set-config=compression_level=6
        echo -e "${GREEN}ccache configured with 10GB cache${NC}"
    fi
}

# Main installation flow
main() {
    echo -e "${GREEN}Tungsten Browser Build Dependencies Installer${NC}"
    echo -e "${GREEN}OS: $OS, Architecture: $ARCH${NC}"
    echo
    
    if [ "$OS" = "unknown" ]; then
        echo -e "${RED}Unable to detect operating system${NC}"
        exit 1
    fi
    
    # Confirm installation
    if ! confirm "This will install build dependencies for Tungsten. Continue?"; then
        echo "Installation cancelled"
        exit 0
    fi
    
    # Check sudo for Linux
    if [ "$OS" != "macos" ] && [ "$OS" != "windows" ]; then
        check_sudo
    fi
    
    # Install base dependencies
    case $OS in
        ubuntu|debian|fedora|rhel|centos|arch)
            install_linux_deps
            ;;
        macos)
            install_macos_deps
            ;;
        windows)
            echo -e "${YELLOW}Windows dependencies should be installed manually${NC}"
            echo "Please install:"
            echo "  - Visual Studio 2022 with C++ workload"
            echo "  - Windows 10 SDK"
            echo "  - Git for Windows"
            echo "  - Python 3.x"
            exit 0
            ;;
    esac
    
    # Install optional dependencies
    if [ "$INSTALL_ANDROID" = true ]; then
        install_android_deps
    fi
    
    if [ "$INSTALL_ARM" = true ]; then
        install_arm_deps
    fi
    
    # Install Python dependencies
    install_python_deps
    
    # Install performance tools
    install_perf_tools
    
    # Setup depot_tools
    setup_depot_tools
    
    echo
    echo -e "${GREEN}Build dependencies installation completed!${NC}"
    echo
    echo "Next steps:"
    echo "1. Add depot_tools to your PATH"
    echo "2. Run 'gclient' to verify depot_tools is working"
    echo "3. Run './build.py --help' to see build options"
    
    if [ "$INSTALL_ANDROID" = true ]; then
        echo
        echo -e "${YELLOW}Android SDK still needs to be installed manually${NC}"
        echo "Download from: https://developer.android.com/studio"
    fi
}

# Run main function
main