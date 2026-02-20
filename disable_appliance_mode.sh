#!/bin/bash
# Disable Appliance Mode for Dub Siren
# Makes the filesystem writable again so you can make changes
#
# Use this when you need to:
#   - Update the dub siren software
#   - Change configuration
#   - Install new packages
#
# After making changes, run enable_appliance_mode.sh to re-enable protection

set -e

echo "========================================"
echo "  Dub Siren - Disable Appliance Mode"
echo "========================================"
echo ""

# Check if running as root or with sudo
if [ "$EUID" -ne 0 ]; then
    echo "This script must be run with sudo:"
    echo "  sudo ./disable_appliance_mode.sh"
    exit 1
fi

# Check if running on Raspberry Pi
if [ ! -f /proc/device-tree/model ]; then
    echo "ERROR: This script only works on Raspberry Pi"
    exit 1
fi

# Check if raspi-config is available
if ! command -v raspi-config &> /dev/null; then
    echo "ERROR: raspi-config not found. Is this Raspberry Pi OS?"
    exit 1
fi

# Check if overlay is actually enabled
if ! mount | grep -q "overlay on / "; then
    echo "ℹ️  Appliance mode is not currently enabled."
    echo ""
    echo "The filesystem is already writable."
    echo "You can make changes directly."
    exit 0
fi

echo "This will disable the read-only filesystem protection."
echo ""
echo "After reboot:"
echo "  • Filesystem will be writable"
echo "  • You can update software and configuration"
echo "  • SD card is vulnerable to corruption on power loss"
echo ""
echo "Remember to re-enable appliance mode after making changes:"
echo "  sudo ./enable_appliance_mode.sh"
echo ""

read -p "Disable appliance mode? (y/N): " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cancelled."
    exit 0
fi

echo ""
echo "Disabling overlay filesystem..."
raspi-config nonint do_overlayfs 1

echo "Making boot partition writable..."
raspi-config nonint do_boot_ro 1

echo ""
echo "========================================"
echo "  Appliance Mode Disabled!"
echo "========================================"
echo ""
echo "✓ Overlay filesystem disabled"
echo "✓ Boot partition writable"
echo ""
echo "The system will reboot now to apply changes."
echo ""
echo "After reboot, you can:"
echo "  • Update software: git pull && ./setup.sh"
echo "  • Edit configuration files"
echo "  • Install packages"
echo ""
echo "⚠️  Remember: Re-enable appliance mode when done:"
echo "    sudo ./enable_appliance_mode.sh"
echo ""

read -p "Reboot now? (Y/n): " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    echo "Rebooting..."
    reboot
else
    echo ""
    echo "⚠️  Changes will not take effect until you reboot!"
    echo "   Run 'sudo reboot' when ready."
fi
