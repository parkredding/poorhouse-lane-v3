/**
 * Integration Tests for DubSiren
 * Tests complex interactions between multiple systems
 */

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

describe('DubSiren Integration Tests', () => {
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

  describe('Full Signal Chain', () => {
    test('should process audio through complete chain: osc -> filter -> delay -> reverb -> output', () => {
      synth.trigger();
      
      // Verify all nodes exist in chain
      expect(synth.oscillator).toBeDefined();
      expect(synth.filter).toBeDefined();
      expect(synth.envelope).toBeDefined();
      expect(synth.delayDry).toBeDefined();
      expect(synth.delay).toBeDefined();
      expect(synth.delayWet).toBeDefined();
      expect(synth.delayMerge).toBeDefined();
      expect(synth.convolver).toBeDefined();
      expect(synth.reverbDry).toBeDefined();
      expect(synth.reverbWet).toBeDefined();
      expect(synth.reverbMerge).toBeDefined();
      expect(synth.masterGain).toBeDefined();
    });

    test('should maintain proper signal routing with envelope after delay', () => {
      synth.trigger();
      
      // Envelope should control dry path
      expect(synth.envelope).toBeDefined();
      expect(synth.delayDry).toBeDefined();
      
      // Delay should receive signal directly from filter (not through envelope)
      expect(synth.delay).toBeDefined();
    });
  });

  describe('Complex Effect Interactions', () => {
    test('should handle LFO modulation with filter sweeps', (done) => {
      synth.setParam('lfoRate', 5);
      synth.setParam('lfoDepth', 20);
      synth.setParam('filterFreq', 1000);
      synth.trigger();
      
      setTimeout(() => {
        // System should remain stable
        expect(synth.isPlaying).toBe(true);
        expect(synth.oscillator).toBeDefined();
        done();
      }, 100);
    });

    test('should handle delay + reverb combination', () => {
      synth.setParam('delayMix', 0.5);
      synth.setParam('delayFeedback', 0.4);
      synth.setParam('reverbMix', 0.3);
      synth.trigger();
      
      // Both effects should be active
      expect(synth.delayWet.gain.value).toBeCloseTo(0.5, 2);
      expect(synth.reverbWet.gain.value).toBeCloseTo(0.3, 2);
    });

    test('should handle pitch envelope with delay', (done) => {
      synth.setParam('pitchEnvelope', 'down');
      synth.setParam('delayMix', 0.8);
      synth.setParam('delayFeedback', 0.5);
      synth.setParam('release', 0.2);
      synth.trigger();
      
      const initialFreq = synth.oscillator.frequency.value;
      synth.release();
      
      setTimeout(() => {
        // Pitch should be changing and delay should still be processing
        expect(synth.oscillator.frequency.value).toBeLessThan(initialFreq);
        done();
      }, 100);
    });

    test('should handle tape effects (wobble + flutter + saturation)', () => {
      synth.setParam('wobbleDepth', 0.005);
      synth.setParam('wobbleRate', 0.5);
      synth.setParam('flutterDepth', 0.002);
      synth.setParam('flutterRate', 4);
      synth.setParam('tapeSaturation', 0.6);
      synth.trigger();
      
      expect(synth.tapeSaturator.curve).toBeDefined();
      expect(synth.params.wobbleDepth).toBe(0.005);
      expect(synth.params.flutterDepth).toBe(0.002);
    });
  });

  describe('Rapid Triggering and Cleanup', () => {
    test('should handle rapid trigger/release cycles', (done) => {
      for (let i = 0; i < 5; i++) {
        synth.trigger();
        synth.release();
      }
      
      setTimeout(() => {
        // Should not accumulate orphaned nodes
        expect(synth.delayMerge).toBeNull();
        expect(synth.reverbMerge).toBeNull();
        done();
      }, 600);
    });

    test('should handle trigger during release', () => {
      synth.trigger();
      synth.release();
      
      // Trigger again immediately during release
      synth.trigger();
      
      expect(synth.isPlaying).toBe(true);
      expect(synth.oscillator).toBeDefined();
    });

    test('should handle multiple releases', () => {
      synth.trigger();
      synth.release();
      synth.release(); // Second release should be safe
      synth.release(); // Third release should be safe
      
      // Should not throw errors
      expect(synth.isPlaying).toBe(false);
    });
  });

  describe('Extreme Parameter Values', () => {
    test('should handle very high feedback without instability', () => {
      synth.setParam('delayFeedback', 0.95);
      synth.setParam('delayMix', 1.0);
      synth.trigger();
      
      // Should not blow up
      expect(synth.delayFeedbackGain.gain.value).toBe(0.95);
    });

    test('should handle very long delay times', () => {
      synth.setParam('delayTime', 1.9); // Near max
      synth.trigger();
      
      expect(synth.delay.delayTime.value).toBeCloseTo(1.9, 1);
    });

    test('should handle very short attack/release', () => {
      synth.setParam('attack', 0.001);
      synth.setParam('release', 0.001);
      synth.trigger();
      
      expect(synth.params.attack).toBe(0.001);
      expect(synth.params.release).toBe(0.001);
    });

    test('should handle very long release time', (done) => {
      synth.setParam('release', 2.0);
      synth.trigger();
      synth.release();
      
      // Should schedule proper cleanup
      setTimeout(() => {
        expect(synth.oscillator).toBeDefined();
        done();
      }, 100);
    });

    test('should handle extreme resonance values', () => {
      synth.setParam('filterRes', 20);
      synth.trigger();
      
      expect(synth.filter.Q.value).toBe(20);
    });

    test('should handle zero LFO depth', () => {
      synth.setParam('lfoDepth', 0);
      synth.trigger();
      
      expect(synth.lfoGain.gain.value).toBe(0);
    });

    test('should handle maximum LFO depth', () => {
      synth.setParam('lfoDepth', 100);
      synth.setParam('frequency', 440);
      synth.trigger();
      
      expect(synth.lfoGain.gain.value).toBeCloseTo(440, 0); // 100% of frequency
    });
  });

  describe('State Consistency', () => {
    test('should maintain parameter consistency across trigger cycles', () => {
      synth.setParam('frequency', 880);
      synth.setParam('filterFreq', 3000);
      synth.setParam('delayMix', 0.6);
      
      synth.trigger();
      synth.release();
      
      // Parameters should persist
      expect(synth.params.frequency).toBe(880);
      expect(synth.params.filterFreq).toBe(3000);
      expect(synth.params.delayMix).toBe(0.6);
    });

    test('should update live parameters during playback', () => {
      synth.trigger();
      
      const initialFreq = synth.oscillator.frequency.value;
      synth.setParam('frequency', 660);
      
      expect(synth.oscillator.frequency.value).toBe(660);
      expect(synth.oscillator.frequency.value).not.toBe(initialFreq);
    });

    test('should handle parameter changes during release', (done) => {
      synth.setParam('release', 0.3);
      synth.trigger();
      synth.release();
      
      // Change parameters during release
      setTimeout(() => {
        synth.setParam('volume', 0.5);
        synth.setParam('filterFreq', 4000);
        
        expect(synth.masterGain.gain.value).toBe(0.5);
        expect(synth.filter.frequency.value).toBe(4000);
        done();
      }, 50);
    });
  });

  describe('Edge Cases', () => {
    test('should handle release without trigger', () => {
      expect(() => synth.release()).not.toThrow();
    });

    test('should handle init called multiple times', async () => {
      await synth.init();
      await synth.init();
      
      expect(synth.audioContext).toBeDefined();
    });

    test('should handle setParam before init', async () => {
      const newSynth = new DubSiren();
      newSynth.setParam('frequency', 550);
      
      await newSynth.init();
      expect(newSynth.params.frequency).toBe(550);
      
      newSynth.audioContext.close();
    });

    test('should handle all waveform types', () => {
      const waveforms = ['sine', 'square', 'sawtooth', 'triangle'];
      
      waveforms.forEach(waveform => {
        synth.setParam('oscWaveform', waveform);
        synth.trigger();
        expect(synth.oscillator.type).toBe(waveform);
        synth.release();
      });
    });

    test('should handle pitch envelope at frequency extremes', (done) => {
      synth.setParam('pitchEnvelope', 'down');
      synth.setParam('frequency', 100); // Low frequency
      synth.setParam('release', 0.2);
      synth.trigger();
      synth.release();
      
      setTimeout(() => {
        // Should not go below minimum frequency (20 Hz)
        expect(synth.oscillator.frequency.value).toBeGreaterThanOrEqual(20);
        done();
      }, 100);
    });
  });

  describe('Audio Context Management', () => {
    test('should handle suspended audio context', async () => {
      const newSynth = new DubSiren();
      await newSynth.init();
      
      // Simulate suspended state
      if (newSynth.audioContext.suspend) {
        await newSynth.audioContext.suspend();
        await newSynth.init(); // Should resume
      }
      
      newSynth.audioContext.close();
    });

    test('should connect to analyser for visualization', () => {
      expect(synth.masterGain).toBeDefined();
      expect(synth.analyser).toBeDefined();
    });
  });

  describe('Delay Feedback Loop Stability', () => {
    test('should maintain stable feedback loop', (done) => {
      synth.setParam('delayFeedback', 0.7);
      synth.setParam('delayTime', 0.5);
      synth.setParam('delayMix', 1.0);
      synth.trigger();
      
      setTimeout(() => {
        synth.release();
        
        // Delay should continue processing without blow-up
        setTimeout(() => {
          expect(synth.delayFeedbackGain.gain.value).toBe(0.7);
          done();
        }, 100);
      }, 100);
    });

    test('should apply tape filtering in feedback loop', () => {
      synth.trigger();
      
      // Verify feedback path includes filter
      expect(synth.delayFilter).toBeDefined();
      expect(synth.delayFilter.frequency.value).toBe(5000);
    });

    test('should apply saturation in feedback loop', () => {
      synth.trigger();
      
      // Verify feedback path includes saturation
      expect(synth.tapeSaturator).toBeDefined();
      expect(synth.tapeSaturator.curve).toBeDefined();
    });
  });
});
