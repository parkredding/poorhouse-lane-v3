#pragma once

#include "Common.h"

namespace DubSiren {

/**
 * Audio oscillator with multiple waveform types and PolyBLEP anti-aliasing.
 * 
 * PolyBLEP (Polynomial Band-Limited Step) is applied to square and sawtooth
 * waveforms to reduce aliasing artifacts. This is especially important at
 * higher frequencies where harmonics would otherwise fold back into the
 * audible range.
 */
class Oscillator {
public:
    explicit Oscillator(int sampleRate = DEFAULT_SAMPLE_RATE);
    
    /**
     * Generate audio samples for the current waveform.
     * @param output Buffer to fill with samples
     * @param numSamples Number of samples to generate
     */
    void generate(float* output, int numSamples);
    
    /**
     * Generate a single sample (for sample-accurate processing)
     */
    float generateSample();
    
    // Parameter setters
    void setFrequency(float freq);
    void setWaveform(Waveform waveform);
    void resetPhase();
    
    // Getters
    float getFrequency() const { return frequency; }
    Waveform getWaveform() const { return waveform; }
    float getPhase() const { return phase; }
    
private:
    int sampleRate;
    float frequency;
    float phase;  // Phase accumulator (0.0 to 1.0)
    Waveform waveform;
    
    // PolyBLEP helper function
    float polyBlep(float t, float dt) const;
    
    // Waveform generators
    float generateSine();
    float generateSquarePolyBlep();
    float generateSawPolyBlep();
    float generateTriangle();
};

} // namespace DubSiren
