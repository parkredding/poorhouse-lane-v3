#!/bin/bash
# Build script for Dub Siren V2 C++ implementation

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Detect platform
if [[ "$(uname -m)" == "aarch64" ]] || [[ "$(uname -m)" == "armv7l" ]]; then
    IS_PI=true
    echo -e "${GREEN}Detected Raspberry Pi${NC}"
else
    IS_PI=false
    echo -e "${YELLOW}Not running on Raspberry Pi - building for development${NC}"
fi

# Parse arguments
BUILD_TYPE="Release"
CLEAN=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --debug    Build with debug symbols"
            echo "  --clean    Clean build directory before building"
            echo "  --help     Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="$SCRIPT_DIR/build"

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake
echo -e "${GREEN}Configuring CMake...${NC}"

CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if [ "$IS_PI" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DBUILD_FOR_PI=ON"
fi

cmake .. $CMAKE_ARGS

# Build
echo -e "${GREEN}Building...${NC}"

# Use all available cores
if [ "$IS_PI" = true ]; then
    # On Pi, use fewer cores to avoid overheating
    make -j2
else
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi

echo ""
echo -e "${GREEN}Build complete!${NC}"
echo ""
echo "Binary location: $BUILD_DIR/dubsiren"
echo ""
echo "To test:"
echo "  $BUILD_DIR/dubsiren --simulate --interactive"
echo ""
if [ "$IS_PI" = true ]; then
    echo "To run on hardware:"
    echo "  $BUILD_DIR/dubsiren"
fi
