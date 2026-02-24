#pragma once

#include "Common.h"

namespace DubSiren {

/**
 * Two-pole resonant low-pass filter using the Chamberlin State Variable Filter (SVF).
 *
 * A one-pole filter cannot produce resonance. The SVF uses two integrator states
 * (low-pass and band-pass) to form a 12dB/oct slope with a true resonant peak
 * at the cutoff frequency controlled by Q (the resonance parameter).
 *
 * Parameter smoothing prevents "zipper noise" and clicks when filter
 * parameters change rapidly (e.g., from rotary encoder adjustments).
 */
class LowPassFilter {
public:
    explicit LowPassFilter(int sampleRate = DEFAULT_SAMPLE_RATE);

    /**
     * Process audio through the filter.
     * @param input Input buffer
     * @param output Output buffer (can be same as input)
     * @param numSamples Number of samples to process
     */
    void process(const float* input, float* output, int numSamples);

    /**
     * Process a single sample (for sample-accurate processing)
     */
    float processSample(float input);

    // Parameter setters
    void setCutoff(float freq);
    void setResonance(float res);
    void reset();

    // Getters
    float getCutoff() const { return cutoff; }
    float getResonance() const { return resonance; }

private:
    int sampleRate;
    float cutoff;           // Target cutoff frequency
    float cutoffCurrent;    // Smoothed current cutoff
    float resonance;        // Target resonance (Q factor, 0.1–20)
    float resonanceCurrent; // Smoothed current resonance
    float lpState;          // SVF low-pass integrator state
    float bpState;          // SVF band-pass integrator state
    float smoothing;        // Smoothing coefficient
};

/**
 * DC blocking filter to remove DC offset.
 * 
 * DC offset can accumulate in feedback loops (filters, delay, reverb) and waste
 * headroom, leading to asymmetric clipping and pops. This first-order high-pass
 * filter at ~10Hz removes DC while preserving bass frequencies.
 */
class DCBlocker {
public:
    DCBlocker();
    
    void process(const float* input, float* output, int numSamples);
    float processSample(float input);
    void reset();
    
private:
    float xPrev;
    float yPrev;
    float coeff;  // High-pass at ~10Hz @ 48kHz
};

} // namespace DubSiren
