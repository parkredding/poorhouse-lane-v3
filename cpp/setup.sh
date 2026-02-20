#!/bin/bash
# Setup script for Dub Siren V2 - C++ Edition
# Run this on your Raspberry Pi Zero 2W

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo ""
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}  Dub Siren V2 - C++ Edition Setup${NC}"
echo -e "${CYAN}  Raspberry Pi Zero 2W + PCM5102 DAC${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo ""

# Detect the actual user (not root if running via sudo)
if [ -n "$SUDO_USER" ]; then
    INSTALL_USER="$SUDO_USER"
    USER_HOME=$(eval echo ~$SUDO_USER)
else
    INSTALL_USER=$(whoami)
    USER_HOME="$HOME"
fi

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CPP_DIR="$SCRIPT_DIR"
PROJECT_DIR="$(dirname "$CPP_DIR")"

echo -e "Installing for user: ${CYAN}$INSTALL_USER${NC}"
echo -e "Home directory: ${CYAN}$USER_HOME${NC}"
echo -e "C++ source directory: ${CYAN}$CPP_DIR${NC}"
echo ""

# Check if running on Raspberry Pi
IS_PI=false
if [ -f /proc/device-tree/model ]; then
    if grep -q "Raspberry Pi" /proc/device-tree/model 2>/dev/null; then
        IS_PI=true
        PI_MODEL=$(cat /proc/device-tree/model | tr -d '\0')
        echo -e "${GREEN}✓ Detected: $PI_MODEL${NC}"
    fi
fi

if [ "$IS_PI" = false ]; then
    echo -e "${YELLOW}⚠️  WARNING: Not running on Raspberry Pi${NC}"
    echo "   GPIO and I2S features will be simulated"
fi
echo ""

