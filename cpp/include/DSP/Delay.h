#pragma once

#include "Common.h"
#include <vector>

namespace DubSiren {

/**
 * Tape-style Delay/Echo effect with authentic analog behavior.
 * 
 * Features:
 * - Dry/Wet mix control
 * - Tape-style high-frequency damping (5kHz LP) for natural degradation
 * - High-pass filter in feedback path (removes mud buildup)
 * - Tape saturation for warmth and harmonic richness
 * - Dual time modulation: slow wobble + fast flutter for tape character
 * - Analog repitch behavior: changing delay time causes pitch-shifting
 */
class DelayEffect {
public:
    explicit DelayEffect(int sampleRate = DEFAULT_SAMPLE_RATE, float maxDelay = 2.0f);
    
    /**
     * Process audio through the delay.
     * @param input Input buffer
     * @param output Output buffer (can be same as input)
     * @param numSamples Number of samples to process
     */
    void process(const float* input, float* output, int numSamples);
    
    // Parameter setters
    void setDelayTime(float timeSeconds);
    void setFeedback(float feedback);
    void setDryWet(float mix);
    void setRepitchRate(float rate);
    void setModDepth(float depth);
    void setModRate(float rate);
    void setTapeSaturation(float amount);
    
    // Getters
    float getDelayTime() const { return delayTime; }
    float getFeedback() const { return feedback; }
    float getDryWet() const { return dryWet; }
    
private:
    int sampleRate;
    int maxDelaySamples;
    std::vector<float> buffer;
    int writePos;
    
    // Core parameters
    float delayTime;     // Target delay time in seconds
    float feedback;      // 0.0 to 1.0
    float dryWet;        // 0.0 = dry, 1.0 = wet
    
    // Analog repitch behavior
    float currentDelaySamples;  // Actual read offset (smoothed)
    float repitchRate;          // 0.0 = instant, 1.0 = slow pitch shift
    float slewRate;             // Calculated from repitchRate
    
    // Feedback filters
    float filterHpFreq;
    float filterLpFreq;
    float hpState;
    float lpState;
    
    // Time modulation (wobble)
    float modDepth;
    float modRate;
    float modPhase;
    
    // Flutter modulation
    float flutterDepth;
    float flutterRate;
    float flutterPhase;
    
    // Saturation
    float tapeSaturation;
    
    // Internal methods
    float calculateSlewRate() const;
    float processFeedbackFilters(float sample);
    float lerpRead(float delaySamples) const;
};

} // namespace DubSiren
