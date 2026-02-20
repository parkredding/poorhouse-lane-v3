# PCM5102 DAC Troubleshooting Guide

## Symptom: speaker-test runs successfully but no audio output

If `speaker-test -t wav -c 2 -D hw:0,0 -l 1` completes without errors but you hear no audio in your headphones/speakers, the issue is almost certainly with the PCM5102 DAC hardware setup.

## Step 1: Verify Headphones/Speakers Connection

**CRITICAL:** Make sure your headphones or speakers are connected to the **PCM5102 DAC output**, NOT the Raspberry Pi's onboard 3.5mm jack (which is disabled).

The PCM5102 has:
- **OUT L** - Left channel output
- **OUT R** - Right channel output
- **GND** - Audio ground

Connect your headphones/speakers to these outputs on the DAC board.

## Step 2: Check I2S Data Pin Wiring

Verify these connections are secure and correct:

| PCM5102 Pin | Raspberry Pi Physical Pin | GPIO Number | Function |
|-------------|---------------------------|-------------|----------|
| **LCK** (LRCLK) | Pin 12 | GPIO 18 | Word clock (left/right) |
| **BCK** (BCLK) | Pin 35 | GPIO 19 | Bit clock |
| **DIN** (DATA) | Pin 40 | GPIO 21 | Data input |

### Common Wiring Mistakes:

1. **Swapped pins** - BCK and DIN are close together (pins 35 and 40)
2. **Loose connections** - Dupont wires can come loose
3. **Wrong GPIO numbering** - Make sure you're using physical pin numbers, not BCM GPIO numbers

### Pin Location Reference:

```
Raspberry Pi Zero 2 GPIO Header (Top View)
Looking at the Pi with the GPIO pins at the top:

Left Side (Odd Numbers):          Right Side (Even Numbers):
 1  3.3V  ●●  2  5V
 3  GPIO2 ●●  4  5V
 5  GPIO3 ●●  6  GND  ← PCM5102 GND
 7  GPIO4 ●●  8  GPIO14
 9  GND   ●●  10 GPIO15
11  GPIO17●●  12 GPIO18 ← PCM5102 LCK (LRCLK)
13  GPIO27●●  14 GND
15  GPIO22●●  16 GPIO23
17  3.3V  ●●  18 GPIO24
19  GPIO10●●  20 GND
21  GPIO9 ●●  22 GPIO25
23  GPIO11●●  24 GPIO8
25  GND   ●●  26 GPIO7
27  GPIO0 ●●  28 GPIO1
29  GPIO5 ●●  30 GND
31  GPIO6 ●●  32 GPIO12
33  GPIO13●●  34 GND
35  GPIO19●●  36 GPIO16 ← PCM5102 BCK (BCLK) goes to pin 35, not 36!
37  GPIO26●●  38 GPIO20
39  GND   ●●  40 GPIO21 ← PCM5102 DIN (DATA)
```

**Important:** Pin 35 (GPIO19/BCK) is on the LEFT side, Pin 40 (GPIO21/DIN) is on the RIGHT side.

## Step 3: Check Power Connections

| PCM5102 Pin | Raspberry Pi Connection |
|-------------|-------------------------|
| **VIN** | Pin 1 (3.3V) |
| **GND** | Pin 6 (GND) or any other GND pin |

### Visual Check:
- Does the PCM5102 have a power LED? Is it lit?
- If no LED is lit, check VIN and GND connections

## Step 4: Verify PCM5102 Configuration Pins ⚠️ CRITICAL

The PCM5102 has several configuration pins that **must** be set correctly. Many PCM5102 boards come with solder jumpers that pre-configure these, but some require manual wiring.

### Required Configuration:

| PCM5102 Pin | Must Connect To | Purpose | **If Wrong** |
|-------------|-----------------|---------|--------------|
| **XMT** | **GND** | **Soft mute control** | **⚠️ If HIGH = NO AUDIO!** |
| **SCK** | GND | Sets sample rate mode (GND = 48kHz) | Wrong sample rate |
| **FLT** | GND | Filter select (GND = normal latency) | Audio quality issues |
| **FMT** | GND | Format select (GND = I2S standard) | No audio/wrong format |

### ⚠️ XMT Pin is the Most Common Cause of "No Audio"!

**XMT (Soft Mute) behavior:**
- **XMT = HIGH (3.3V)** → DAC is **MUTED** → **NO AUDIO OUTPUT**
- **XMT = LOW (GND)** → DAC is **UNMUTED** → Normal operation
- **XMT = Floating** → Usually unmuted, but not guaranteed

**IMPORTANT:** If XMT is connected to 3.3V or pulled high by a jumper, speaker-test will run successfully but you'll hear no audio because the DAC is muted!

**Fix:** Connect XMT to GND to guarantee unmuted operation.

