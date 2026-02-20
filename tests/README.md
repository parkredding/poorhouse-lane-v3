# DubSiren Tests

Comprehensive test suite for the DubSiren synthesizer, covering all effects, controls, and critical functionality.

## Test Coverage

### Core Functionality (`dubsiren.test.js`)
- **Initialization**: Audio context, nodes, and default parameters
- **Filter Controls**: Frequency, resonance, and parameter validation
- **Delay Effect**: Time, feedback, mix, tape saturation, and feedback loop
- **Reverb Effect**: Impulse response, dry/wet mix
- **Oscillator & LFO**: Waveforms, frequency, modulation depth
- **Envelope**: Attack, release, and amplitude control
- **Delay/Release Interaction**: Critical test ensuring delay continues during release
- **Pitch Envelope**: Up/down sweeps during release with LFO scaling
- **Tape Saturation**: Waveshaping curves and feedback processing
- **Volume Control**: Master gain and parameter clamping
- **Cleanup**: Proper node disconnection and state management
- **Parameter Validation**: Edge cases and invalid inputs

### Integration Tests (`integration.test.js`)
- **Full Signal Chain**: Complete audio path verification
- **Complex Effect Interactions**: LFO + filter, delay + reverb, pitch + delay
- **Rapid Triggering**: Multiple trigger/release cycles, cleanup verification
- **Extreme Parameters**: High feedback, long delays, extreme resonance
- **State Consistency**: Parameter persistence, live updates
- **Edge Cases**: Release without trigger, multiple inits, waveform types
- **Audio Context Management**: Suspended state handling
- **Delay Feedback Stability**: Preventing audio blow-up in feedback loops

## Key Tests for Recent Fix

The test suite includes specific tests for the delay/release interaction fix:

```javascript
test('should route filter output to both envelope and delay')
test('should create separate dry (enveloped) and wet (delay) gains')
test('should allow delay to continue after envelope fades')
```

These tests verify that:
1. The filter output splits into dry (enveloped) and wet (delay) paths
2. The envelope controls only the dry signal
3. The delay receives full-volume input until the oscillator stops
4. Delay feedback continues naturally even during release

## Running Tests

Install dependencies:
```bash
npm install
```

Run all tests:
```bash
npm test
```

Run tests in watch mode (auto-rerun on changes):
```bash
npm run test:watch
```

Generate coverage report:
```bash
npm run test:coverage
```

## Test Structure

Tests use:
- **Jest** as the test framework
- **jsdom** for browser environment simulation
- **web-audio-test-api** for Web Audio API mocking

The test setup (`setup.js`) mocks the Web Audio API, including:
- `AudioContext` for audio processing
- `OfflineAudioContext` for reverb impulse response generation
- All Web Audio nodes (oscillators, filters, delays, etc.)

## Writing New Tests

When adding new features, follow this pattern:

```javascript
describe('Feature Name', () => {
  test('should do something specific', () => {
    // Arrange: Set up parameters
    synth.setParam('paramName', value);
    
    // Act: Trigger the feature
    synth.trigger();
    
    // Assert: Verify behavior
    expect(synth.someNode).toBeDefined();
  });
});
```

For timing-dependent tests (release, envelopes), use `done` callback:

```javascript
test('should fade during release', (done) => {
  synth.trigger();
  synth.release();
  
  setTimeout(() => {
    expect(synth.envelope.gain.value).toBeLessThan(1.0);
    done();
  }, 100);
});
```

## Continuous Integration

These tests can be integrated into CI/CD pipelines to ensure:
- All effects and controls work correctly
- No regressions are introduced
- Critical audio routing remains correct
- Parameter validation prevents invalid states

## Known Limitations

- Tests use mocked Web Audio API, not real audio processing
- Timing tests rely on `setTimeout` and may be flaky in slow environments
- Actual audio output quality cannot be tested (requires human listening)
- Some browser-specific behaviors may not be caught

## Future Improvements

- [ ] Add performance benchmarks for audio processing
- [ ] Test audio buffer contents for expected waveforms
- [ ] Add visual regression tests for UI components
- [ ] Test MIDI input integration (if added)
- [ ] Add E2E tests with real audio context
