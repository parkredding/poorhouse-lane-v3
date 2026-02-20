#pragma once

#include "Common.h"
#include <atomic>

namespace DubSiren {

/**
 * Simple exponential envelope generator.
 * 
 * Uses first-order exponential approach to target values,
 * matching the reference dub siren implementation.
 */
class Envelope {
public:
    explicit Envelope(int sampleRate = DEFAULT_SAMPLE_RATE);
    
    /**
     * Generate envelope values into a buffer.
     * @param output Buffer to fill with envelope values
     * @param numSamples Number of samples to generate
     */
    void generate(float* output, int numSamples);
    
    /**
     * Generate a single envelope sample (for sample-accurate processing)
     */
    float generateSample();
    
    /**
     * Trigger the envelope (start attack phase)
     */
    void trigger();
    
    /**
     * Release the envelope (start release phase)
     */
    void release();
    
    // Parameter setters
    void setAttack(float timeSeconds);
    void setRelease(float timeSeconds);
    
    // Getters
    float getAttack() const { return attackTime; }
    float getRelease() const { return releaseTime; }
    float getCurrentValue() const { return currentValue; }
    bool isActive() const { return active.load(); }
    
private:
    int sampleRate;
    float attackTime;   // Attack time in seconds
    float releaseTime;  // Release time in seconds
    float attackCoeff;  // Calculated coefficient for attack
    float releaseCoeff; // Calculated coefficient for release
    float currentValue; // Current envelope value
    std::atomic<bool> active;  // Whether envelope is in attack phase (thread-safe)
    
    void updateCoefficients();
};

} // namespace DubSiren