# ============================================================================
# Ensure Swap Space (Pi Zero needs this for compilation)
# ============================================================================
if [ "$IS_PI" = true ]; then
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  Checking Swap Space${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
    
    # Check current swap
    SWAP_TOTAL=$(free -m | awk '/^Swap:/ {print $2}')
    
    if [ "$SWAP_TOTAL" -lt 512 ]; then
        echo -e "${YELLOW}⚠️  Insufficient swap space (${SWAP_TOTAL}MB). Pi Zero needs swap for compilation.${NC}"
        
        # Check if we're in appliance mode (overlay filesystem)
        if mount | grep -q "overlay on / "; then
            echo -e "${RED}✗ Cannot create swap: System is in read-only appliance mode${NC}"
            echo -e "${YELLOW}  Run 'sudo raspi-config' and disable overlay filesystem, then reboot${NC}"
            echo -e "${YELLOW}  Or run: sudo raspi-config nonint do_overlayfs 0 && sudo reboot${NC}"
            exit 1
        fi
        
        # Check if swapfile already exists but isn't active
        if [ -f /swapfile ]; then
            echo -e "${GREEN}📦 Activating existing swap file...${NC}"
            sudo swapon /swapfile 2>/dev/null || true
        else
            echo -e "${GREEN}📦 Creating 1GB swap file (required for compilation)...${NC}"
            
            # Check available disk space
            AVAIL_SPACE=$(df -m / | awk 'NR==2 {print $4}')
            if [ "$AVAIL_SPACE" -lt 1500 ]; then
                echo -e "${RED}✗ Not enough disk space for swap file (need 1.5GB, have ${AVAIL_SPACE}MB)${NC}"
                exit 1
            fi
            
            sudo fallocate -l 1G /swapfile
            sudo chmod 600 /swapfile
            sudo mkswap /swapfile
            sudo swapon /swapfile
            
            # Make swap permanent
            if ! grep -q "/swapfile" /etc/fstab; then
                echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab > /dev/null
            fi
        fi
        
        # Verify swap is now active
        SWAP_TOTAL=$(free -m | awk '/^Swap:/ {print $2}')
        echo -e "${GREEN}✓ Swap space: ${SWAP_TOTAL}MB${NC}"
    else
        echo -e "${GREEN}✓ Swap space available: ${SWAP_TOTAL}MB${NC}"
    fi
    echo ""
fi

# ============================================================================
# Install Build Dependencies
# ============================================================================
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}  Installing Build Dependencies${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

echo -e "${GREEN}📦 Updating system packages...${NC}"
sudo apt-get update -qq

echo -e "${GREEN}📦 Installing build tools and libraries...${NC}"
sudo apt-get install -y \
    build-essential \
    cmake \
    libasound2-dev \
    git \
    wget \
    unzip

# Install GPIO library for Pi
if [ "$IS_PI" = true ]; then
    echo -e "${GREEN}📦 Installing GPIO library (libgpiod)...${NC}"
    
    # Install libgpiod - the modern Linux GPIO interface
    # This works on all Raspberry Pi OS versions including newer Debian Trixie
    sudo apt-get install -y libgpiod-dev gpiod
    
    echo -e "${GREEN}✓ libgpiod installed${NC}"
    
    # Install WS2812 LED library (optional - for status LED)
    echo -e "${GREEN}📦 Installing WS2812 LED library (optional)...${NC}"
    
    # Try to install from package manager first
    if apt-cache show libws2811-dev &>/dev/null; then
        sudo apt-get install -y libws2811-dev
        echo -e "${GREEN}✓ ws2811 library installed${NC}"
    else
        # Build from source if not available as package
        echo -e "${YELLOW}  ws2811 not in package manager, building from source...${NC}"
        
        TEMP_DIR=$(mktemp -d)
        cd "$TEMP_DIR"
        
        git clone https://github.com/jgarff/rpi_ws281x.git
        cd rpi_ws281x
        
        mkdir build && cd build
        cmake -DBUILD_SHARED=ON -DBUILD_TEST=OFF ..
        make
        sudo make install
        sudo ldconfig
        
        cd "$SCRIPT_DIR"
        rm -rf "$TEMP_DIR"
        
        echo -e "${GREEN}✓ ws2811 library built and installed${NC}"
    fi
fi

echo -e "${GREEN}✓ Dependencies installed${NC}"
echo ""

# ============================================================================
# Configure I2S for PCM5102
# ============================================================================
if [ "$IS_PI" = true ]; then
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  I2S Audio Configuration${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""

    # Detect correct config.txt location (newer OS uses /boot/firmware/config.txt)
    if [ -f /boot/firmware/config.txt ]; then
        CONFIG_FILE="/boot/firmware/config.txt"
    else
        CONFIG_FILE="/boot/config.txt"
    fi

    echo -e "Using config file: ${CYAN}$CONFIG_FILE${NC}"

    # Backup config if not already backed up
    if [ ! -f "$CONFIG_FILE.backup.dubsiren" ]; then
        echo "Creating backup of config.txt..."
        sudo cp "$CONFIG_FILE" "$CONFIG_FILE.backup.dubsiren"
    fi

    # Check if I2S already configured
    I2S_CONFIGURED=false
    if grep -q "dtoverlay=hifiberry-dac" "$CONFIG_FILE" && grep -q "dtparam=i2s=on" "$CONFIG_FILE"; then
        I2S_CONFIGURED=true
        echo -e "${GREEN}✓ I2S already configured${NC}"
    fi

    if [ "$I2S_CONFIGURED" = false ]; then
        echo ""
        echo "The PCM5102 DAC requires I2S audio to be enabled."
        echo "This will add the following to $CONFIG_FILE:"
        echo "  - dtparam=i2s=on"
        echo "  - dtoverlay=hifiberry-dac"
        echo ""
        read -p "Configure I2S for PCM5102? (Y/n): " -n 1 -r </dev/tty
        echo ""
        
        if [[ ! $REPLY =~ ^[Nn]$ ]]; then
            echo "Adding I2S configuration..."
            
            # Add I2S configuration
            if ! grep -q "dtparam=i2s=on" "$CONFIG_FILE"; then
                echo "" | sudo tee -a "$CONFIG_FILE" >/dev/null
                echo "# I2S Audio for PCM5102 DAC (Dub Siren V2)" | sudo tee -a "$CONFIG_FILE" >/dev/null
                echo "dtparam=i2s=on" | sudo tee -a "$CONFIG_FILE" >/dev/null
            fi
            
            if ! grep -q "dtoverlay=hifiberry-dac" "$CONFIG_FILE"; then
                echo "dtoverlay=hifiberry-dac" | sudo tee -a "$CONFIG_FILE" >/dev/null
            fi
            
            # Disable onboard audio
            if grep -q "dtparam=audio=on" "$CONFIG_FILE"; then
                echo "Disabling onboard audio..."
                sudo sed -i 's/dtparam=audio=on/dtparam=audio=off/' "$CONFIG_FILE"
            fi
            
            echo -e "${GREEN}✓ I2S configured${NC}"
            NEEDS_REBOOT=true
        fi
    fi
    echo ""
fi

# ============================================================================
# Handle Required Reboot
# ============================================================================
if [ "${NEEDS_REBOOT:-false}" = true ]; then
    echo -e "${YELLOW}════════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}  ⚠️  REBOOT REQUIRED${NC}"
    echo -e "${YELLOW}════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "I2S audio has been configured for the PCM5102 DAC."
    echo "A reboot is required before the audio device will be available."
    echo ""
    echo -e "After reboot, run the installer again to complete setup:"
    echo -e "  ${CYAN}cd ~/poor-house-dub-v2/cpp && bash setup.sh${NC}"
    echo ""
    read -p "Reboot now? (Y/n): " REBOOT_CHOICE </dev/tty
    REBOOT_CHOICE=${REBOOT_CHOICE:-Y}
    
    if [[ "$REBOOT_CHOICE" =~ ^[Yy] ]]; then
        echo ""
        echo "Rebooting in 3 seconds..."
        sleep 3
        sudo reboot
    else
        echo ""
        echo -e "Please reboot manually with: ${CYAN}sudo reboot${NC}"
        echo "Then run the installer again to complete setup."
    fi
    exit 0
fi

# ============================================================================
# Audio Device Selection
# ============================================================================
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}  Audio Device Selection${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

echo "Available audio output devices:"
echo ""

# List audio devices and create an array
declare -a DEVICES
declare -a CARD_NUMBERS
INDEX=1

# Parse aplay -l output to get device info
while IFS= read -r line; do
    if [[ $line =~ ^card\ ([0-9]+):\ ([^[]+)\[([^\]]+)\] ]]; then
        CARD_NUM="${BASH_REMATCH[1]}"
        CARD_NAME="${BASH_REMATCH[2]}"
        DEVICE_NAME="${BASH_REMATCH[3]}"
        DEVICES[$INDEX]="$CARD_NAME[$DEVICE_NAME]"
        CARD_NUMBERS[$INDEX]=$CARD_NUM
        
        # Highlight likely PCM5102 devices
        if [[ "$DEVICE_NAME" =~ hifiberry|HiFiBerry|pcm5102|PCM5102|i2s|I2S ]]; then
            echo -e "  ${GREEN}$INDEX) Card $CARD_NUM: $CARD_NAME[$DEVICE_NAME] ← Likely PCM5102${NC}"
        else
            echo "  $INDEX) Card $CARD_NUM: $CARD_NAME[$DEVICE_NAME]"
        fi
        ((INDEX++))
    fi
done < <(aplay -l 2>/dev/null)

if [ ${#DEVICES[@]} -eq 0 ]; then
    echo -e "${YELLOW}⚠️  No audio devices detected!${NC}"
    echo "   This might be normal if I2S isn't configured yet."
    echo "   Using default card 0"
    SELECTED_CARD=0
else
    echo ""
    echo "Which audio device would you like to use?"
    echo "(For PCM5102 DAC, select the hifiberry-dac or snd_rpi_hifiberry_dac device)"
    echo ""
    read -p "Enter number [1]: " DEVICE_CHOICE </dev/tty

    # Default to 1 if empty
    DEVICE_CHOICE=${DEVICE_CHOICE:-1}

    if [ $DEVICE_CHOICE -ge 1 ] && [ $DEVICE_CHOICE -lt $INDEX ]; then
        SELECTED_CARD=${CARD_NUMBERS[$DEVICE_CHOICE]}
        echo -e "${GREEN}✓ Selected: ${DEVICES[$DEVICE_CHOICE]}${NC}"
    else
        echo -e "${YELLOW}⚠️  Invalid selection, using default card 0${NC}"
        SELECTED_CARD=0
    fi
fi

echo ""
echo -e "Configuring ALSA for card ${CYAN}$SELECTED_CARD${NC}..."

# Create ALSA configuration
cat > /tmp/asound.conf << EOF
# ALSA configuration for Dub Siren V2
# Generated by setup.sh

pcm.!default {
    type hw
    card $SELECTED_CARD
}

ctl.!default {
    type hw
    card $SELECTED_CARD
}
EOF

sudo mv /tmp/asound.conf /etc/asound.conf
echo -e "${GREEN}✓ ALSA configured${NC}"
echo ""

# ============================================================================
# Build the C++ Application
# ============================================================================
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}  Building Dub Siren (C++)${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

cd "$CPP_DIR"

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo -e "${GREEN}📦 Configuring build...${NC}"
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release"

if [ "$IS_PI" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DBUILD_FOR_PI=ON"
fi

cmake .. $CMAKE_ARGS

# Build (single-threaded on Pi Zero to avoid OOM crashes)
echo -e "${GREEN}🔨 Compiling (this may take 5-10 minutes on Pi Zero)...${NC}"
if [ "$IS_PI" = true ]; then
    make -j1
else
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi

echo -e "${GREEN}✓ Build complete${NC}"
echo ""

# ============================================================================
# Setup MP3 Directory
# ============================================================================
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}  MP3 Playback Mode Setup${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

MP3_DIR="$USER_HOME/dubsiren/mp3s"
REPO_MP3_DIR="$PROJECT_DIR/mp3s"

echo -e "${GREEN}📦 Creating MP3 directory...${NC}"
mkdir -p "$MP3_DIR"
chown $INSTALL_USER:$INSTALL_USER "$MP3_DIR"

# Copy MP3 files from repo if any exist
if [ -d "$REPO_MP3_DIR" ]; then
    MP3_COUNT=$(find "$REPO_MP3_DIR" -maxdepth 1 -name "*.mp3" 2>/dev/null | wc -l)
    if [ "$MP3_COUNT" -gt 0 ]; then
        echo -e "${GREEN}📁 Copying $MP3_COUNT MP3 file(s) from repository...${NC}"
        cp -f "$REPO_MP3_DIR"/*.mp3 "$MP3_DIR/" 2>/dev/null || true
        chown $INSTALL_USER:$INSTALL_USER "$MP3_DIR"/*.mp3 2>/dev/null || true
        echo -e "${GREEN}✓ MP3 files copied${NC}"
    else
        echo -e "${YELLOW}ℹ  No MP3 files found in repository${NC}"
    fi

    # Copy test script if it exists
    if [ -f "$REPO_MP3_DIR/test-tone.sh" ]; then
        cp -f "$REPO_MP3_DIR/test-tone.sh" "$MP3_DIR/"
        chmod +x "$MP3_DIR/test-tone.sh"
        chown $INSTALL_USER:$INSTALL_USER "$MP3_DIR/test-tone.sh"
    fi
fi

echo -e "${GREEN}✓ MP3 directory created at: ${CYAN}$MP3_DIR${NC}"
echo ""
echo -e "${YELLOW}To use MP3 mode:${NC}"
echo "  1. Place MP3 files in: $MP3_DIR"
echo "  2. Toggle pitch envelope OFF→ON 5 times within 2 seconds"
echo "  3. Press TRIGGER to play, SHIFT to cycle files"
echo "  4. Mode auto-exits when playback finishes"
echo ""

# ============================================================================
# Create systemd Service
# ============================================================================
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}  Systemd Service Configuration${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

BINARY_PATH="$CPP_DIR/build/dubsiren"

echo "Creating systemd service..."

cat > /tmp/dubsiren-cpp.service << EOF
[Unit]
Description=Dub Siren V2 Synthesizer (C++ Edition)
After=sound.target

[Service]
Type=simple
# Run as root for GPIO access (pigpio requires direct hardware access)
WorkingDirectory=$CPP_DIR
ExecStart=$BINARY_PATH --device hw:$SELECTED_CARD,0
Restart=on-failure
RestartSec=5
Nice=-10

# Real-time priority for audio
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=80

[Install]
WantedBy=multi-user.target
EOF

sudo mv /tmp/dubsiren-cpp.service /etc/systemd/system/dubsiren-cpp.service
sudo systemctl daemon-reload

echo -e "${GREEN}✓ Service created: dubsiren-cpp.service${NC}"
echo ""

# ============================================================================
# Auto-Start Configuration
# ============================================================================
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}  Auto-Start Configuration${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

echo "Would you like the Dub Siren to start automatically on boot?"
echo ""
read -p "Enable auto-start? (y/N): " -n 1 -r </dev/tty
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Enabling auto-start service..."
    sudo systemctl enable dubsiren-cpp.service
    echo -e "${GREEN}✓ Auto-start enabled${NC}"
    echo ""
    echo "The Dub Siren will start automatically on boot."
    echo ""
    echo "To disable auto-start later:"
    echo -e "  ${CYAN}sudo systemctl disable dubsiren-cpp.service${NC}"
else
    echo "Auto-start not enabled."
    echo ""
    echo "To enable auto-start later:"
    echo -e "  ${CYAN}sudo systemctl enable dubsiren-cpp.service${NC}"
fi

echo ""

# ============================================================================
# Summary
# ============================================================================
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}  Setup Complete!${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

echo -e "${YELLOW}Binary location:${NC}"
echo -e "  $BINARY_PATH"
echo ""

echo -e "${YELLOW}Service commands:${NC}"
echo -e "  Start:   ${CYAN}sudo systemctl start dubsiren-cpp.service${NC}"
echo -e "  Stop:    ${CYAN}sudo systemctl stop dubsiren-cpp.service${NC}"
echo -e "  Status:  ${CYAN}sudo systemctl status dubsiren-cpp.service${NC}"
echo -e "  Logs:    ${CYAN}journalctl -u dubsiren-cpp.service -f${NC}"
echo ""

echo -e "${YELLOW}Manual testing:${NC}"
echo -e "  Simulation:  ${CYAN}$BINARY_PATH --simulate --interactive${NC}"
echo -e "  Hardware:    ${CYAN}$BINARY_PATH${NC}"
echo ""

echo -e "${YELLOW}Control Surface (GPIO):${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  5 Rotary Encoders + 4 Buttons = 10 Parameters (14 GPIO pins)"
echo ""
echo "  Bank A: Volume, Filter Freq, Filter Res, Delay FB, Reverb Mix"
echo "  Bank B: Release, Delay Time, Filter Res, Osc Wave, Reverb Size"
echo "  Buttons: Trigger, Pitch Env, Shift, Shutdown"
echo ""
echo -e "  ${YELLOW}⚠️  IMPORTANT: Avoid GPIO 18, 19, 21 (used by I2S audio)${NC}"
echo ""
echo "  See GPIO_WIRING_GUIDE.md for complete wiring diagrams"
echo ""
