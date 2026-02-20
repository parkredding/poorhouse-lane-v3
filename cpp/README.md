# Dub Siren V2 - C++ Implementation

High-performance C++ implementation of the dub siren synthesizer, optimized for Raspberry Pi Zero 2 W.

## Why C++?

The Python implementation was hitting CPU limits (~80-100% on a single core), causing audio pulsing when adding features. This C++ version provides:

- **5-10x faster DSP processing**
- **True real-time performance**
- **Headroom for new features**
- **Foundation for future JUCE integration**

## Quick Start (Raspberry Pi)

### One-Line Installer (Recommended)

```bash
curl -sSL https://raw.githubusercontent.com/parkredding/poor-house-dub-v2/main/cpp/install.sh | bash
```

This will:
- Install all build dependencies (cmake, ALSA, pigpio)
- Configure I2S audio for PCM5102 DAC
- Let you select your audio device
- Build the C++ application
- Create and configure the systemd service
- Optionally enable auto-start on boot

After installation, reboot and the siren will be ready!

### Manual Installation

If you prefer to install manually:

```bash
# Clone the repository
git clone https://github.com/parkredding/poor-house-dub-v2.git
cd poor-house-dub-v2/cpp

# Run setup (installs deps, configures audio, builds, creates service)
bash setup.sh
```

## Building (Development)

### Prerequisites

**On Raspberry Pi:**
```bash
sudo apt update
sudo apt install build-essential cmake libasound2-dev libpigpio-dev
```

**On macOS (for development):**
```bash
brew install cmake
```

### Build Commands

**Debug build (with symbols, no optimization):**
```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4
```

**Release build (optimized):**
```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

**Raspberry Pi optimized build:**
```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_FOR_PI=ON
make -j2
```

## Usage

```bash
# Test in simulation mode (no hardware required)
./dubsiren --simulate --interactive

# Run on hardware
./dubsiren

# Specify audio device
./dubsiren --device hw:0,0

# Adjust buffer size for lower latency or fewer underruns
./dubsiren --buffer-size 512
```

### Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `--sample-rate RATE` | Audio sample rate | 48000 |
| `--buffer-size SIZE` | Audio buffer size | 256 |
| `--device DEVICE` | ALSA audio device | "default" |
| `--simulate` | Run without hardware | false |
| `--interactive` | Keyboard control mode | false |
| `--help` | Show help message | - |

### Interactive Mode Commands

| Key | Action |
|-----|--------|
| `t` | Toggle trigger (start/stop siren) |
| `p` | Cycle pitch envelope mode (none → up → down) |
| `s` | Show status |
| `h` | Show help |
| `q` | Quit |

**Note:** On hardware, pitch envelope is controlled by a 3-position toggle switch, not a button.

## Service Management

The installer creates a systemd service for auto-start and easy management:

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

## Architecture

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

## File Structure

```
cpp/
├── CMakeLists.txt           # Build configuration
├── README.md                # This file
├── include/
│   ├── Common.h             # Shared types and utilities
│   ├── DSP/
│   │   ├── Oscillator.h     # PolyBLEP oscillator
│   │   ├── Envelope.h       # ADSR envelope
│   │   ├── LFO.h            # Low-frequency oscillator
│   │   ├── Filter.h         # Low-pass filter
│   │   ├── Delay.h          # Tape-style delay
│   │   └── Reverb.h         # Chamber reverb
│   ├── Audio/
│   │   ├── AudioEngine.h    # Main synth engine
│   │   └── AudioOutput.h    # ALSA audio output
│   └── Hardware/
│       ├── GPIOController.h # Raspberry Pi GPIO
│       └── LEDController.h  # WS2812 LED control
├── src/
│   ├── main.cpp             # Entry point
│   ├── DSP/
│   │   ├── Oscillator.cpp
│   │   ├── Envelope.cpp
│   │   ├── LFO.cpp
│   │   ├── Filter.cpp
│   │   ├── Delay.cpp
│   │   └── Reverb.cpp
│   ├── Audio/
│   │   ├── AudioEngine.cpp
│   │   └── AudioOutput.cpp
│   └── Hardware/
│       ├── GPIOController.cpp
│       └── LEDController.cpp
└── build/                   # Build output (git-ignored)
```

## Performance

Compared to Python implementation:

| Metric | Python | C++ | Improvement |
|--------|--------|-----|-------------|
| CPU Usage (single core) | 80-100% | 10-20% | **5-8x** |
| Latency (256 samples) | ~5ms | ~5ms | Same |
| Headroom for features | None | **60-80%** | Massive |

## Hardware Requirements

- Raspberry Pi Zero 2 W
- PCM5102 I2S DAC
- 5x Rotary encoders (EC11 5-pin, no VCC required)
- 3x Momentary switches (Trigger, Shift, Shutdown)
- 1x 3-Position ON/OFF/ON toggle switch (Pitch Envelope)
- 5V 2.5A power supply
- Optional: WS2812D-F5 RGB LED for status indication

See [HARDWARE.md](../HARDWARE.md) and [GPIO_WIRING_GUIDE.md](../GPIO_WIRING_GUIDE.md) for wiring.

## Features

- ✅ Pitch envelope with 3-position toggle switch (up/off/down)
- ✅ Secret modes (NJD and UFO) with preset cycling
- ✅ Optional WS2812 RGB LED status indicator
- ✅ Sound-reactive LED pulsing

## Future Plans

- [ ] JUCE framework integration for VST3 plugin support
- [ ] MIDI input support
- [ ] Preset save/load
- [ ] OLED display support

## License

MIT License - see [LICENSE](../LICENSE) file.
