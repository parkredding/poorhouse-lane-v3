# Minimal Build Guide

This guide shows how to build a minimal test setup with just 2-3 controls to verify functionality before wiring up the complete control surface.

## Why a Minimal Build?

- **Quick testing**: Verify your Pi, DAC, and basic GPIO before committing to full wiring
- **Incremental assembly**: Build and test one component at a time
- **Troubleshooting**: Isolate problems to specific components
- **Prototyping**: Test the concept before building an enclosure

## Minimal Build Option 1: Just Audio

Test audio output without any GPIO controls.

### Hardware Required:
- Raspberry Pi Zero 2 W
- PCM5102 DAC module
- Speaker or headphones

### Wiring:
Connect only the PCM5102 DAC (see [HARDWARE.md](HARDWARE.md) for details).

### Testing:
```bash
# Run in simulation mode
~/poor-house-dub-v2/cpp/build/dubsiren --simulate --interactive

# Commands:
# t - Toggle trigger (start/stop siren)
# p - Cycle pitch envelope
# s - Show status
# q - Quit
```

## Minimal Build Option 2: LFO Depth + Trigger

Add basic LFO depth control and trigger button.

### Hardware Required:
- Everything from Option 1, plus:
- 1x rotary encoder (KY-040 or similar)
- 1x momentary switch

### Wiring:

**Encoder 1 (LFO Depth):**
```
Encoder CLK → GPIO 2
Encoder DT  → GPIO 17
Encoder GND → GND
```

**Trigger Button:**
```
Switch Pin 1 → GPIO 4
Switch Pin 2 → GND
```

### Testing:
```bash
~/poor-house-dub-v2/cpp/build/dubsiren
```

You should see the siren start up with partial hardware detected.
**Controls:**
- Rotate encoder 1 to adjust LFO depth
- Press trigger button to start siren
- Release trigger button to stop

## Minimal Build Option 3: LFO Depth + Base Freq + Trigger

Add base frequency control for pitch adjustment.

### Hardware Required:
- Everything from Option 2, plus:
- 1x additional rotary encoder

### Wiring:

Add **Encoder 2 (Base Frequency):**
```
Encoder CLK → GPIO 22
Encoder DT  → GPIO 27
Encoder GND → GND
```

### Testing:
```bash
~/poor-house-dub-v2/cpp/build/dubsiren
```

**Controls:**
- Encoder 1: LFO Depth (0.0 to 1.0)
- Encoder 2: Base frequency (50Hz to 2kHz) - adjusts the pitch
- Trigger: Start/stop siren

## Adding More Controls

The control surface gracefully handles partial hardware. You can add encoders and buttons one at a time:

### Full Encoder List:
1. **Encoder 1** (GPIO 2, 17) - Bank A: LFO Depth | Bank B: LFO Rate
2. **Encoder 2** (GPIO 22, 27) - Bank A: Base Freq | Bank B: Delay Time
3. **Encoder 3** (GPIO 24, 23) - Bank A: Filter Freq | Bank B: Filter Res
4. **Encoder 4** (GPIO 26, 20) - Bank A: Delay FB | Bank B: Osc Waveform
5. **Encoder 5** (GPIO 13, 14) - Bank A: Reverb Mix | Bank B: Reverb Size

### Full Button List:
1. **Trigger** (GPIO 4) - Start/stop siren
2. **Shift** (GPIO 15) - Access Bank B parameters; cycles presets in secret modes
3. **Shutdown** (GPIO 3) - Safe system shutdown

### Pitch Envelope Switch (3-Position Toggle):
- **UP** (GPIO 9) - Pitch rises on release
- **OFF** (center) - No pitch envelope
- **DOWN** (GPIO 10) - Pitch falls on release

### Optional LED:
- **WS2812 LED** (GPIO 12) - Status indicator with color cycling

## Recommended Build Order

1. **Start with audio only** - Verify DAC and I2S work
2. **Add trigger button** - Test GPIO and basic control
3. **Add LFO depth encoder** - Verify encoder wiring and quadrature decoding
4. **Add base freq encoder** - Test multiple encoders working together
5. **Add remaining encoders** - Build out Bank A controls
6. **Add shift button** - Enable Bank B access
7. **Add pitch envelope switch** - 3-position toggle for pitch control
8. **Add shutdown button** - Safe power-off
9. **Add status LED (optional)** - WS2812 for visual feedback

## Troubleshooting Partial Builds

### Encoder not working:
- Check CLK and DT aren't swapped
- Verify GND connection
- Try rotating slowly and watching console output
- Check GPIO pin numbers (BCM mode)

### Button not working:
- Verify it's wired to GND (active-low)
- Check GPIO pin number
- Try a different button to rule out hardware failure

### Multiple controls failing:
- Check common GND connection
- Verify power supply is adequate
- Look for shorts between GPIO pins
- Review console output for specific error messages

## Secret Modes

Once you have the pitch envelope switch wired, you can access hidden preset modes:

- **NJD Mode**: Toggle the pitch switch rapidly 5 times in 1 second
- **UFO Mode**: Toggle the pitch switch rapidly 10 times in 2 seconds

In secret modes, the Shift button cycles through presets instead of switching banks.

## Important Notes

- **I2S Pins**: Never use GPIO 18, 19, or 21 for controls (reserved for audio)
- **GPIO Mode**: All pin numbers use BCM numbering, not physical pin numbers
- **Pull-ups**: Internal pull-up resistors are enabled in software (no external resistors needed)
- **Active-Low**: Buttons and switch terminals are active-low (connected to GND = LOW signal)

## See Also

- [GPIO_WIRING_GUIDE.md](GPIO_WIRING_GUIDE.md) - Complete wiring guide with diagrams
- [HARDWARE.md](HARDWARE.md) - Hardware specifications and full pin assignments
- [QUICKSTART.md](QUICKSTART.md) - Quick setup and installation guide
- [README.md](README.md) - Full project documentation
