#!/bin/bash
# Dub Siren V2 Setup Script
# Wrapper that runs the C++ setup

set -e

# Colors for output
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo ""
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}  Dub Siren V2 Setup${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo ""

# Check if cpp/setup.sh exists
if [ ! -f "$SCRIPT_DIR/cpp/setup.sh" ]; then
    echo "ERROR: cpp/setup.sh not found!"
    echo "Make sure you have the complete repository."
    exit 1
fi

# Make C++ setup executable and run it
chmod +x "$SCRIPT_DIR/cpp/setup.sh"
chmod +x "$SCRIPT_DIR/cpp/build.sh"

# Run the C++ setup
cd "$SCRIPT_DIR/cpp"
./setup.sh

# Return to original directory
cd "$SCRIPT_DIR"

echo ""
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}  Appliance Mode (Recommended)${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo ""
echo "To protect the SD card from corruption when power is unplugged,"
echo "enable appliance mode after everything is working:"
echo ""
echo -e "  ${CYAN}sudo ./enable_appliance_mode.sh${NC}"
echo ""
echo "This makes the filesystem read-only so users can safely"
echo "unplug the power anytime without risk of corruption."
echo ""
