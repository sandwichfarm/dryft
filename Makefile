# Tungsten Browser Makefile
# Simplifies common build operations

# Load configuration from .build.conf if it exists
-include .build.conf

# Default configuration
PLATFORM ?= auto
ARCH ?= auto
BUILD_TYPE ?= release
JOBS ?= auto

# Directories
DEPOT_TOOLS_DIR = $(HOME)/depot_tools
BUILD_SCRIPT = ./build.py
OUT_DIR = src/out

# Color output
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[0;33m
NC = \033[0m # No Color

# Default target
.DEFAULT_GOAL := help

# Phony targets
.PHONY: help setup deps build release debug clean test run package cache-setup \
        cache-stats linux windows macos android raspi all-platforms check-deps \
        fetch-chromium sync

## Help
help: ## Show this help message
	@echo "Tungsten Browser Build System"
	@echo "============================="
	@echo ""
	@echo "Usage: make [target] [VARIABLE=value]"
	@echo ""
	@echo "Variables:"
	@echo "  PLATFORM=$(PLATFORM)    Platform to build for (auto|linux|windows|macos|android|raspi)"
	@echo "  ARCH=$(ARCH)           Architecture (auto|x64|arm64|arm32)"
	@echo "  BUILD_TYPE=$(BUILD_TYPE) Build type (release|debug)"
	@echo "  JOBS=$(JOBS)           Number of parallel jobs (auto|number)"
	@echo ""
	@echo "Common targets:"
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "  $(GREEN)%-20s$(NC) %s\n", $$1, $$2}'

## Setup
setup: check-deps deps cache-setup ## Complete setup for new developers
	@echo "$(GREEN)Setup complete! You can now run 'make build'$(NC)"

check-deps: ## Check if required dependencies are installed
	@echo "Checking dependencies..."
	@command -v python3 >/dev/null 2>&1 || { echo "$(RED)Python 3 is required but not installed.$(NC)" >&2; exit 1; }
	@command -v git >/dev/null 2>&1 || { echo "$(RED)Git is required but not installed.$(NC)" >&2; exit 1; }
	@if [ ! -d "$(DEPOT_TOOLS_DIR)" ]; then \
		echo "$(YELLOW)depot_tools not found. Run 'make install-depot-tools' to install.$(NC)"; \
	fi

install-depot-tools: ## Install depot_tools
	@echo "Installing depot_tools..."
	@if [ ! -d "$(DEPOT_TOOLS_DIR)" ]; then \
		git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git $(DEPOT_TOOLS_DIR); \
		echo "$(GREEN)depot_tools installed to $(DEPOT_TOOLS_DIR)$(NC)"; \
		echo "$(YELLOW)Add this to your shell configuration:$(NC)"; \
		echo "export PATH=\"\$$PATH:$(DEPOT_TOOLS_DIR)\""; \
	else \
		echo "$(YELLOW)depot_tools already installed at $(DEPOT_TOOLS_DIR)$(NC)"; \
	fi

deps: ## Install build dependencies
	@echo "Installing build dependencies..."
	@if [ "$$(uname)" = "Linux" ]; then \
		sudo ./scripts/install-build-deps.sh --no-prompt; \
	elif [ "$$(uname)" = "Darwin" ]; then \
		./scripts/install-build-deps.sh --no-prompt; \
	else \
		echo "$(YELLOW)Please install dependencies manually for your platform$(NC)"; \
	fi

cache-setup: ## Set up build caching (ccache/sccache)
	@echo "Setting up build cache..."
	@./scripts/setup-build-cache.sh setup

cache-stats: ## Show cache statistics
	@./scripts/setup-build-cache.sh stats

cache-clean: ## Clean build cache
	@./scripts/setup-build-cache.sh clean

## Building
build: ## Build Tungsten (uses BUILD_TYPE variable)
	@echo "Building Tungsten ($(BUILD_TYPE))..."
	$(BUILD_SCRIPT) --$(BUILD_TYPE) \
		$(if $(filter-out auto,$(PLATFORM)),--platform=$(PLATFORM)) \
		$(if $(filter-out auto,$(ARCH)),--arch=$(ARCH)) \
		$(if $(filter-out auto,$(JOBS)),--jobs=$(JOBS))

release: ## Build release version
	@$(MAKE) build BUILD_TYPE=release

debug: ## Build debug version
	@$(MAKE) build BUILD_TYPE=debug

quick: ## Quick debug build (component build)
	@echo "Building debug with component build..."
	$(BUILD_SCRIPT) --debug --component-build \
		$(if $(filter-out auto,$(PLATFORM)),--platform=$(PLATFORM)) \
		$(if $(filter-out auto,$(ARCH)),--arch=$(ARCH))

# Platform-specific targets
linux: ## Build for Linux
	@$(MAKE) build PLATFORM=linux ARCH=x64

windows: ## Build for Windows
	@$(MAKE) build PLATFORM=windows ARCH=x64

macos: ## Build for macOS (auto-detect arch)
	@$(MAKE) build PLATFORM=macos

android: ## Build for Android
	@$(MAKE) build PLATFORM=android ARCH=arm64

raspi: ## Build for Raspberry Pi
	@$(MAKE) build PLATFORM=raspi

all-platforms: ## Build for all platforms (CI only)
	@echo "$(YELLOW)Building for all platforms - this will take a very long time!$(NC)"
	@$(MAKE) linux
	@$(MAKE) windows
	@$(MAKE) macos
	@$(MAKE) android
	@$(MAKE) raspi

## Chromium Source
fetch-chromium: ## Fetch Chromium source (requires depot_tools)
	@echo "$(YELLOW)Fetching Chromium source - this will download ~30GB!$(NC)"
	@echo "$(RED)This is not yet implemented in the Makefile.$(NC)"
	@echo "Run the GitHub Actions workflow 'Cache Chromium Source' instead."

sync: ## Sync Chromium source
	@if [ -d "chromium/src" ]; then \
		cd chromium && gclient sync; \
	else \
		echo "$(RED)Chromium source not found. Run 'make fetch-chromium' first.$(NC)"; \
	fi

## Testing & Running
test: ## Run tests
	@echo "Running tests..."
	@python3 test_build_system.py

run: ## Run Tungsten browser
	@if [ -f "$(OUT_DIR)/Release-$$(uname | tr '[:upper:]' '[:lower:]')-$$(uname -m)/chrome" ]; then \
		$(OUT_DIR)/Release-$$(uname | tr '[:upper:]' '[:lower:]')-$$(uname -m)/chrome; \
	elif [ -f "$(OUT_DIR)/Debug-$$(uname | tr '[:upper:]' '[:lower:]')-$$(uname -m)/chrome" ]; then \
		$(OUT_DIR)/Debug-$$(uname | tr '[:upper:]' '[:lower:]')-$$(uname -m)/chrome; \
	else \
		echo "$(RED)No build found. Run 'make build' first.$(NC)"; \
	fi

run-debug: ## Run Tungsten with debug logging
	@LOG_LEVEL=0 $(MAKE) run

## Packaging
package: ## Package the current build
	$(BUILD_SCRIPT) --package \
		$(if $(filter-out auto,$(PLATFORM)),--platform=$(PLATFORM)) \
		$(if $(filter-out auto,$(ARCH)),--arch=$(ARCH))

## Cleaning
clean: ## Clean build directory
	@echo "Cleaning build directories..."
	@rm -rf $(OUT_DIR)/*
	@echo "$(GREEN)Build directories cleaned$(NC)"

clean-all: clean cache-clean ## Clean everything (builds + cache)
	@echo "$(GREEN)All cleaned$(NC)"

## Development Workflow Helpers
dev: ## Set up for development (install deps + quick build)
	@$(MAKE) deps
	@$(MAKE) cache-setup
	@$(MAKE) quick
	@echo "$(GREEN)Ready for development! Run 'make run' to test.$(NC)"

rebuild: clean build ## Clean and rebuild

refresh: ## Pull latest changes and rebuild
	@git pull
	@$(MAKE) deps
	@$(MAKE) build

## CI/CD Helpers
ci-cache-chromium: ## Trigger GitHub Action to cache Chromium source
	@echo "Triggering Chromium cache workflow..."
	@gh workflow run cache-chromium-source.yml

ci-build: ## Trigger GitHub Action to build
	@echo "Triggering build workflow..."
	@gh workflow run build-all.yml

ci-status: ## Check CI build status
	@gh run list --workflow=build-all.yml --limit=5

## Advanced Options
custom: ## Build with custom GN args (use GN_ARGS variable)
	$(BUILD_SCRIPT) --$(BUILD_TYPE) \
		$(if $(filter-out auto,$(PLATFORM)),--platform=$(PLATFORM)) \
		$(if $(filter-out auto,$(ARCH)),--arch=$(ARCH)) \
		$(foreach arg,$(GN_ARGS),--gn-args "$(arg)")

pgo: ## Build with Profile-Guided Optimization
	$(BUILD_SCRIPT) --release --pgo \
		$(if $(filter-out auto,$(PLATFORM)),--platform=$(PLATFORM)) \
		$(if $(filter-out auto,$(ARCH)),--arch=$(ARCH))

lto: ## Build with Link-Time Optimization
	$(BUILD_SCRIPT) --release --lto \
		$(if $(filter-out auto,$(PLATFORM)),--platform=$(PLATFORM)) \
		$(if $(filter-out auto,$(ARCH)),--arch=$(ARCH))

# Examples section
examples: ## Show example commands
	@echo "Example Commands:"
	@echo "================="
	@echo ""
	@echo "# First-time setup:"
	@echo "  make setup"
	@echo ""
	@echo "# Build release version:"
	@echo "  make release"
	@echo ""
	@echo "# Quick debug build:"
	@echo "  make quick"
	@echo ""
	@echo "# Build for specific platform:"
	@echo "  make linux"
	@echo "  make PLATFORM=windows ARCH=x64 build"
	@echo ""
	@echo "# Build with custom options:"
	@echo "  make BUILD_TYPE=debug JOBS=16 build"
	@echo "  make GN_ARGS='use_vaapi=true use_pulseaudio=true' custom"
	@echo ""
	@echo "# Run the browser:"
	@echo "  make run"
	@echo ""
	@echo "# Clean and rebuild:"
	@echo "  make rebuild"