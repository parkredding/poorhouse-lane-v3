# Feature Restoration Plan
## Dub Siren V2 - Systematic Feature Re-integration

**Status:** Simple exponential envelope + DC blocker working perfectly ✅
**Goal:** Add back all features from original design without reintroducing clicks

---

## Phase 1: Core Envelope Improvements ✅ FOUNDATION COMPLETE

### ✅ Step 1.1: Simple Exponential Envelope (DONE)
- **Status:** Implemented and tested - NO CLICK!
- **Implementation:** First-order exponential filter
  ```python
  current_value += (target - current_value) * coefficient
  ```
- **Result:** Clean attack and release with no discontinuities

### ✅ Step 1.2: DC Blocker (DONE)
- **Status:** Already in place with correct coefficient (0.995)
- **Location:** Applied after envelope, before volume
- **Result:** Removes DC offset that could cause clicks

---

## Phase 2: Envelope Enhancements (SAFE ADDITIONS)

### 🔲 Step 2.1: Adjustable Release Time (Encoder 1, Bank B)
**Goal:** User control of envelope release duration
**Risk:** LOW - just changes coefficient value
**Implementation:**
```python
# In Envelope class
def set_release(self, release_time: float):
    """Set release time in seconds (0.001 to 5.0)"""
    self.release_time = max(0.001, min(release_time, 5.0))
    self.release_coeff = 1.0 / (self.release_time * self.sample_rate)
```
**Testing:**
- Verify no clicks at minimum release (0.001s)
- Verify no clicks at maximum release (5.0s)
- Verify smooth transitions when adjusting during playback

### 🔲 Step 2.2: Soft Retrigger
**Goal:** Smooth retriggering when pressing trigger before release completes
**Risk:** MEDIUM - adds state tracking
**Implementation:**
```python
def trigger(self):
    """Trigger with soft retrigger - start from current level"""
    # Don't reset current_value to 0, let it continue from wherever it is
    # The exponential approach to 1.0 will handle the transition smoothly
    self.is_active = True
```
**Testing:**
- Rapid trigger presses during release
- Verify no pops when retriggering
- Verify envelope smoothly transitions from current level to 1.0

---

## Phase 3: Effects Chain (DRY PATH → EFFECTS)

### 🔲 Step 3.1: Low-Pass Filter
**Goal:** Enable state variable filter for tone shaping
**Controls:**
- Bank A, Encoder 2: Filter Frequency (20Hz - 20kHz)
- Bank A, Encoder 3: Filter Resonance (0.0 - 0.95)

**Risk:** MEDIUM - filter state could accumulate DC offset
**Safety:** Filter already has internal clamping and NaN protection
**Implementation:**
```python
# In generate_audio(), after envelope
audio = audio * env
audio = self.filter.process(audio)  # ADD THIS
audio = self.dc_blocker.process(audio)
```
**Testing:**
- Test with resonance at 0.0 (no resonance)
- Test with resonance at 0.95 (maximum)
- Verify no clicks when releasing with filter engaged
- Verify filter doesn't ring or cause artifacts at release

### 🔲 Step 3.2: Delay Effect
**Goal:** Enable tape-style delay with feedback
**Controls:**
- Bank A, Encoder 4: Delay Feedback (0.0 - 0.95)
- Bank B, Encoder 2: Delay Time (0.001s - 2.0s)

**Risk:** HIGH - feedback loops can cause instability
**Safety:** Delay already has:
- Buffer clamping
- NaN protection
- Feedback limiting (max 0.95)

**Implementation:**
```python
# In generate_audio(), after filter
audio = self.filter.process(audio)
audio = self.delay.process(audio)  # ADD THIS
audio = self.dc_blocker.process(audio)
```
**Testing:**
- Test with feedback at 0.0 (no feedback)
- Test with feedback at 0.5 (medium)
- Test with feedback at 0.95 (maximum)
- Let delay tail continue after releasing trigger
- Verify no buildup or instability

### 🔲 Step 3.3: Reverb Effect
**Goal:** Enable Freeverb chamber reverb
**Controls:**
- Bank A, Encoder 5: Reverb Mix (0.0 - 1.0)
- Bank B, Encoder 3: Reverb Size (0.0 - 1.0)

**Risk:** MEDIUM - reverb has many buffers/states
**Safety:** Reverb already has NaN protection in comb/allpass filters
**Implementation:**
```python
# In generate_audio(), after delay
audio = self.delay.process(audio)
audio = self.reverb.process(audio)  # ADD THIS
audio = self.dc_blocker.process(audio)
```
**Testing:**
- Test with mix at 0.0 (dry)
- Test with mix at 1.0 (fully wet)
- Verify reverb tail decays cleanly
- No clicks when releasing during reverb tail

---

## Phase 4: Oscillator Enhancements

### 🔲 Step 4.1: Waveform Selection (Encoder 4, Bank B)
**Goal:** Switch between Sine, Square, Saw, Triangle
**Risk:** LOW - already implemented in Oscillator class
**Implementation:**
```python
# In DubSiren class
def set_oscillator_waveform(self, index: int):
    """Set oscillator waveform (0=sine, 1=square, 2=saw, 3=triangle)"""
    waveforms = ['sine', 'square', 'saw', 'triangle']
    self.oscillator.set_waveform(waveforms[index])
```
**Testing:**
- Switch waveforms while playing
- Verify no clicks when switching during release
- Verify PolyBLEP working on square/saw

