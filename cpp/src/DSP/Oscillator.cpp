#include "DSP/Oscillator.h"
#include <cmath>

namespace DubSiren {

Oscillator::Oscillator(int sampleRate)
    : sampleRate(sampleRate)
    , frequency(440.0f)
    , phase(0.0f)
    , waveform(Waveform::Sine)
{
}

void Oscillator::generate(float* output, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        output[i] = generateSample();
    }
}

float Oscillator::generateSample() {
    float sample = 0.0f;
    
    switch (waveform) {
        case Waveform::Sine:
            sample = generateSine();
            break;
        case Waveform::Square:
            sample = generateSquarePolyBlep();
            break;
        case Waveform::Saw:
            sample = generateSawPolyBlep();
            break;
        case Waveform::Triangle:
            sample = generateTriangle();
            break;
    }
    
    // Advance phase
    float dt = frequency / static_cast<float>(sampleRate);
    phase += dt;
    if (phase >= 1.0f) {
        phase -= 1.0f;
    }
    
    return sample;
}

float Oscillator::polyBlep(float t, float dt) const {
    /**
     * Calculate PolyBLEP (Polynomial Band-Limited Step) residual.
     * 
     * PolyBLEP reduces aliasing in discontinuous waveforms (square, sawtooth)
     * by applying a polynomial correction near discontinuities.
     * 
     * @param t Current phase position (0.0 to 1.0)
     * @param dt Phase increment per sample (frequency / sample_rate)
     * @return The PolyBLEP residual to subtract from the naive waveform
     */
    
    // Check if we're within one sample of a discontinuity
    if (t < dt) {
        // Just after the discontinuity (phase recently wrapped)
        float tNorm = t / dt;  // Normalize to 0-1 range
        return tNorm + tNorm - tNorm * tNorm - 1.0f;
    } else if (t > 1.0f - dt) {
        // Just before the discontinuity (phase about to wrap)
        float tNorm = (t - 1.0f) / dt;  // Normalize to -1 to 0 range
        return tNorm * tNorm + tNorm + tNorm + 1.0f;
    }
    
    return 0.0f;
}

float Oscillator::generateSine() {
    // Sine wave - naturally band-limited, no anti-aliasing needed
    return std::sin(TWO_PI * phase);
}

float Oscillator::generateSquarePolyBlep() {
    /**
     * Generate square wave with PolyBLEP anti-aliasing.
     * 
     * PolyBLEP is applied at both transitions (0->1 at phase=0, 1->0 at phase=0.5)
     * to smooth the discontinuities and reduce aliasing.
     */
    float dt = frequency / static_cast<float>(sampleRate);
    
    // Naive square wave: +1 for first half, -1 for second half
    float value = (phase < 0.5f) ? 1.0f : -1.0f;
    
    // Apply PolyBLEP correction at the rising edge (phase = 0)
    value += polyBlep(phase, dt);

    // Apply PolyBLEP correction at the falling edge (phase = 0.5)
    float phaseShifted = phase + 0.5f;
    if (phaseShifted >= 1.0f) {
        phaseShifted -= 1.0f;
    }
    value -= polyBlep(phaseShifted, dt);
    
    return value;
}

float Oscillator::generateSawPolyBlep() {
    /**
     * Generate sawtooth wave with PolyBLEP anti-aliasing.
     * 
     * PolyBLEP is applied at the phase reset (when saw jumps from +1 to -1)
     * to smooth the discontinuity and reduce aliasing.
     */
    float dt = frequency / static_cast<float>(sampleRate);
    
    // Naive sawtooth: ramps from -1 to +1 over one cycle
    float value = 2.0f * phase - 1.0f;
    
    // Apply PolyBLEP correction at the discontinuity (phase = 0)
    value -= polyBlep(phase, dt);
    
    return value;
}

float Oscillator::generateTriangle() {
    /**
     * Generate triangle wave (continuous, no anti-aliasing needed).
     * 
     * Triangle waves have no discontinuities - they're continuous with
     * continuous first derivative at peaks. This makes them naturally
     * band-limited with harmonics that fall off as 1/n².
     */
    
    // Triangle from phase: rises 0->0.5, falls 0.5->1
    if (phase < 0.5f) {
        return 4.0f * phase - 1.0f;  // -1 to +1
    } else {
        return 3.0f - 4.0f * phase;  // +1 to -1
    }
}

void Oscillator::setFrequency(float freq) {
    frequency = clamp(freq, 20.0f, 20000.0f);
}

void Oscillator::setWaveform(Waveform wf) {
    waveform = wf;
}

void Oscillator::resetPhase() {
    phase = 0.0f;
}

} // namespace DubSiren
