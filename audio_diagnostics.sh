#!/bin/bash
# Audio Diagnostics Script for Dub Siren V2
# Run this script on the Raspberry Pi to diagnose audio issues

set -e

echo "========================================"
echo "  Dub Siren V2 Audio Diagnostics"
echo "========================================"
echo ""

# Check if running on Raspberry Pi
if [ ! -f /proc/cpuinfo ]; then
    echo "⚠️  WARNING: /proc/cpuinfo not found"
else
    if grep -q "Raspberry Pi" /proc/cpuinfo 2>/dev/null; then
        echo "✓ Running on Raspberry Pi"
        grep "Model" /proc/cpuinfo | head -1
    else
        echo "⚠️  Not running on Raspberry Pi"
    fi
fi
echo ""

# Check I2S configuration
echo "----------------------------------------"
echo "1. Checking I2S Configuration"
echo "----------------------------------------"

CONFIG_FILE=""
if [ -f /boot/firmware/config.txt ]; then
    CONFIG_FILE="/boot/firmware/config.txt"
elif [ -f /boot/config.txt ]; then
    CONFIG_FILE="/boot/config.txt"
fi

if [ -n "$CONFIG_FILE" ]; then
    echo "Config file: $CONFIG_FILE"
    echo ""
    echo "I2S settings:"
    grep -E "dtparam=i2s|dtoverlay=hifiberry|dtparam=audio" "$CONFIG_FILE" || echo "  No I2S settings found!"
else
    echo "⚠️  Config file not found!"
fi
echo ""

# Check loaded kernel modules
echo "----------------------------------------"
echo "2. Checking Sound Kernel Modules"
echo "----------------------------------------"
if command -v lsmod &> /dev/null; then
    echo "Loaded sound modules:"
    lsmod | grep snd || echo "  No sound modules loaded!"
else
    echo "⚠️  lsmod command not available"
fi
echo ""

# Check ALSA devices
echo "----------------------------------------"
echo "3. Checking ALSA Devices"
echo "----------------------------------------"
echo "Output of 'aplay -l':"
aplay -l 2>&1 || echo "  Failed to list devices"
echo ""

# Check ALSA configuration
echo "----------------------------------------"
echo "4. Checking ALSA Configuration"
echo "----------------------------------------"
if [ -f /etc/asound.conf ]; then
    echo "Contents of /etc/asound.conf:"
    cat /etc/asound.conf
else
    echo "⚠️  /etc/asound.conf not found"
fi
echo ""

# Check device tree
echo "----------------------------------------"
echo "5. Checking Device Tree (I2S)"
echo "----------------------------------------"
if [ -d /proc/device-tree ]; then
    echo "I2S related device tree entries:"
    find /proc/device-tree -name "*i2s*" 2>/dev/null || echo "  No I2S device tree entries found"
    echo ""
    echo "Sound related device tree entries:"
    find /proc/device-tree -name "*sound*" 2>/dev/null || echo "  No sound device tree entries found"
else
    echo "⚠️  /proc/device-tree not found"
fi
echo ""

# Check for processes using audio device
echo "----------------------------------------"
echo "6. Checking Audio Device Usage"
echo "----------------------------------------"
if command -v fuser &> /dev/null; then
    echo "Processes using audio devices:"
    fuser -v /dev/snd/* 2>&1 | grep -v "Cannot stat" || echo "  No processes using audio devices"
else
    echo "⚠️  fuser command not available"
fi
echo ""

# Check ALSA mixer settings
echo "----------------------------------------"
echo "7. Checking ALSA Mixer Settings"
echo "----------------------------------------"
if command -v amixer &> /dev/null; then
    echo "Mixer controls:"
    amixer 2>&1 || echo "  Failed to query mixer"
else
    echo "⚠️  amixer command not available"
fi
echo ""

# Test audio output
echo "----------------------------------------"
echo "8. Testing Audio Output"
echo "----------------------------------------"
echo "Attempting to play test tone on hw:0,0..."
echo "(You should hear a tone in your headphones/speakers)"
echo ""

if speaker-test -t wav -c 2 -D hw:0,0 -l 1 2>&1; then
    echo ""
    echo "✓ speaker-test completed"
    echo "  Did you hear audio? (y/n)"
    echo "  If NO audio:"
    echo "    - Check PCM5102 wiring (especially LCK, BCK, DIN)"
    echo "    - Verify PCM5102 config pins (SCK→GND, FLT→GND, FMT→GND)"
    echo "    - Check headphones/speakers are connected to DAC output"
    echo "    - Try different headphones/speakers"
else
    echo ""
    echo "⚠️  speaker-test failed"
fi
echo ""

# Python sounddevice check
echo "----------------------------------------"
echo "9. Checking Python sounddevice Library"
echo "----------------------------------------"
VENV_PYTHON="$HOME/poor-house-dub-v2-venv/bin/python3"
if [ -f "$VENV_PYTHON" ]; then
    echo "Querying devices via Python sounddevice:"
    "$VENV_PYTHON" -c "import sounddevice as sd; print(sd.query_devices())" 2>&1 || echo "  Failed to query devices"
else
    echo "⚠️  Virtual environment Python not found at $VENV_PYTHON"
fi
echo ""

# Summary
echo "========================================"
echo "  Diagnostics Complete"
echo "========================================"
echo ""
echo "Common Issues:"
echo ""
echo "1. NO AUDIO FROM speaker-test:"
echo "   → Check PCM5102 physical wiring:"
echo "     VIN  → Pi Pin 1  (3.3V)"
echo "     GND  → Pi Pin 6  (GND)"
echo "     LCK  → Pi Pin 12 (GPIO 18)"
echo "     BCK  → Pi Pin 35 (GPIO 19)"
echo "     DIN  → Pi Pin 40 (GPIO 21)"
echo "     SCK  → GND (on DAC board)"
echo "     FLT  → GND (on DAC board)"
echo "     FMT  → GND (on DAC board)"
echo ""
echo "2. DEVICE BUSY ERRORS:"
echo "   → Stop dubsiren service:"
echo "     sudo systemctl stop dubsiren.service"
echo ""
echo "3. SOUNDDEVICE RETURNS EMPTY:"
echo "   → This is a known issue on some Pi setups"
echo "   → ALSA should still work directly"
echo ""
echo "4. I2S NOT CONFIGURED:"
echo "   → Re-run setup script: ./setup.sh"
echo "   → Then reboot: sudo reboot"
echo ""
