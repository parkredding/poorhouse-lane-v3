# Quick Start Guide

This guide will get your Dub Siren up and running in 15 minutes.

## Step 1: Hardware Assembly (5 minutes)

### Minimal Setup (PCM5102 only)

Connect just the DAC first to test audio:

```
PCM5102 Pin    →    Raspberry Pi Pin
VIN            →    Pin 1 (3.3V)
GND            →    Pin 6 (GND)
LCK            →    Pin 12 (GPIO 18)
BCK            →    Pin 35 (GPIO 19)
DIN            →    Pin 40 (GPIO 21)
```

Additional PCM5102 configuration pins (if available):
- SCK → GND
- FLT → GND
- FMT → GND
- XSMT → GND (soft mute OFF)

## Step 2: Software Installation (5 minutes)

### On your Raspberry Pi:

```bash
# One-line installer (recommended)
curl -sSL https://raw.githubusercontent.com/parkredding/poor-house-dub-v2/main/cpp/install.sh | bash
```

Or manually:

```bash
# Clone repository
cd ~
git clone https://github.com/parkredding/poor-house-dub-v2.git
cd poor-house-dub-v2

# Run setup (builds C++ and configures service)
bash setup.sh
```

### Reboot Required

```bash
sudo reboot
```

## Step 3: Test Audio (2 minutes)

### After reboot, test the audio output:

```bash
# List audio devices
aplay -l

# You should see something like:
# card 0: sndrpihifiberry [snd_rpi_hifiberry_dac]
```

### Test with ALSA:

```bash
speaker-test -t wav -c 2 -D hw:0,0
```

## Step 4: Test Without Controls (1 minute)

Run in simulation mode to test the synthesizer:

```bash
~/poor-house-dub-v2/cpp/build/dubsiren --simulate --interactive
```

Commands in interactive mode:
- `t` - Toggle trigger (start/stop siren)
- `p` - Cycle pitch envelope mode
- `s` - Show status
- `q` - Quit

## Step 5: Add Controls (Optional)

The control surface uses **5 rotary encoders** with a **shift button** for bank switching:

- **Bank A (normal):** Volume, Filter Freq, Base Freq, Delay FB, Reverb Mix
- **Bank B (shift held):** Release Time, Delay Time, Filter Res, Osc Waveform, Reverb Size
- **3 Buttons:** Trigger, Shift, Shutdown
- **3-Position Toggle Switch:** Pitch Envelope (UP = rise / OFF = none / DOWN = fall)
- **Optional:** WS2812 RGB LED for status indication

See [GPIO_WIRING_GUIDE.md](GPIO_WIRING_GUIDE.md) for complete wiring instructions.

### Secret Modes

Rapidly toggle the pitch envelope switch to activate hidden presets:
- **NJD Mode:** 5 toggles in 1 second - Classic dub siren presets
- **UFO Mode:** 10 toggles in 2 seconds - Sci-fi alien presets

Use Shift button to cycle through presets within each secret mode.

### Run full system:

```bash
~/poor-house-dub-v2/cpp/build/dubsiren
```

## Step 6: Run at Startup (Optional)

Enable the systemd service to start on boot:

```bash
sudo systemctl enable dubsiren-cpp.service
sudo systemctl start dubsiren-cpp.service

# Check status
sudo systemctl status dubsiren-cpp.service

# View logs
journalctl -u dubsiren-cpp.service -f
```

## Step 7: Enable Appliance Mode (Recommended)

Once everything is working, enable appliance mode to protect against SD card corruption when power is unplugged:

```bash
cd ~/poor-house-dub-v2
sudo ./enable_appliance_mode.sh
```

This makes the filesystem **read-only** using OverlayFS:
- ✅ Users can safely unplug power anytime
- ✅ SD card is protected from corruption
- ✅ System always boots to a known good state

**Note:** All changes after enabling are temporary (lost on reboot). To make updates:

```bash
# Disable appliance mode
sudo ./disable_appliance_mode.sh

# Make your changes, then re-enable
sudo ./enable_appliance_mode.sh
```

---

## Troubleshooting Quick Fixes

### No audio output?

```bash
# Check I2S enabled
grep "dtparam=i2s=on" /boot/config.txt

# Should return: dtparam=i2s=on

# Check DAC overlay
grep "dtoverlay=hifiberry-dac" /boot/config.txt

# Should return: dtoverlay=hifiberry-dac

# If missing, add them and reboot
```

### Permission errors?

```bash
# Add user to gpio and audio groups
sudo usermod -a -G gpio,audio $USER

# Log out and back in
```

### Service not starting?

```bash
# Check service status
sudo systemctl status dubsiren-cpp.service

# View detailed logs
journalctl -u dubsiren-cpp.service -n 50
```

## Basic Usage

### Command Line Options

```bash
# Default mode (hardware)
~/poor-house-dub-v2/cpp/build/dubsiren

# Simulation mode (no hardware)
~/poor-house-dub-v2/cpp/build/dubsiren --simulate --interactive

# Specify audio device
~/poor-house-dub-v2/cpp/build/dubsiren --device hw:0,0

# Larger buffer for stability
~/poor-house-dub-v2/cpp/build/dubsiren --buffer-size 512
```

## What's Next?

1. **Wire up controls** - See [HARDWARE.md](HARDWARE.md) for GPIO pin assignments
2. **Customize sounds** - Adjust parameters with encoders
3. **Add effects** - Use Bank B for advanced parameters
4. **Create enclosure** - Design a case for your dub siren
5. **Go dub!** - Connect to speakers and make some noise!

## Default Control Values

When you start the siren, these are the default values:

| Parameter | Default Value |
|-----------|---------------|
| Volume | 0.5 (50%) |
| Filter Frequency | 2000 Hz |
| Filter Resonance | 0.1 |
| Delay Time | 0.5 seconds |
| Delay Feedback | 0.3 |
| Reverb Size | 0.5 |
| Reverb Mix | 0.3 (30% wet) |
| Release Time | 0.5 seconds |
| Osc Waveform | Sine (0) |
| Base Frequency | 440 Hz |

## Performance Tweaks

### For better performance on Pi Zero 2:

```bash
# Use performance governor
echo "performance" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Disable unnecessary services
sudo systemctl disable bluetooth
sudo systemctl disable wifi

# Reduce GPU memory (in /boot/config.txt)
gpu_mem=16
```

## Next Steps

- Read the full [README.md](README.md) for detailed information
- Check [HARDWARE.md](HARDWARE.md) for complete wiring guide
- See [cpp/README.md](cpp/README.md) for C++ specific details

---

**You're ready to dub! 🔊**
