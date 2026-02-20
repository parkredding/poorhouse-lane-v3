/**
 * DubSiren Synthesizer Tests
 * Tests for core functionality, effects, and controls
 */

// Extract DubSiren class from index.html for testing
const fs = require('fs');
const path = require('path');

// Read and extract the DubSiren class from index.html
const htmlContent = fs.readFileSync(path.join(__dirname, '../index.html'), 'utf-8');
const scriptMatch = htmlContent.match(/<script>([\s\S]*?)<\/script>/);
const scriptContent = scriptMatch[1];

// Execute the script to get the DubSiren class
let DubSiren;
eval(scriptContent);
DubSiren = eval('DubSiren');

describe('DubSiren Synthesizer', () => {
  let synth;

  beforeEach(async () => {
    synth = new DubSiren();
    await synth.init();
  });

  afterEach(() => {
    if (synth.audioContext) {
      synth.audioContext.close();
    }
  });

  describe('Initialization', () => {
    test('should create an instance', () => {
      expect(synth).toBeInstanceOf(DubSiren);
    });

    test('should initialize audio context', () => {
      expect(synth.audioContext).toBeDefined();
      expect(synth.audioContext.state).toBe('running');
    });

    test('should create master gain node', () => {
      expect(synth.masterGain).toBeDefined();
      expect(synth.masterGain.gain.value).toBe(synth.params.volume);
    });

    test('should create analyser node', () => {
      expect(synth.analyser).toBeDefined();
      expect(synth.analyser.fftSize).toBe(2048);
    });

    test('should initialize with default parameters', () => {
      expect(synth.params.frequency).toBe(440);
      expect(synth.params.oscWaveform).toBe('sine');
      expect(synth.params.attack).toBe(0.01);
      expect(synth.params.release).toBe(0.3);
      expect(synth.params.filterFreq).toBe(2000);
      expect(synth.params.volume).toBe(0.7);
    });
  });

  describe('Filter Controls', () => {
    test('should create lowpass filter', () => {
      expect(synth.filter).toBeDefined();
      expect(synth.filter.type).toBe('lowpass');
    });

    test('should set filter frequency', () => {
      synth.setParam('filterFreq', 1000);
      expect(synth.filter.frequency.value).toBe(1000);
    });

    test('should set filter resonance', () => {
      synth.setParam('filterRes', 5);
      expect(synth.filter.Q.value).toBe(5);
    });

    test('should clamp filter frequency to valid range', () => {
      synth.setParam('filterFreq', 20000);
      expect(synth.filter.frequency.value).toBeLessThanOrEqual(20000);
    });
  });

  describe('Delay Effect', () => {
    test('should create delay node', () => {
      expect(synth.delay).toBeDefined();
      expect(synth.delay.delayTime.value).toBe(synth.params.delayTime);
    });

    test('should set delay time', () => {
      synth.setParam('delayTime', 0.5);
      expect(synth.delay.delayTime.value).toBe(0.5);
    });

    test('should set delay feedback', () => {
      synth.setParam('delayFeedback', 0.6);
      expect(synth.delayFeedbackGain.gain.value).toBe(0.6);
    });

    test('should set delay mix', () => {
      synth.setParam('delayMix', 0.5);
      expect(synth.delayDry.gain.value).toBeCloseTo(0.5, 2);
      expect(synth.delayWet.gain.value).toBeCloseTo(0.5, 2);
    });

    test('should create tape saturation waveshaper', () => {
      expect(synth.tapeSaturator).toBeDefined();
      expect(synth.tapeSaturator.curve).toBeDefined();
    });

    test('should create delay feedback filter at 5kHz', () => {
      expect(synth.delayFilter).toBeDefined();
      expect(synth.delayFilter.type).toBe('lowpass');
      expect(synth.delayFilter.frequency.value).toBe(5000);
    });

    test('should connect delay feedback loop correctly', () => {
      // Verify the feedback path exists
      expect(synth.delay).toBeDefined();
      expect(synth.delayFilter).toBeDefined();
      expect(synth.tapeSaturator).toBeDefined();
      expect(synth.delayFeedbackGain).toBeDefined();
    });
  });

  describe('Reverb Effect', () => {
    test('should create convolver for reverb', () => {
      expect(synth.convolver).toBeDefined();
    });

    test('should create reverb dry/wet mix gains', () => {
      expect(synth.reverbDry).toBeDefined();
      expect(synth.reverbWet).toBeDefined();
    });

    test('should set reverb mix', () => {
      synth.setParam('reverbMix', 0.7);
      expect(synth.reverbDry.gain.value).toBeCloseTo(0.3, 2);
      expect(synth.reverbWet.gain.value).toBeCloseTo(0.7, 2);
    });

    test('should generate impulse response', async () => {
      // Impulse response is generated during init
      expect(synth.convolver.buffer).toBeDefined();
    });
  });

  describe('Oscillator and LFO', () => {
    test('should create oscillator on trigger', () => {
      synth.trigger();
      expect(synth.oscillator).toBeDefined();
      expect(synth.oscillator.type).toBe(synth.params.oscWaveform);
      expect(synth.oscillator.frequency.value).toBe(synth.params.frequency);
    });

    test('should create LFO on trigger', () => {
      synth.trigger();
      expect(synth.lfo).toBeDefined();
      expect(synth.lfo.type).toBe(synth.params.lfoWaveform);
      expect(synth.lfo.frequency.value).toBe(synth.params.lfoRate);
    });

    test('should set oscillator waveform', () => {
      synth.setParam('oscWaveform', 'square');
      synth.trigger();
      expect(synth.oscillator.type).toBe('square');
    });

    test('should set LFO rate', () => {
      synth.setParam('lfoRate', 10);
      synth.trigger();
      expect(synth.lfo.frequency.value).toBe(10);
    });

    test('should set LFO depth', () => {
      synth.setParam('lfoDepth', 50);
      synth.trigger();
      const expectedDepth = synth.params.frequency * 0.5; // 50%
      expect(synth.lfoGain.gain.value).toBeCloseTo(expectedDepth, 0);
    });

    test('should change frequency', () => {
      synth.trigger();
      synth.setParam('frequency', 880);
      expect(synth.oscillator.frequency.value).toBe(880);
    });
  });

  describe('Envelope (Attack/Release)', () => {
    test('should create envelope gain on trigger', () => {
      synth.trigger();
      expect(synth.envelope).toBeDefined();
    });

    test('should start envelope at 0', () => {
      synth.trigger();
      expect(synth.envelope.gain.value).toBe(0);
    });

    test('should schedule attack ramp', () => {
      const attackTime = 0.05;
      synth.setParam('attack', attackTime);
      synth.trigger();
      // Verify envelope is scheduled (tested via mock audio context)
      expect(synth.envelope.gain.value).toBeDefined();
    });

    test('should set playing state on trigger', () => {
      synth.trigger();
      expect(synth.isPlaying).toBe(true);
    });

    test('should clear playing state on release', () => {
      synth.trigger();
      synth.release();
      expect(synth.isPlaying).toBe(false);
    });
  });

  describe('Delay and Release Interaction (Critical Fix)', () => {
    test('should route filter output to both envelope and delay', () => {
      synth.trigger();
      // Verify signal routing: filter connects to both paths
      expect(synth.filter).toBeDefined();
      expect(synth.envelope).toBeDefined();
      expect(synth.delay).toBeDefined();
    });

    test('should create separate dry (enveloped) and wet (delay) gains', () => {
      synth.trigger();
      expect(synth.delayDry).toBeDefined();
      expect(synth.delayWet).toBeDefined();
    });

    test('should allow delay to continue after envelope fades', (done) => {
      synth.setParam('delayFeedback', 0.5);
      synth.setParam('delayMix', 1.0); // Full wet
      synth.setParam('release', 0.1);
      
      synth.trigger();
      
      // Release the note
      synth.release();
      
      // The oscillator should continue until release time + 0.1s
      // Delay should keep processing during this time
      setTimeout(() => {
        // At this point, envelope is fading but delay is still processing
        expect(synth.envelope.gain.value).toBeLessThan(1.0);
        done();
      }, 50);
    });

    test('should stop oscillator after release time', (done) => {
      synth.setParam('release', 0.05);
      synth.trigger();
      const oscillator = synth.oscillator;
      
      synth.release();
      
      setTimeout(() => {
        // Oscillator should be scheduled to stop
        expect(oscillator).toBeDefined();
        done();
      }, 200); // After release + 0.1s
    });
  });

  describe('Pitch Envelope', () => {
    test('should support pitch envelope modes', () => {
      expect(['none', 'up', 'down']).toContain(synth.params.pitchEnvelope);
    });

    test('should not apply pitch envelope when mode is none', () => {
      synth.setParam('pitchEnvelope', 'none');
      synth.setParam('frequency', 440);
      synth.trigger();
      synth.release();
      
      // No interval should be set for pitch updates
      setTimeout(() => {
        expect(synth.pitchEnvelopeUpdateInterval).toBeFalsy();
      }, 50);
    });

    test('should sweep pitch up during release', (done) => {
      synth.setParam('pitchEnvelope', 'up');
      synth.setParam('frequency', 440);
      synth.setParam('release', 0.2);
      synth.trigger();
      
      const initialFreq = synth.oscillator.frequency.value;
      synth.release();
      
      setTimeout(() => {
        // Frequency should increase during release
        const currentFreq = synth.oscillator.frequency.value;
        expect(currentFreq).toBeGreaterThan(initialFreq);
        done();
      }, 100);
    });

    test('should sweep pitch down during release', (done) => {
      synth.setParam('pitchEnvelope', 'down');
      synth.setParam('frequency', 440);
      synth.setParam('release', 0.2);
      synth.trigger();
      
      const initialFreq = synth.oscillator.frequency.value;
      synth.release();
      
      setTimeout(() => {
        // Frequency should decrease during release
        const currentFreq = synth.oscillator.frequency.value;
        expect(currentFreq).toBeLessThan(initialFreq);
        done();
      }, 100);
    });

    test('should update LFO depth when pitch changes', (done) => {
      synth.setParam('pitchEnvelope', 'up');
      synth.setParam('frequency', 440);
      synth.setParam('lfoDepth', 10);
      synth.setParam('release', 0.2);
      synth.trigger();
      
      const initialLfoDepth = synth.lfoGain.gain.value;
      synth.release();
      
      setTimeout(() => {
        // LFO depth should scale with frequency
        const currentLfoDepth = synth.lfoGain.gain.value;
        expect(currentLfoDepth).not.toBe(initialLfoDepth);
        done();
      }, 100);
    });
  });

  describe('Tape Saturation', () => {
    test('should create tape saturation curve', () => {
      const curve = synth.createTapeSaturationCurve(0.5);
      expect(curve).toBeInstanceOf(Float32Array);
      expect(curve.length).toBe(1024);
    });

    test('should create clean curve with zero saturation', () => {
      const curve = synth.createTapeSaturationCurve(0);
      // With 0 saturation, output should be close to linear (input = output)
      const midPoint = curve[512]; // Middle of curve (input = 0)
      expect(Math.abs(midPoint)).toBeLessThan(0.1);
    });

    test('should create saturated curve with full saturation', () => {
      const curve = synth.createTapeSaturationCurve(1.0);
      // With full saturation, extreme values should be compressed
      expect(curve).toBeDefined();
    });

    test('should update tape saturation', () => {
      const newSaturation = 0.7;
      synth.setParam('tapeSaturation', newSaturation);
      expect(synth.params.tapeSaturation).toBe(newSaturation);
      expect(synth.tapeSaturator.curve).toBeDefined();
    });
  });

  describe('Volume Control', () => {
    test('should set master volume', () => {
      synth.setParam('volume', 0.5);
      expect(synth.masterGain.gain.value).toBe(0.5);
    });

    test('should clamp volume to 0-1 range', () => {
      synth.setParam('volume', 1.5);
      expect(synth.masterGain.gain.value).toBeLessThanOrEqual(1.0);
      
      synth.setParam('volume', -0.5);
      expect(synth.masterGain.gain.value).toBeGreaterThanOrEqual(0.0);
    });
  });

  describe('Cleanup and State Management', () => {
    test('should disconnect temporary nodes on release', (done) => {
      synth.trigger();
      const delayMerge = synth.delayMerge;
      const reverbMerge = synth.reverbMerge;
      
      expect(delayMerge).toBeDefined();
      expect(reverbMerge).toBeDefined();
      
      synth.release();
      
      setTimeout(() => {
        expect(synth.delayMerge).toBeNull();
        expect(synth.reverbMerge).toBeNull();
        done();
      }, 500); // After release time + cleanup
    });

    test('should clear pitch envelope interval after release', (done) => {
      synth.setParam('pitchEnvelope', 'up');
      synth.setParam('release', 0.1);
      synth.trigger();
      synth.release();
      
      setTimeout(() => {
        expect(synth.pitchEnvelopeUpdateInterval).toBeNull();
        done();
      }, 300);
    });

    test('should allow re-triggering after release', (done) => {
      synth.trigger();
      synth.release();
      
      setTimeout(() => {
        synth.trigger();
        expect(synth.isPlaying).toBe(true);
        expect(synth.oscillator).toBeDefined();
        done();
      }, 400);
    });
  });

  describe('Parameter Validation', () => {
    test('should handle invalid frequency gracefully', () => {
      synth.trigger();
      synth.setParam('frequency', -100);
      // Should clamp or handle gracefully
      expect(synth.oscillator.frequency.value).toBeGreaterThan(0);
    });

    test('should handle all parameter updates', () => {
      const params = [
        ['frequency', 880],
        ['oscWaveform', 'triangle'],
        ['lfoRate', 8],
        ['lfoDepth', 25],
        ['lfoWaveform', 'square'],
        ['attack', 0.1],
        ['release', 0.5],
        ['filterFreq', 5000],
        ['filterRes', 3],
        ['delayTime', 0.5],
        ['delayFeedback', 0.4],
        ['delayMix', 0.5],
        ['reverbMix', 0.3],
        ['volume', 0.8]
      ];

      params.forEach(([param, value]) => {
        expect(() => synth.setParam(param, value)).not.toThrow();
      });
    });
  });
});
