#!/bin/bash
# KPM Build Script for macOS/Linux
# Copyright (c) 2024

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Load .env file if exists
if [ -f ".env" ]; then
    echo -e "${BLUE}Loading configuration from .env...${NC}"
    export $(grep -v '^#' .env | grep -v '^$' | xargs)
fi

# Print banner
echo -e "${GREEN}╔════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   KPM Build System (CMake)             ║${NC}"
echo -e "${GREEN}║   Build Anywhere - Any Platform        ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════╝${NC}"
echo ""

# Parse command line arguments
CLEAN=false
VERBOSE=""
BUILD_JOBS=""

while [[ $# -gt 0 ]]; do
    case $1 in
        clean)
            CLEAN=true
            shift
            ;;
        -v|--verbose)
            VERBOSE="--verbose"
            shift
            ;;
        -j*)
            BUILD_JOBS="$1"
            shift
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Usage: $0 [clean] [-v|--verbose] [-j<N>]"
            exit 1
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}🧹 Cleaning build directory...${NC}"
    rm -rf build
    rm -rf third_party
    echo -e "${GREEN}✅ Clean complete${NC}"
    exit 0
fi

# Show configuration
echo -e "${BLUE}Configuration:${NC}"
if [ -n "$NDK_PATH" ]; then
    echo -e "  ${GREEN}✓${NC} NDK Path: ${NDK_PATH}"
else
    echo -e "  ${YELLOW}⚡${NC} NDK Path: Auto-detect"
fi

if [ -n "$KP_DIR" ]; then
    echo -e "  ${GREEN}✓${NC} KernelPatch: ${KP_DIR}"
else
    echo -e "  ${YELLOW}⚡${NC} KernelPatch: Auto-download to third_party/"
fi

# Detect build jobs
if [ -z "$BUILD_JOBS" ]; then
    if [ -n "$BUILD_JOBS" ]; then
        BUILD_JOBS="-j$BUILD_JOBS"
    elif command -v nproc &> /dev/null; then
        BUILD_JOBS="-j$(nproc)"
    elif command -v sysctl &> /dev/null; then
        BUILD_JOBS="-j$(sysctl -n hw.ncpu)"
    else
        BUILD_JOBS="-j2"
    fi
fi

echo -e "  ${GREEN}✓${NC} Parallel jobs: ${BUILD_JOBS}"
echo ""

# Configure CMake
echo -e "${YELLOW}⚙️  Configuring CMake...${NC}"
CMAKE_ARGS=""
if [ -n "$KP_DIR" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DKP_DIR=$KP_DIR"
fi
if [ -n "$KP_ZIP_URL" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DKP_ZIP_URL=$KP_ZIP_URL"
fi

cmake -B build $CMAKE_ARGS

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ CMake configuration failed${NC}"
    exit 1
fi

# Build
echo ""
echo -e "${YELLOW}🔨 Building...${NC}"
cmake --build build $BUILD_JOBS $VERBOSE

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Build failed${NC}"
    exit 1
fi

# Check if build was successful
KPM_FILES=$(find build -name "*.kpm" 2>/dev/null)
if [ -n "$KPM_FILES" ]; then
    echo ""
    echo -e "${GREEN}╔════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║  ✅ Build Successful!                  ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${BLUE}Output files:${NC}"
    find build -name "*.kpm" -exec ls -lh {} \;
    echo ""
    echo -e "${GREEN}📦 KPM modules ready:${NC}"
    find build -name "*.kpm" | while read kpm; do
        echo -e "  ${GREEN}✓${NC} $kpm"
    done
else
    echo -e "${RED}❌ Build failed - no output files found${NC}"
    exit 1
fi

