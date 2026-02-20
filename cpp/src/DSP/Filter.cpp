#include "DSP/Filter.h"
#include <cmath>
#include <algorithm>

namespace DubSiren {

// ============================================================================
// LowPassFilter Implementation
// ============================================================================

LowPassFilter::LowPassFilter(int sampleRate)
    : sampleRate(sampleRate)
    , cutoff(3000.0f)  // Standard filter setting for siren
    , cutoffCurrent(3000.0f)
    , resonance(1.0f)
    , resonanceCurrent(1.0f)
    , prevOutput(0.0f)
    , smoothing(0.05f)  // Increased from 0.001 to allow LFO modulation to be audible
{
}

void LowPassFilter::process(const float* input, float* output, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        output[i] = processSample(input[i]);
    }
}

float LowPassFilter::processSample(float input) {
    // Smooth parameter changes to prevent zipper noise
    cutoffCurrent += (cutoff - cutoffCurrent) * smoothing;
    resonanceCurrent += (resonance - resonanceCurrent) * smoothing;
    
    // Calculate filter coefficient with smoothed cutoff
    float rc = 1.0f / (TWO_PI * cutoffCurrent);
    float dt = 1.0f / static_cast<float>(sampleRate);
    float alpha = dt / (rc + dt);
    
    // Add resonance (feedback)
    // Scale Q value (0.1-20) to reasonable feedback range
    float resonanceFactor = (resonanceCurrent - 0.1f) / 19.9f;  // Normalize to 0.0-1.0
    alpha = alpha * (1.0f + resonanceFactor * 2.0f);
    alpha = std::min(alpha, 0.99f);
    
    float output = prevOutput + alpha * (input - prevOutput);
    
    // Clamp state to prevent runaway values that lead to NaN
    prevOutput = clampSample(output);
    
    return output;
}

void LowPassFilter::setCutoff(float freq) {
    cutoff = std::clamp(freq, 20.0f, 20000.0f);
}

void LowPassFilter::setResonance(float res) {
    resonance = std::clamp(res, 0.1f, 20.0f);
}

void LowPassFilter::reset() {
    prevOutput = 0.0f;
    cutoffCurrent = cutoff;
    resonanceCurrent = resonance;
}

// ============================================================================
// DCBlocker Implementation
// ============================================================================

DCBlocker::DCBlocker()
    : xPrev(0.0f)
    , yPrev(0.0f)
    , coeff(0.995f)  // High-pass at ~10Hz @ 48kHz
{
}

void DCBlocker::process(const float* input, float* output, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        output[i] = processSample(input[i]);
    }
}

float DCBlocker::processSample(float input) {
    // First-order high-pass filter: y[n] = x[n] - x[n-1] + coeff * y[n-1]
    float output = input - xPrev + coeff * yPrev;
    xPrev = input;
    yPrev = output;
    return output;
}

void DCBlocker::reset() {
    xPrev = 0.0f;
    yPrev = 0.0f;
}

} // namespace DubSiren
