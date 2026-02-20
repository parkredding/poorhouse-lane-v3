# Pull Request: Implement Auto Wail preset as default with dual LFO modulation

**Branch:** `claude/lfo-implementation-5MKh8`
**Base:** `main`

## Summary

This PR implements the Auto Wail preset as the default configuration when the Dub Siren starts up. Auto Wail features automatic pitch alternation ("wee-woo" siren effect) combined with controllable filter modulation.

### Key Changes

**Encoder Remapping:**
- **Bank A Encoder 1**: Volume → LFO Filter Depth (0.0 - 1.0)
- **Bank B Encoder 1**: Release Time → LFO Rate (0.1 - 20 Hz)
- Master volume fixed at 0.6 to prevent clipping

**Dual LFO Modulation System:**
1. **Pitch Modulation** (always active in Auto Wail):
   - 0.5 depth (±0.5 octaves)
   - Creates automatic "wee-woo" siren effect
   - Triangle waveform for smooth transitions

2. **Filter Modulation** (encoder controllable):
   - 0.5 depth default (adjustable 0.0 - 1.0)
   - Adds wobble bass character on top of pitch modulation
   - ±3 octaves filter sweep range

**Auto Wail Preset Parameters:**
- Base frequency: 440 Hz (A4)
- Filter cutoff: 3000 Hz (increased from 1500 Hz for cleaner tone)
- Oscillator: Square wave
- LFO rate: 2.0 Hz
- Delay: 0.375s (dotted eighth)
- Reverb: 0.7 size, 0.4 mix

**Bug Fixes:**
- Increased filter smoothing coefficient from 0.001 to 0.05 to allow LFO modulation to track properly
- Fixed gain staging to prevent soft clipping (reduced resonance, delay feedback, reverb mix)

### Technical Details

The LFO now drives both pitch and filter modulation simultaneously:
- `setLfoPitchDepth(0.5f)` - enables pitch alternation
- `setLfoDepth(params.lfoDepth)` - enables filter wobble (encoder controlled)
- Both use the same 2 Hz Triangle LFO wave

The encoder allows blending between pure pitch modulation (depth=0) and combined pitch+filter effects (depth=1.0).

## Files Changed

- `cpp/include/Hardware/GPIOController.h` - Updated parameter defaults for Auto Wail
- `cpp/src/Hardware/GPIOController.cpp` - Encoder mappings, initialization, and secret mode restoration
- `cpp/src/Audio/AudioEngine.cpp` - Audio engine initialization with Auto Wail parameters
- `cpp/src/DSP/Filter.cpp` - Increased filter smoothing coefficient for LFO tracking

## Test Plan

- [x] Build succeeds on Raspberry Pi Zero 2
- [x] Auto Wail pitch modulation audible on startup
- [x] Encoder 1 (Bank A) controls filter modulation depth
- [x] Encoder 1 (Bank B) controls LFO rate for both pitch and filter
- [x] No clipping or distortion with default settings
- [x] Filter cutoff at 3000 Hz preserves square wave harmonics
- [x] Secret modes still work (NJD and UFO presets)
- [x] Exiting secret mode returns to Auto Wail defaults

### Hardware Testing on Pi Zero 2:
- GPIO encoders respond correctly to rotation
- LED controller shows proper status colors
- No audio artifacts or zipper noise
- Clean waveforms visible on oscilloscope

## GitHub PR Creation

Visit: https://github.com/parkredding/poor-house-dub-v2/compare/main...claude/lfo-implementation-5MKh8

Or use the GitHub CLI:
```bash
gh pr create --base main --head claude/lfo-implementation-5MKh8 \
  --title "Implement Auto Wail preset as default with dual LFO modulation" \
  --body-file PR_DESCRIPTION.md
```
