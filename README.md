# Dub Siren V2

A professional dub siren synthesizer built on Raspberry Pi Zero 2 with PCM5102 I2S DAC.

![Platform](https://img.shields.io/badge/platform-Raspberry%20Pi%20Zero%202-red)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![License](https://img.shields.io/badge/license-MIT-green)

## Features

- **High-Performance C++ Engine**
  - 5-10x faster than Python implementation
  - ~10-20% CPU usage (vs 80-100% Python)
  - Rock-solid real-time audio without pulsing

- **Real-time Audio Synthesis**
  - Multiple oscillator waveforms (Sine, Square, Saw, Triangle)
  - PolyBLEP anti-aliasing for clean sound
  - Envelope generator with adjustable release
  - Low-frequency oscillator (LFO) for modulation

- **Professional DSP Effects**
  - Low-pass filter with resonance control
  - Analog-style delay/echo with pitch-shifting time modulation
  - Hybrid chamber reverb with early reflections, diffusion, and warm damping

- **Hardware Control Surface**
  - 5 rotary encoders with bank switching (10 parameters total)
  - 3 momentary switches (trigger, shift, shutdown)
  - 3-position toggle switch for pitch envelope (up/off/down)
  - Shift button for accessing Bank A/B parameters
  - Uses 15 GPIO pins (avoids I2S conflict)
  - Secret modes: NJD (rasta presets) and UFO (sci-fi presets)

- **Optional Status LED**
  - WS2812 RGB LED for visual feedback
  - Amber during boot, lime green when ready
  - Slow color cycling in normal mode
  - Rasta colors in NJD mode, green/purple in UFO mode
  - Sound-reactive pulsing

- **High-Quality Audio**
  - PCM5102 I2S DAC for pristine audio output
  - 48kHz sample rate
  - Low latency real-time processing

- **Appliance Mode**
  - Read-only filesystem protection
  - Safe to unplug power anytime
  - No SD card corruption risk

## Quick Start

### Prerequisites

- Raspberry Pi Zero 2 W
- PCM5102 DAC module
- 5x rotary encoders (EC11 5-pin, no VCC required)
- 3x momentary switches
- 1x 3-position ON/OFF/ON toggle switch (for pitch envelope)
- MicroSD card (8GB+)
- 5V 2.5A power supply
- Optional: WS2812D-F5 RGB LED for status indication

### Installation

#### One-Line Installer (Recommended)

The easiest way to install on your Raspberry Pi Zero 2W:

```bash
curl -sSL https://raw.githubusercontent.com/parkredding/poor-house-dub-v2/main/cpp/install.sh | bash
```

This will:
- Install all build dependencies (cmake, ALSA, libgpiod)
- Configure I2S audio for PCM5102 DAC
- Build the C++ application
- Create and configure the systemd service
- Display wiring instructions

After installation completes, reboot your Pi and follow the on-screen instructions.

#### Manual Installation

If you prefer to install manually:

1. **Flash Raspberry Pi OS**
   ```bash
   # Use Raspberry Pi Imager to flash Raspberry Pi OS Lite (64-bit)
   ```

2. **Clone the repository**
   ```bash
   git clone https://github.com/parkredding/poor-house-dub-v2.git
   cd poor-house-dub-v2
   ```

3. **Run setup script**
   ```bash
   bash setup.sh
   ```

4. **Reboot**
   ```bash
   sudo reboot
   ```

### Hardware Setup

See [HARDWARE.md](HARDWARE.md) for detailed wiring instructions.

**Testing incrementally?** Check out [MINIMAL_BUILD.md](MINIMAL_BUILD.md) to build with just 1-2 encoders and a button for quick testing.

**Quick PCM5102 Wiring:**
```
PCM5102    ->  Raspberry Pi Zero 2
VIN        ->  3.3V (Pin 1)
GND        ->  GND (Pin 6)
LCK        ->  GPIO 18 (Pin 12)
BCK        ->  GPIO 19 (Pin 35)
DIN        ->  GPIO 21 (Pin 40)
SCK        ->  GND (for 48kHz)
FMT        ->  GND (I2S format)
XSMT       ->  GND (soft mute OFF)
```

### Running the Siren

**Test in simulation mode (no hardware required):**
```bash
~/poor-house-dub-v2/cpp/build/dubsiren --simulate --interactive
```

**Run on hardware:**
```bash
~/poor-house-dub-v2/cpp/build/dubsiren
```

**Run as system service:**
```bash
sudo systemctl enable dubsiren-cpp.service
sudo systemctl start dubsiren-cpp.service
```

### Enable Appliance Mode (Recommended)

After everything is working, enable appliance mode to protect against SD card corruption:

```bash
sudo ./enable_appliance_mode.sh
```

This makes the filesystem read-only - users can safely unplug power anytime!

## Control Layout

The control surface uses 5 rotary encoders with a **shift button** for bank switching, providing access to 10 parameters across two banks:

```
Encoders:  [Encoder 1]  [Encoder 2]  [Encoder 3]  [Encoder 4]  [Encoder 5]
            LFO Depth    Base Freq    Filter Freq  Delay FB     Reverb Mix
            (LFO Rate)   (Delay)      (Filter Res) (Osc Wave)   (Rev Size)

Buttons:   [TRIGGER]    [↑|○|↓]      [SHIFT]      [SHUTDOWN]
                        PITCH ENV
                        (3-pos toggle)

Optional:  [◉ LED]  ← WS2812 status indicator
```

### Bank A (Normal Mode)

| Encoder | Parameter | Function | Range |
|---------|-----------|----------|-------|
| **Encoder 1** | LFO Depth | LFO filter modulation depth | 0.0 to 1.0 |
| **Encoder 2** | Base Freq | Oscillator base pitch | 50Hz to 2kHz |
| **Encoder 3** | Filter Freq | Low-pass filter cutoff frequency | 20Hz to 20kHz |
| **Encoder 4** | Delay FB | Delay feedback amount | 0.0 to 0.95 |
| **Encoder 5** | Reverb Mix | Reverb dry/wet mix | 0.0 (dry) to 1.0 (wet) |

### Bank B (Shift Held)

| Encoder | Parameter | Function | Range |
|---------|-----------|----------|-------|
| **Encoder 1** | LFO Rate | LFO oscillation rate | 0.1Hz to 20Hz |
| **Encoder 2** | Delay Time | Delay effect time | 0.001s to 2.0s |
| **Encoder 3** | Filter Res | Filter resonance/emphasis | 0.0 to 0.95 |
| **Encoder 4** | Osc Wave | Oscillator waveform | Sine/Square/Saw/Triangle |
| **Encoder 5** | Reverb Size | Reverb room size | 0.0 to 1.0 |

### Button Functions

| Button | Function | Behavior |
|--------|----------|----------|
| **TRIGGER** | Trigger the siren | Press to start, release to stop |
| **SHIFT** | Switch to Bank B | Hold to access Bank B parameters |
| **SHUTDOWN** | Safe system shutdown | Press to safely power down the Pi |

### Pitch Envelope Switch (3-Position Toggle)

| Position | Effect |
|----------|--------|
| **UP** | Pitch rises (2 octaves) on release |
| **CENTER** | No pitch envelope |
| **DOWN** | Pitch falls (2 octaves) on release |

### Secret Modes

Rapidly toggle the pitch envelope switch to unlock hidden preset modes:

| Mode | Activation | Description |
|------|------------|-------------|
| **NJD Mode** | 5 toggles in 1 second | Classic NJD dub siren presets |
| **UFO Mode** | 10 toggles in 2 seconds | Sci-fi alien sound presets |

**In secret modes:**
- Use **SHIFT** button to cycle through presets
- Delay and reverb effects still apply
- Toggle switch again to exit (or power cycle)

### Optional Status LED (WS2812)

| State | LED Behavior |
|-------|--------------|
| **Boot** | Amber color |
| **Ready** | Lime green (2 sec), then cycling |
| **Normal Mode** | Slow color cycling (changes over minutes) |
| **NJD Mode** | Fast Rasta colors (red/yellow/green) |
| **UFO Mode** | Fast green/purple alien theme |
| **Audio Playing** | Pulses brighter with sound |

## Architecture

### Software Components

```
┌─────────────────────────────────────────┐
│           main.cpp                       │
│      (Application Entry Point)           │
└─────────────────────────────────────────┘
                  │
    ┌─────────────┼─────────────┐
    │             │             │
    ▼             ▼             ▼
┌────────┐  ┌──────────┐  ┌──────────┐
│  GPIO  │  │  Audio   │  │  Audio   │
│Control │─▶│  Engine  │─▶│  Output  │
└────────┘  └──────────┘  └──────────┘
                │
    ┌───────────┴───────────────┐
    │          DSP              │
    │  ┌──────┐ ┌──────┐       │
    │  │ Osc  │ │ Env  │       │
    │  └──────┘ └──────┘       │
    │  ┌──────┐ ┌──────┐       │
    │  │Filter│ │ LFO  │       │
    │  └──────┘ └──────┘       │
    │  ┌──────┐ ┌──────┐       │
    │  │Delay │ │Reverb│       │
    │  └──────┘ └──────┘       │
    └───────────────────────────┘
```

### File Structure

```
poor-house-dub-v2/
├── cpp/                         # C++ implementation
│   ├── CMakeLists.txt           # Build configuration
│   ├── build.sh                 # Build script
│   ├── setup.sh                 # Full setup script
│   ├── install.sh               # One-line installer
│   ├── include/                 # Header files
│   │   ├── Common.h
│   │   ├── Audio/
│   │   │   ├── AudioEngine.h
│   │   │   └── AudioOutput.h
│   │   ├── DSP/
│   │   │   ├── Oscillator.h
│   │   │   ├── Envelope.h
│   │   │   ├── Filter.h
│   │   │   ├── LFO.h
│   │   │   ├── Delay.h
│   │   │   └── Reverb.h
│   │   └── Hardware/
│   │       ├── GPIOController.h
│   │       └── LEDController.h
│   └── src/                     # Source files
│       ├── main.cpp
│       ├── Audio/
│       ├── DSP/
│       └── Hardware/
├── setup.sh                     # Main setup (runs cpp/setup.sh)
├── enable_appliance_mode.sh     # Enable read-only filesystem
├── disable_appliance_mode.sh    # Disable for updates
├── HARDWARE.md                  # Hardware wiring guide
├── GPIO_WIRING_GUIDE.md         # GPIO pin assignments
├── QUICKSTART.md                # Quick start guide
└── README.md                    # This file
```

## Development

### Command Line Options

```bash
./dubsiren --help

Options:
  --sample-rate RATE    Audio sample rate (default: 48000)
  --buffer-size SIZE    Audio buffer size (default: 256)
  --device DEVICE       ALSA audio device (default: "default")
  --simulate            Run in simulation mode
  --interactive         Run in interactive mode
```

### Simulation Mode

Test without hardware using simulation mode:

```bash
./dubsiren --simulate --interactive

Commands:
  t - Toggle trigger (start/stop siren)
  p - Cycle pitch envelope mode
  s - Show status
  h - Show help
  q - Quit
```

### Building from Source

```bash
cd cpp
./build.sh           # Release build
./build.sh --debug   # Debug build
./build.sh --clean   # Clean rebuild
```

## Performance

| Metric | Value |
|--------|-------|
| **CPU Usage** | ~10-20% on Pi Zero 2 (single core) |
| **Latency** | ~5ms (256 sample buffer @ 48kHz) |
| **Sample Rate** | 48kHz |
| **Bit Depth** | 16-bit I2S output |

## Troubleshooting

### No audio output
```bash
# Check I2S configuration
grep "dtparam=i2s=on" /boot/config.txt
grep "dtoverlay=hifiberry-dac" /boot/config.txt

# List audio devices
aplay -l

# Test with ALSA
speaker-test -t wav -c 2 -D hw:0,0
```

### Service issues
```bash
# Check service status
sudo systemctl status dubsiren-cpp.service

# View logs
journalctl -u dubsiren-cpp.service -f

# Restart service
sudo systemctl restart dubsiren-cpp.service
```

### Buffer underruns
```bash
# Increase buffer size
./dubsiren --buffer-size 512

# Check CPU usage
top
```

See [HARDWARE.md](HARDWARE.md) for more troubleshooting tips.

## Technical Details

### Audio Processing Chain

```
Oscillator → Envelope → Filter → Delay → Reverb → Output
     ↑
    LFO (modulation)
```

### DSP Algorithms

- **Oscillator:** PolyBLEP anti-aliased waveforms (sine, square, saw, triangle)
- **Filter:** One-pole low-pass with resonance feedback
- **Delay:** Circular buffer with feedback and analog-style pitch-shifting modulation
- **Reverb:** Hybrid chamber reverb (early reflections + allpass diffusion + 6 damped comb filters)
- **Envelope:** ADSR with configurable release

### GPIO Interrupt Handling

- Rotary encoders use quadrature decoding via libgpiod
- Software debouncing (50ms)
- Interrupt-driven for low latency
- Internal pull-up resistors enabled

## Service Management

```bash
# Start the siren
sudo systemctl start dubsiren-cpp.service

# Stop the siren
sudo systemctl stop dubsiren-cpp.service

# Check status
sudo systemctl status dubsiren-cpp.service

# View logs
journalctl -u dubsiren-cpp.service -f

# Enable auto-start on boot
sudo systemctl enable dubsiren-cpp.service

# Disable auto-start
sudo systemctl disable dubsiren-cpp.service
```

## Contributing

Contributions welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test on actual hardware
5. Submit a pull request

## License

MIT License - see LICENSE file for details

## Acknowledgments

- PCM5102 DAC implementation based on HiFiBerry DAC
- DSP algorithms inspired by classic analog synthesizers
- Dub siren concept from Jamaican sound system culture

## Support

- **Issues:** https://github.com/parkredding/poor-house-dub-v2/issues
- **Documentation:** See [HARDWARE.md](HARDWARE.md)

## Roadmap

- [ ] MIDI input support
- [ ] Preset save/load system
- [ ] OLED display for parameter feedback
- [ ] Additional effects (chorus, phaser)
- [ ] CV/Gate inputs for modular integration
- [ ] Web interface for remote control
- [ ] JUCE framework integration for VST3 plugin

---

**Built with ❤️ for the dub community**
