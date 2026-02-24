#include "DSP/Filter.h"
#include <cmath>
#include <algorithm>

namespace DubSiren {

// Bounded tanh approximation for integrator-state saturation.
// Accurate to ~4% for |x| <= 3; hard-clamps beyond (true tanh ≈ ±1 there).
// Much cheaper than std::tanh on Raspberry Pi while staying bounded,
// unlike the fastTanh() Padé approximant in Common.h which diverges for
// large inputs.
static inline float tanhSat(float x) {
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// ============================================================================
// LowPassFilter Implementation
// ============================================================================

LowPassFilter::LowPassFilter(int sampleRate)
    : sampleRate(std::max(1, sampleRate))
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
    float fc = std::min(cutoffCurrent, static_cast<float>(sampleRate) * 0.49f);
    float f = 2.0f * std::sin(PI * fc / static_cast<float>(sampleRate));
    float q_inv = 1.0f / resonanceCurrent;

    // SVF tick: lp → hp → bp (canonical Chamberlin order).
    // Computing lp first with the OLD bp state gives the classic delayed-
    // feedback path described in Chamberlin (1985).  The previous hp→bp→lp
    // ordering fed the current sample through both integrators in a single
    // tick, producing higher instantaneous peaks at high resonance.
    float lp = lpState + f * bpState;
    float hp = input - lp - q_inv * bpState;
    float bp = f * hp + bpState;

    // Soft-saturate integrator states using tanh to emulate analog component
    // saturation.  A resonant SVF amplifies signals near the cutoff by a
    // factor of Q — with Q=5 a ±1.0 input can produce ±5.0 output, causing
    // harsh digital clipping at the DAC.  Tanh saturation naturally limits
    // the resonant peak while leaving the passband (which sits in the linear
    // region of the curve) nearly unchanged.
    constexpr float SAT = 1.5f;
    lpState = SAT * tanhSat(lp / SAT);
    bpState = SAT * tanhSat(bp / SAT);

    // Return the saturated value so downstream stages see the limited signal
    return lpState;
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
