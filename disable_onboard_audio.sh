#!/bin/bash
# Disable Raspberry Pi Onboard Audio
# This ensures only the PCM5102 I2S DAC is used

set -e

echo "========================================"
echo "  Disabling Onboard Audio"
echo "========================================"
echo ""

# Detect config file location
if [ -f /boot/firmware/config.txt ]; then
    CONFIG_FILE="/boot/firmware/config.txt"
elif [ -f /boot/config.txt ]; then
    CONFIG_FILE="/boot/config.txt"
else
    echo "❌ Error: Cannot find config.txt"
    exit 1
fi

echo "Using config file: $CONFIG_FILE"
echo ""

# Backup config file
echo "Creating backup: ${CONFIG_FILE}.backup"
sudo cp "$CONFIG_FILE" "${CONFIG_FILE}.backup"

# Remove any existing dtparam=audio lines
echo "Removing existing audio configuration..."
sudo sed -i '/^dtparam=audio=/d' "$CONFIG_FILE"

# Add dtparam=audio=off at the end
echo "Adding dtparam=audio=off..."
echo "dtparam=audio=off" | sudo tee -a "$CONFIG_FILE" > /dev/null

# Verify I2S is enabled
if ! grep -q "^dtparam=i2s=on" "$CONFIG_FILE"; then
    echo "Enabling I2S..."
    echo "dtparam=i2s=on" | sudo tee -a "$CONFIG_FILE" > /dev/null
fi

# Verify hifiberry overlay is present
if ! grep -q "^dtoverlay=hifiberry-dac" "$CONFIG_FILE"; then
    echo "Adding hifiberry-dac overlay..."
    echo "dtoverlay=hifiberry-dac" | sudo tee -a "$CONFIG_FILE" > /dev/null
fi

# Blacklist onboard audio driver
echo "Blacklisting onboard audio driver..."
echo "blacklist snd_bcm2835" | sudo tee /etc/modprobe.d/blacklist-onboard-audio.conf > /dev/null

echo ""
echo "✓ Configuration complete!"
echo ""
echo "Current audio settings:"
grep -E "dtparam=audio|dtparam=i2s|dtoverlay=hifiberry" "$CONFIG_FILE"
echo ""
echo "⚠️  You must REBOOT for changes to take effect:"
echo "    sudo reboot"
echo ""
