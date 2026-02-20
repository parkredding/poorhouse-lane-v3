#include "DSP/Envelope.h"
#include <cmath>
#include <algorithm>

namespace DubSiren {

// Decay scale factor: -ln(0.01) for reaching 99% of target
constexpr float DECAY_SCALE = 4.605f;

Envelope::Envelope(int sampleRate)
    : sampleRate(sampleRate)
    , attackTime(0.01f)    // 10ms default attack
    , releaseTime(0.05f)   // 50ms default release
    , attackCoeff(0.0f)
    , releaseCoeff(0.0f)
    , currentValue(0.0f)
    , active(false)
{
    updateCoefficients();
}

void Envelope::updateCoefficients() {
    // Attack coefficient: time to reach 99% of target
    attackCoeff = DECAY_SCALE / (attackTime * static_cast<float>(sampleRate));
    
    // Release coefficient: time to decay to 1% of peak
    releaseCoeff = DECAY_SCALE / (releaseTime * static_cast<float>(sampleRate));
}

void Envelope::generate(float* output, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        output[i] = generateSample();
    }
}

float Envelope::generateSample() {
    float target;
    float coeff;
    
    if (active.load(std::memory_order_acquire)) {
        // Attack: approach 1.0
        target = 1.0f;
        coeff = attackCoeff;
    } else {
        // Release: approach 0.0
        target = 0.0f;
        coeff = releaseCoeff;
    }
    
    // Exponential approach to target (first-order filter)
    currentValue += (target - currentValue) * coeff;
    
    return currentValue;
}

void Envelope::trigger() {
    active.store(true, std::memory_order_release);
}

void Envelope::release() {
    active.store(false, std::memory_order_release);
}

void Envelope::setAttack(float timeSeconds) {
    attackTime = std::clamp(timeSeconds, 0.001f, 2.0f);
    updateCoefficients();
}

void Envelope::setRelease(float timeSeconds) {
    releaseTime = std::clamp(timeSeconds, 0.01f, 5.0f);
    updateCoefficients();
}

} // namespace DubSiren
