# GPIO Wiring Guide for Dub Siren V2

Complete wiring instructions for the Poor House Dub V2 control surface with shift button bank switching.

## Overview

The Dub Siren V2 uses **5 rotary encoders** with a **shift button** to control 10 parameters across 2 banks, plus 3 function buttons, a 3-position pitch envelope switch, and an optional status LED.

**Total GPIO pins: 15-16** (10 for encoders + 3 for buttons + 2 for pitch envelope switch + 1 optional for LED)

### Bank System
- **Bank A (Normal):** Primary dub controls (LFO, frequency, filter, delay, reverb)
- **Bank B (Shift held):** Secondary controls (LFO rate, timing, resonance, waveforms)

## Components Needed

- **5x Rotary Encoders** (EC11 5-pin, no VCC required - uses Pi's internal pull-ups)
- **3x Momentary Switches** (Trigger, Shift, Shutdown)
- **1x 3-Position Toggle Switch** (ON/OFF/ON, SPDT) for pitch envelope
- **Breadboard** or **PCB** for prototyping
- **Jumper wires** (male-to-female, 10-15cm)
- **Pi Zero 2W** with 40-pin GPIO header
- **Optional:** Panel-mount encoders with knobs for enclosure
- **Optional:** WS2812D-F5 RGB LED (5mm through-hole, single addressable LED) for status indication

## Critical: I2S Pin Avoidance

⚠️ **DO NOT USE** these GPIO pins - they are reserved for PCM5102 DAC audio:
- **GPIO 18** (Pin 12) - I2S LRCLK
- **GPIO 19** (Pin 35) - I2S BCLK
- **GPIO 21** (Pin 40) - I2S DOUT

The pin assignments below carefully avoid these pins.

## Complete Pin Assignments

### 5 Rotary Encoders (10 GPIO pins)

| Encoder | Function (Bank A / Bank B) | CLK Pin | DT Pin | Physical Pins |
|---------|---------------------------|---------|--------|---------------|
| **Encoder 1** | LFO Depth / LFO Rate | GPIO 2 | GPIO 17 | Pin 3, Pin 11 |
| **Encoder 2** | Base Freq / Delay Time | GPIO 22 | GPIO 27 | Pin 15, Pin 13 |
| **Encoder 3** | Filter Freq / Filter Res | GPIO 24 | GPIO 23 | Pin 18, Pin 16 |
| **Encoder 4** | Delay Feedback / Osc Wave | GPIO 26 | GPIO 20 | Pin 37, Pin 38 |
| **Encoder 5** | Reverb Mix / Reverb Size | GPIO 13 | GPIO 14 | Pin 33, Pin 8 |

### 3 Buttons (3 GPIO pins)

| Button | Function | GPIO Pin | Physical Pin |
|--------|----------|----------|--------------|
| **Trigger** | Main sound trigger (hold to play) | GPIO 4 | Pin 7 |
| **Shift** | Access Bank B parameters | GPIO 15 | Pin 10 |
| **Shutdown** | Safe system shutdown | GPIO 3 | Pin 5 |

### Pitch Envelope Switch (2 GPIO pins)

3-position ON/OFF/ON toggle switch for pitch envelope selection:

| Position | Pitch Envelope | GPIO Pin | Physical Pin |
|----------|----------------|----------|--------------|
| **UP** | Rise (pitch sweeps up on release) | GPIO 9 | Pin 21 |
| **OFF** | None (no pitch sweep) | — | — |
| **DOWN** | Fall (pitch sweeps down on release) | GPIO 10 | Pin 19 |

### Optional Status LED (1 GPIO pin)

WS2812D-F5 RGB LED for visual status indication:

| Function | GPIO Pin | Physical Pin |
|----------|----------|--------------|
| **LED Data** | GPIO 12 | Pin 32 (PWM0) |

**LED Features:**
- **Amber** during boot
- **Lime Green** when siren is ready
- **Slow color cycling** in normal mode (changes over minutes)
- **Rasta colors** (red/yellow/green) in NJD secret mode
- **Green/purple** alien theme in UFO secret mode
- **Pulses with audio** for sound-reactive feedback

### Power Connections

| Connection | Pin(s) |
|------------|--------|
| **GND** (common ground) | Pins 6, 9, 14, 20, 25, 30, 34, 39 |

**Note:** No 3.3V power needed for encoders - they use the Pi's internal pull-up resistors.

## Parameter Bank Mapping

### Bank A (Normal - no shift)
1. **LFO Depth** - LFO filter modulation depth (0-100%)
2. **Base Frequency** - Oscillator pitch (50-2000 Hz)
3. **Filter Frequency** - Low-pass filter cutoff (20-20000 Hz)
4. **Delay Feedback** - Echo repeats (0-95%)
5. **Reverb Mix** - Reverb dry/wet (0-100%)

### Bank B (Hold Shift)
1. **LFO Rate** - LFO oscillation rate (0.1-20 Hz)
2. **Delay Time** - Echo timing (0.001-2.0s)
3. **Filter Resonance** - Filter emphasis (0-95%)
4. **Osc Waveform** - Oscillator shape (Sine/Square/Saw/Triangle)
5. **Reverb Size** - Room size (0-100%)

## Detailed Wiring Diagrams

### GPIO Header Overview (Pi Zero 2W)

```
Raspberry Pi Zero 2W GPIO Header (40-pin)
┌────────────────────────────────────────┐
│ 1  3.3V        [5V]     2  │
│ 3  [GPIO 2]    5V       4  │  ← Enc1-CLK
│ 5  [GPIO 3]    [GND]    6  │  ← Shutdown, PCM5102 GND
│ 7  [GPIO 4]    [GPIO 14] 8 │  ← Trigger, Enc5-DT
│ 9  [GND]       [GPIO 15] 10│  ← Common GND, Shift
│ 11 [GPIO 17]   GPIO 18  12 │  ← Enc1-DT, I2S (DO NOT USE 18!)
│ 13 [GPIO 27]   [GND]    14 │  ← Enc2-DT
│ 15 [GPIO 22]   [GPIO 23] 16│  ← Enc2-CLK, Enc3-DT
│ 17 3.3V        [GPIO 24] 18│  ← Enc3-CLK
│ 19 [GPIO 10]   [GND]    20 │  ← Pitch Env DOWN, Switch GND
│ 21 [GPIO 9]    GPIO 25  22 │  ← Pitch Env UP
│ 23 GPIO 11     GPIO 8   24 │
│ 25 [GND]       GPIO 7   26 │
│ 27 ID_SD       ID_SC    28 │
│ 29 GPIO 5      [GND]    30 │
│ 31 GPIO 6      GPIO 12  32 │
│ 33 [GPIO 13]   [GND]    34 │  ← Enc5-CLK
│ 35 GPIO 19     GPIO 16  36 │  ← I2S (DO NOT USE 19!)
│ 37 [GPIO 26]   [GPIO 20] 38│  ← Enc4-CLK, Enc4-DT
│ 39 [GND]       GPIO 21  40 │  ← I2S (DO NOT USE 21!)
└────────────────────────────────────────┘

[Bracketed] = Used by Dub Siren V2
```

### Rotary Encoder Wiring (All 5 Encoders)

**5-Pin EC11-style encoders (no VCC required):**

The Pi's internal pull-up resistors are enabled in software, so no external VCC or pull-up resistors are needed.

```
┌─────────────┐
│   Encoder   │
│   (top view)│
└─────────────┘
    │ │ │ │ │
    │ │ │ │ │
    │ │ │ │ └─ GND (or GND 2)
    │ │ │ └─── SW (push button - not used)
    │ │ └───── GND
    │ └─────── DT (B) → GPIO pin
    └───────── CLK (A) → GPIO pin

Note: Pin order may vary by manufacturer - check your encoder's datasheet.
      Some encoders label pins as A/B/C or have different arrangements.
```

**Encoder 1 (LFO Depth / LFO Rate):**
```
CLK (A) → Pin 3  (GPIO 2)
DT (B)  → Pin 11 (GPIO 17)
GND     → Pin 9  (GND)
```

**Encoder 2 (Base Freq / Delay Time):**
```
CLK (A) → Pin 15 (GPIO 22)
DT (B)  → Pin 13 (GPIO 27)
GND     → Pin 9  (GND)
```

**Encoder 3 (Filter Freq / Filter Res):**
```
CLK (A) → Pin 18 (GPIO 24)
DT (B)  → Pin 16 (GPIO 23)
GND     → Pin 9  (GND)
```

**Encoder 4 (Delay FB / Osc Wave):**
```
CLK (A) → Pin 37 (GPIO 26)
DT (B)  → Pin 38 (GPIO 20)
GND     → Pin 39 (GND)
```

**Encoder 5 (Reverb Mix / Reverb Size):**
```
CLK (A) → Pin 33 (GPIO 13)
DT (B)  → Pin 8  (GPIO 14)
GND     → Pin 9  (GND)
```

### Button Wiring (3 Buttons)

All buttons are wired as **active low** (pressed = connects to GND). Internal pull-ups are enabled in software.

**Trigger Button (Main Sound):**
```
Pin 1 → Pin 7  (GPIO 4)
Pin 2 → Pin 9  (GND)
```

**Shift Button (Access Bank B):**
```
Pin 1 → Pin 10 (GPIO 15)
Pin 2 → Pin 9  (GND)
```

**Shutdown Button (Power Off):**
```
Pin 1 → Pin 5  (GPIO 3)
Pin 2 → Pin 6  (GND)
```

### 3-Position Pitch Envelope Switch

Use an ON/OFF/ON SPDT toggle switch. The middle position is OFF (no pitch envelope).

```
3-Position Toggle Switch (ON/OFF/ON)
┌─────────────────────────────────┐
│                                 │
│    UP position (Rise)           │
│         ┌───┐                   │
│         │ ● │ ← Toggle lever    │
│         └───┘                   │
│    [1] [C] [2]  ← Terminals     │
│                                 │
│    DOWN position (Fall)         │
└─────────────────────────────────┘

Wiring:
  Terminal 1 (UP)   → Pin 21 (GPIO 9)
  Terminal C (COM)  → Pin 20 (GND)
  Terminal 2 (DOWN) → Pin 19 (GPIO 10)
```

**Switch positions:**
- **UP:** Terminal 1 connects to Common → GPIO 9 reads LOW → Pitch rises on release
- **CENTER:** Neither terminal connected → Both GPIOs read HIGH → No pitch envelope
- **DOWN:** Terminal 2 connects to Common → GPIO 10 reads LOW → Pitch falls on release

### Optional WS2812 Status LED

The WS2812D-F5 is a 5mm RGB LED with built-in controller - only needs a single data wire plus power.

```
WS2812D-F5 LED (4 pins)
┌────────────────┐
│    (top view)  │
│    ┌──────┐    │
│    │ LED  │    │
│    └──────┘    │
│   [1][2][3][4] │
└────────────────┘
     │  │  │  │
     │  │  │  └─ GND (VSS)
     │  │  └──── Data Out (DOUT) - not used for single LED
     │  └─────── Data In (DIN) → GPIO 12 (Pin 32)
     └────────── VCC (VDD) → 5V (Pin 2 or Pin 4)

Note: Pin order may vary - check your LED's datasheet!
      Common configurations:
      - VDD, DOUT, GND, DIN
      - DIN, VDD, GND, DOUT
```

**Wiring:**
```
VCC (VDD)  → Pin 2 or Pin 4 (5V)
Data (DIN) → Pin 32 (GPIO 12)
GND (VSS)  → Pin 6, 9, or any GND
```

⚠️ **Important Notes:**
- The WS2812 runs on **5V power** but accepts 3.3V data signals
- GPIO 12 supports hardware PWM which is ideal for WS2812 timing
- If you have signal issues, add a 300-500Ω resistor in series with the data line
- Add a 100µF capacitor across VCC and GND near the LED if you see flickering

## Breadboard Layout Example

```
                Raspberry Pi Zero 2W
             ┌─────────────────────────┐
             │      [40-pin GPIO]      │
             │   PCM5102 DAC attached  │
             └───────────┬─────────────┘
                         │
    ┌────────────────────┼────────────────────┐
    │                                         │
    │           Breadboard / Perfboard        │
    │  ┌────────────────────────────────────┐ │
    │  │                                    │ │
    │  │  [Enc1] [Enc2] [Enc3] [Enc4] [Enc5]│ │
    │  │   LFO  B.Freq Filter Delay  Reverb │ │
    │  │                                    │ │
    │  │  [Trig] [Pitch] [Shift] [Shutdown]│ │
    │  │                                    │ │
    │  └────────────────────────────────────┘ │
    │                                         │
    └─────────────────────────────────────────┘
         3.3V, GND, and Signal connections
```

## Step-by-Step Assembly

### 1. Prepare Your Workspace
- **Power off** the Raspberry Pi
- Gather all components
- Use ESD protection (wrist strap recommended)

### 2. Connect Ground Rails
```
Pin 9  (GND)  → Breadboard - rail
Pin 39 (GND)  → Breadboard - rail (additional ground)
```
**Note:** No 3.3V rail needed - encoders use Pi's internal pull-ups.

### 3. Wire Encoders (in order)
Wire one encoder at a time and test:

```bash
# Encoder 1
CLK: GPIO 2 (Pin 3), DT: GPIO 17 (Pin 11)

# Encoder 2
CLK: GPIO 22 (Pin 15), DT: GPIO 27 (Pin 13)

# Encoder 3
CLK: GPIO 24 (Pin 18), DT: GPIO 23 (Pin 16)

# Encoder 4
CLK: GPIO 26 (Pin 37), DT: GPIO 20 (Pin 38)

# Encoder 5
CLK: GPIO 13 (Pin 33), DT: GPIO 14 (Pin 8)
```

### 4. Wire Buttons
```bash
Trigger:   GPIO 4  (Pin 7)  to GND
Shift:     GPIO 15 (Pin 10) to GND
Shutdown:  GPIO 3  (Pin 5)  to GND

# 3-Position Pitch Envelope Switch
Pitch UP:   GPIO 9  (Pin 21) to switch terminal 1
Pitch DOWN: GPIO 10 (Pin 19) to switch terminal 2
Common:     GND (Pin 20) to switch common terminal
```

### 5. Double-Check Connections
⚠️ **Critical checks before powering on:**
- [ ] No shorts between any GPIO pins
- [ ] No connections to GPIO 18, 19, or 21 (I2S pins)
- [ ] All encoder GND pins connected to common GND
- [ ] All button/switch GND pins connected to common GND

## Testing the Control Surface

### 1. Power On and Start Service
```bash
cd ~/poor-house-dub-v2
sudo systemctl start dubsiren.service
sudo journalctl -u dubsiren.service -f
```

### 2. Test Each Encoder (Bank A)

**Encoder 1 (LFO Depth):**
- Turn clockwise → Should see `[Bank A] lfo_depth: 0.542`
- Turn counter-clockwise → Should see `[Bank A] lfo_depth: 0.458`

**Encoder 2 (Base Frequency):**
- Turn → Should see `[Bank A] base_freq: XXX.XXX`

**Encoder 3 (Filter Frequency):**
- Turn → Should see `[Bank A] filter_freq: XXXX.000`

**Encoder 4 (Delay Feedback):**
- Turn → Should see `[Bank A] delay_feedback: 0.XXX`

**Encoder 5 (Reverb Mix):**
- Turn → Should see `[Bank A] reverb_mix: 0.XXX`

### 3. Test Shift Button (Bank B)

**Hold Shift and turn encoders:**
- Should see `Bank B active`
- Encoder 1 → `[Bank B] lfo_rate: X.XXX`
- Encoder 2 → `[Bank B] delay_time: X.XXX`
- Encoder 3 → `[Bank B] filter_res: 0.XXX`
- Encoder 4 → `[Bank B] osc_waveform: Sine/Square/Saw/Triangle`
- Encoder 5 → `[Bank B] reverb_size: 0.XXX`

**Release Shift:**
- Should see `Bank A active`
- Encoders return to Bank A parameters

### 4. Test Function Buttons

**Trigger Button:**
- Press and hold → Should hear dub siren sound
- Release → Sound stops

**Shutdown Button:**
- Press → System shuts down safely after 1 second

### 5. Test Pitch Envelope Switch

**3-Position Switch:**
- Flip UP → Should see `Pitch envelope: up (rise)`
- Flip to CENTER → Should see `Pitch envelope: none`
- Flip DOWN → Should see `Pitch envelope: down (fall)`

**Test with Trigger:**
- Set switch to UP, trigger sound → pitch rises on release
- Set switch to DOWN, trigger sound → pitch falls on release
- Set switch to CENTER, trigger sound → no pitch change on release

## Troubleshooting

### Encoder Not Responding
1. **Check wiring** - verify CLK and DT pins
2. **Test GPIO:**
   ```bash
   python3 -c "import RPi.GPIO as GPIO; GPIO.setmode(GPIO.BCM); \
   GPIO.setup(17, GPIO.IN, pull_up_down=GPIO.PUD_UP); \
   print('GPIO 17:', GPIO.input(17))"
   ```
   Should print `GPIO 17: 1`

3. **Swap CLK and DT** if direction is reversed

### Button Not Responding
1. **Check switch type** - must be normally open (NO)
2. **Test continuity** with multimeter
3. **Verify GPIO not shorted to ground**

### Shift Button Stuck
- Check for debris in switch
- Verify wiring (GPIO 15 to GND)
- Test switch with multimeter

### I2S Audio Conflict Error
```
RuntimeError: Failed to add edge detection
```
**Cause:** Using GPIO 18, 19, or 21 (reserved for I2S audio)
**Solution:** Verify pin assignments match this guide exactly

### Encoder Direction Reversed
- **Quick fix:** Swap CLK and DT wires
- **Code fix:** Edit `gpio_controller.py` encoder pin order

### Noisy/Jittery Encoders
- Add **0.1µF capacitors** between each signal pin and GND
- Use **shorter wires** (<15cm recommended)
- **Twist CLK/DT pairs** together to reduce noise
- Keep wires away from I2S audio signals

## Panel Mount Installation

For a permanent enclosure build:

### Recommended Layout
```
┌──────────────────────────────────┐
│                                  │
│  [1]    [2]    [3]    [4]   [5]  │  ← Encoders
│  LFO  B.Freq Filter Delay  Rev   │
│                                  │
│  (Trigger)   ↑|○|↓   (Shift)     │  ← Buttons + 3-pos switch
│              PITCH                │
│  [Power LED]        (Shutdown)   │
│                                  │
└──────────────────────────────────┘
```

### Tips
- Use **panel-mount encoders** with mounting nuts
- **Label each knob** with bank functions:
  - `LFO / RATE` (LFO Depth / LFO Rate)
  - `FREQ / DLY` (Base Freq / Delay Time)
  - `FILT / RES` (Filter Freq / Filter Res)
  - `FB / WAVE` (Delay Feedback / Osc Wave)
  - `MIX / SIZE` (Reverb Mix / Reverb Size)
- Mount **Shift button** in easy reach
- Mount **WS2812 LED** in a visible location for status indication
- Use **arcade buttons** for trigger (satisfying tactile feel)

## Shopping List

### Encoders (5x)
- **EC11** 5-pin rotary encoders (no VCC required)
- Encoder knobs (recommend 20mm diameter)
- No breakout boards or modules needed - direct wiring to Pi

### Buttons (3x) + Toggle Switch (1x)
- **Trigger:** Arcade button (30mm) or large tactile switch
- **Shift:** 6mm tactile switch or small arcade button
- **Shutdown:** Latching switch or recessed button (prevent accidents)
- **Pitch Envelope:** 3-position ON/OFF/ON toggle switch (SPDT, panel mount)

### Wiring
- **Male-to-female jumper wires** (20-pack, 15cm length)
- **Breadboard** (400-point) for prototyping, OR
- **Perfboard** (70x90mm) for permanent build

### Optional Status LED
- **WS2812D-F5** (5mm through-hole RGB LED with integrated controller)
  - Also known as: WS2812B-F5, NeoPixel 5mm
  - Only needs one data pin + 5V power
  - Shows startup status, mode indication, audio-reactive pulsing
- **100µF electrolytic capacitor** (for LED power filtering)
- **330Ω resistor** (optional, for data line protection)

### Optional Components
- **0.1µF ceramic capacitors** (10-pack) for encoder noise reduction (CLK/DT to GND)
- **Panel-mount encoder brackets**
- **Enclosure** (Hammond 1590DD or similar)

**Note:** Capacitors are usually not needed - software debouncing handles most noise. Add them only if you experience jittery encoder readings.

## Advanced: Custom Pin Configuration

To use different GPIO pins, edit `gpio_controller.py`:

```python
ENCODER_PINS = {
    'encoder_1': (2, 17),    # Change these
    'encoder_2': (22, 27),   # Change these
    'encoder_3': (24, 23),   # Change these
    'encoder_4': (26, 20),   # Change these
    'encoder_5': (13, 14),   # Change these
}

SWITCH_PINS = {
    'trigger': 4,      # Change these
    'pitch_env': 9,    # Change these
    'shift': 15,       # Change these
    'shutdown': 3,     # Change these
}
```

**Remember:** Avoid GPIOs 18, 19, 21 (I2S audio)!

## Safety Notes

⚠️ **Critical Safety:**
- **Double-check** all wiring before powering on
- **Avoid** GPIO 18, 19, 21 (will conflict with audio)
- **Use ESD protection** when handling components
- **Disconnect GPIO** during SD card flashing
- **Encoder GND only** - no power connection needed (internal pull-ups used)

## Next Steps

1. ✅ Wire according to this guide
2. ✅ Test with `sudo journalctl -u dubsiren.service -f`
3. ✅ Verify all encoders and buttons respond
4. ✅ Test shift button bank switching
5. ✅ Build enclosure for permanent installation
6. 🎵 Make dub music!

Happy building! 🎛️🔊