### How to check:
1. Look at your PCM5102 module
2. Find these pins (they may be labeled)
3. Check if there are solder jumpers on the back of the board
4. If no jumpers, you need to wire these pins to GND

### Common PCM5102 Module Types:

**Type 1: Pre-configured with solder jumpers** (most common)
- These boards have SCK, FLT, FMT already soldered to GND
- You only need to connect VIN, GND, and the three I2S pins (LCK, BCK, DIN)

**Type 2: Requires manual configuration**
- All pins are exposed
- You must manually wire SCK→GND, FLT→GND, FMT→GND

## Step 5: Test with Multimeter (Optional)

If you have a multimeter:

1. **Power check:**
   - Measure voltage between PCM5102 VIN and GND
   - Should read ~3.3V when Pi is powered on

2. **I2S signal check (requires oscilloscope or logic analyzer):**
   - BCK should show a clock signal when speaker-test is running
   - LCK should show a slower clock signal (word clock)
   - DIN should show data

## Step 6: Try a Different PCM5102 Module

If all wiring looks correct:
- The PCM5102 module itself may be defective
- Try a different module if available
- Some cheap modules have quality control issues

## Step 7: Verify I2S is Enabled

Run the audio diagnostics script:

```bash
cd ~/poor-house-dub-v2
./audio_diagnostics.sh
```

Check that the output shows:
```
dtparam=i2s=on
dtoverlay=hifiberry-dac
```

If these are missing, re-run setup:
```bash
./setup.sh
sudo reboot
```

## Common Issues and Solutions

### Issue: "No sound but speaker-test runs successfully" ⚠️ MOST COMMON
**Possible Causes (in order of likelihood):**

1. **XMT pin is HIGH (muted)** ← **CHECK THIS FIRST!**
   - Solution: Connect XMT to GND, or check for jumpers pulling it high

2. **Headphones on wrong output**
   - Solution: Connect to PCM5102 OUT L/R pins, NOT Pi's built-in jack

3. **I2S pins not connected properly**
   - Solution: Verify LCK→Pin 12, BCK→Pin 35, DIN→Pin 40

### Issue: "Hearing audio on ground wire" or "Audio without PCM5102 powered"
**Cause:** Pi's onboard audio is still enabled and bleeding signal through ground
**This means you're NOT hearing the I2S DAC!**
**Solution:**
1. Run the disable script:
   ```bash
   cd ~/poor-house-dub-v2
   ./disable_onboard_audio.sh
   sudo reboot
   ```
2. Verify onboard audio is disabled:
   ```bash
   grep "dtparam=audio" /boot/firmware/config.txt  # Should show: audio=off
   lsmod | grep snd_bcm2835  # Should be empty (driver not loaded)
   ```
3. After reboot, connect headphones to PCM5102 output pins (not Pi's jack)

### Issue: "Crackling or distorted audio"
**Cause:** Poor power supply or loose connections
**Solution:**
- Use 5V 2.5A power supply
- Check all connections are secure
- Add bulk capacitor (100μF) near PCM5102 VIN if needed

### Issue: "Audio only in one channel"
**Cause:** LCK (word clock) not connected
**Solution:** Verify LCK connection to GPIO 18 (Pin 12)

### Issue: "No audio at all"
**Causes:**
1. Headphones not connected to DAC output → Connect to PCM5102 OUT L/R/GND
2. I2S pins not connected → Verify LCK, BCK, DIN wiring
3. Config pins not set → Ensure SCK, FLT, FMT tied to GND
4. Defective module → Try different PCM5102 board

## Testing Order

Follow this order to isolate the problem:

1. ✓ Software working (speaker-test completes) ← **You are here**
2. ⚠ **Check XMT pin → Must be GND (not 3.3V or floating)** ← **CHECK FIRST!**
3. ⚠ Check headphones connected to DAC output (not Pi's jack)
4. ⚠ Check other config pins (SCK, FLT, FMT → GND)
5. ⚠ Verify I2S data pins (LCK→Pin 12, BCK→Pin 35, DIN→Pin 40)
6. ⚠ Verify power connections (VIN → 3.3V, GND → GND)
7. ⚠ Test with different headphones/speakers
8. ⚠ Try different PCM5102 module

## Additional Resources

- **Full hardware guide:** [HARDWARE.md](HARDWARE.md)
- **GPIO wiring:** [GPIO_WIRING_GUIDE.md](GPIO_WIRING_GUIDE.md)
- **Audio diagnostics script:** `./audio_diagnostics.sh`

## Still Not Working?

If you've verified all of the above and still have no audio:

1. Take photos of your wiring
2. Run the diagnostic script: `./audio_diagnostics.sh > diagnostics.txt`
3. Open an issue at: https://github.com/parkredding/poor-house-dub-v2/issues

Include:
- Photos of PCM5102 wiring
- Output of diagnostic script
- PCM5102 module model/brand
- Any error messages
