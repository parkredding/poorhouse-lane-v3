#!/bin/bash
# Enable Appliance Mode for Dub Siren
# Makes the filesystem read-only to prevent SD card corruption from power loss
#
# This uses OverlayFS to keep the root filesystem read-only while allowing
# temporary writes to RAM. All changes are lost on reboot, but the system
# is protected from corruption when users unplug the power without shutdown.

set -e

echo "========================================"
echo "  Dub Siren - Appliance Mode Setup"
echo "========================================"
echo ""

# Check if running as root or with sudo
if [ "$EUID" -ne 0 ]; then
    echo "This script must be run with sudo:"
    echo "  sudo ./enable_appliance_mode.sh"
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

# Check if already enabled
if mount | grep -q "overlay on / "; then
    echo "⚠️  Appliance mode is already enabled!"
    echo ""
    echo "The filesystem is currently read-only."
    echo "To make changes, first run: sudo ./disable_appliance_mode.sh"
    exit 0
fi

echo "This will enable read-only filesystem protection."
echo ""
echo "Benefits:"
echo "  ✓ SD card protected from corruption on power loss"
echo "  ✓ Users can safely unplug power anytime"
echo "  ✓ System always boots to known good state"
echo ""
echo "Important:"
echo "  • All changes after reboot will be lost (writes go to RAM)"
echo "  • To update software, disable appliance mode first"
echo "  • The dub siren should be fully configured before enabling"
echo ""

read -p "Enable appliance mode? (y/N): " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cancelled."
    exit 0
fi

echo ""
echo "Disabling swap (not needed with read-only filesystem)..."
dphys-swapfile swapoff 2>/dev/null || true
dphys-swapfile uninstall 2>/dev/null || true
systemctl disable dphys-swapfile 2>/dev/null || true

echo "Enabling overlay filesystem..."
raspi-config nonint do_overlayfs 0

echo "Write-protecting boot partition..."
raspi-config nonint do_boot_ro 0

echo ""
echo "========================================"
echo "  Appliance Mode Enabled!"
echo "========================================"
echo ""
echo "✓ Overlay filesystem configured"
echo "✓ Boot partition write-protected"
echo "✓ Swap disabled"
echo ""
echo "The system will reboot now to activate appliance mode."
echo ""
echo "After reboot:"
echo "  • SD card is protected from power loss corruption"
echo "  • Users can safely unplug power anytime"
echo "  • All changes are temporary (lost on reboot)"
echo ""
echo "To make changes later, run:"
echo "  sudo ./disable_appliance_mode.sh"
echo ""

read -p "Reboot now? (Y/n): " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    echo "Rebooting..."
    reboot
else
    echo ""
    echo "⚠️  Appliance mode will not be active until you reboot!"
    echo "   Run 'sudo reboot' when ready."
fi
