#pragma once

#include "Common.h"

namespace DubSiren {

/**
 * Low Frequency Oscillator for modulation.
 * 
 * Generates modulation signals for filter cutoff, pitch, etc.
 */
class LFO {
public:
    explicit LFO(int sampleRate = DEFAULT_SAMPLE_RATE);
    
    /**
     * Generate LFO modulation signal.
     * @param output Buffer to fill with modulation values (-1.0 to 1.0 scaled by depth)
     * @param numSamples Number of samples to generate
     */
    void generate(float* output, int numSamples);
    
    /**
     * Generate a single LFO sample
     */
    float generateSample();
    
    // Parameter setters
    void setFrequency(float freq);
    void setWaveform(Waveform waveform);
    void setDepth(float depth);
    
    // Getters
    float getFrequency() const { return frequency; }
    Waveform getWaveform() const { return waveform; }
    float getDepth() const { return depth; }
    
private:
    int sampleRate;
    float frequency;  // LFO rate in Hz
    float phase;      // Phase accumulator
    Waveform waveform;
    float depth;      // Modulation depth (0.0 to 1.0)
};

} // namespace DubSiren
