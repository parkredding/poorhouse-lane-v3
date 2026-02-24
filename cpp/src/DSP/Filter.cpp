#include "DSP/Filter.h"
#include <cmath>
#include <algorithm>

namespace DubSiren {

// ============================================================================
// LowPassFilter Implementation
// ============================================================================

LowPassFilter::LowPassFilter(int sampleRate)
    : sampleRate(sampleRate)
    , cutoff(3000.0f)
    , cutoffCurrent(3000.0f)
    , resonance(1.0f)
    , resonanceCurrent(1.0f)
    , lpState(0.0f)
    , bpState(0.0f)
    , smoothing(0.05f)
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

    // Chamberlin State Variable Filter (2-pole, 12dB/oct).
    // F = 2*sin(pi*fc/fs)  — frequency drive coefficient.
    // A two-pole SVF produces a genuine resonant peak at the cutoff
    // frequency rather than merely shifting it, so different waveforms
    // stay perceptually distinct even at high Q values.
    float fc = std::min(cutoffCurrent, static_cast<float>(sampleRate) * 0.49f);
    float f = 2.0f * std::sin(PI * fc / static_cast<float>(sampleRate));

    // q_inv = 1/Q (damping). resonance parameter maps directly to Q:
    //   res ≈ 0.707 → Butterworth (no peak)
    //   res = 1–5   → mild-to-strong resonant peak
    //   res > 10    → near self-oscillation
    float q_inv = 1.0f / resonanceCurrent;

    // SVF tick: lp → band-pass → high-pass
    float lp  = lpState + f * bpState;
    float hp  = input - lp - q_inv * bpState;
    float bp  = f * hp + bpState;

    // Clamp integrator states to prevent runaway on extreme inputs
    lpState = clampSample(lp);
    bpState = clampSample(bp);

    return lp;
}

void LowPassFilter::setCutoff(float freq) {
    cutoff = std::clamp(freq, 20.0f, 20000.0f);
}

void LowPassFilter::setResonance(float res) {
    resonance = std::clamp(res, 0.1f, 20.0f);
}

void LowPassFilter::reset() {
    lpState = 0.0f;
    bpState = 0.0f;
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