---

## Phase 5: LFO System

### 🔲 Step 5.1: Basic LFO (Encoder 5, Bank B)
**Goal:** Enable LFO modulation of oscillator pitch
**Risk:** LOW - LFO is separate from envelope
**Implementation:**
```python
# In generate_audio(), modulate frequency
lfo_value = self.lfo.generate(1)[0]  # Get current LFO sample
freq_modulated = base_freq + (lfo_value * self.lfo.depth * 100.0)
self.oscillator.set_frequency(freq_modulated)
```
**Testing:**
- LFO depth at 0.0 (no modulation)
- LFO depth at 1.0 (full modulation)
- Verify smooth modulation
- No clicks when LFO is active during release

### 🔲 Step 5.2: LFO Waveform Selection
**Goal:** Cycle through LFO waveforms (sine, square, saw, triangle)
**Risk:** LOW - same as oscillator waveform
**Implementation:** Already exists in LFO class

---

## Phase 6: Pitch Envelope (OPTIONAL - COMPLEX)

### 🔲 Step 6.1: Pitch Envelope Modes (Button: Pitch Env)
**Goal:** Cycle through none/up/down pitch sweep modes
**Risk:** HIGH - this was previously disabled
**Implementation:** Complex - involves pitch modulation during release
**Decision:** DEFER until all other features working

---

## Phase 7: GPIO Control Surface Integration

### 🔲 Step 7.1: Encoder Reading
**Goal:** Read 5 rotary encoders via GPIO
**Risk:** LOW - isolated from audio
**Already Implemented:** gpio_controller.py has RotaryEncoder class

### 🔲 Step 7.2: Bank Switching (Button: Shift)
**Goal:** Hold shift to access Bank B parameters
**Risk:** LOW - just changes parameter routing
**Already Implemented:** gpio_controller.py has bank switching

### 🔲 Step 7.3: Wire Encoders to Parameters
**Goal:** Map encoder changes to synth parameters
**Implementation:**
```python
# Bank A (normal)
encoder_1 → set_volume()
encoder_2 → set_filter_frequency()
encoder_3 → set_filter_resonance()
encoder_4 → set_delay_feedback()
encoder_5 → set_reverb_dry_wet()

# Bank B (shift held)
encoder_1 → set_release_time()
encoder_2 → set_delay_time()
encoder_3 → set_reverb_size()
encoder_4 → set_oscillator_waveform()
encoder_5 → set_lfo_waveform()
```

---

## Testing Protocol (Apply to Each Phase)

### Pre-Implementation Checklist
- [ ] Feature code reviewed
- [ ] NaN protection verified
- [ ] DC offset handling confirmed
- [ ] No hard zero transitions

### Testing Sequence
1. **Idle test:** Feature at minimum setting, trigger on/off 10 times
2. **Mid-range test:** Feature at 50%, trigger on/off 10 times
3. **Maximum test:** Feature at maximum, trigger on/off 10 times
4. **Rapid retrigger test:** Press trigger rapidly during release
5. **Long release test:** Let envelope fully decay to silence
6. **Parameter sweep test:** Adjust parameter during playback

### Success Criteria
- ✅ No clicks at any point
- ✅ No pops during parameter changes
- ✅ Clean silence at end of release
- ✅ Smooth retriggering
- ✅ No NaN events in logs

### Failure Response
- Immediately revert feature
- Analyze what caused the click
- Redesign feature approach
- Re-test in isolation

---

## Implementation Order (Recommended)

1. ✅ **Phase 1:** Foundation (DONE - working!)
2. **Phase 2:** Envelope enhancements (2.1 → 2.2)
3. **Phase 3:** Effects in order (3.1 → 3.2 → 3.3)
4. **Phase 4:** Oscillator waveforms
5. **Phase 5:** LFO system (5.1 → 5.2)
6. **Phase 7:** GPIO integration
7. **Phase 6:** Pitch envelope (LAST - most complex)

---

## Rollback Strategy

If ANY phase introduces clicks:
1. `git revert HEAD` - immediately revert the commit
2. Analyze the feature in isolation
3. Test with minimal envelope only
4. Redesign feature to be click-free
5. Re-implement with extra safety

---

## Current Status Summary

**Working Foundation:**
- ✅ Simple exponential envelope (no ADSR complexity)
- ✅ DC blocker (0.995 coefficient)
- ✅ Clean attack and release
- ✅ No clicks or pops

**Next Implementation:**
- 🔲 Phase 2.1: Adjustable release time
- Estimated time: 15 minutes
- Risk level: LOW

**Total Features to Restore:**
- 2 envelope features
- 3 effects (filter, delay, reverb)
- 2 oscillator features
- 2 LFO features
- GPIO control surface
- = **10 features** across 7 phases

---

## Notes

- The working envelope is the foundation - DON'T CHANGE IT
- Add features ON TOP of the working envelope
- Test each feature in isolation before combining
- DC blocker stays in the signal chain at all times
- If a feature causes clicks, it's the FEATURE'S fault, not the envelope

---

## Success Definition

**Project Complete When:**
- All 10 parameters controllable via encoders
- Bank switching functional
- All 4 buttons working
- Zero clicks or pops under any condition
- Clean release to silence every time
- Performance suitable for live dub music production
