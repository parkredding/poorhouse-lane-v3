#include "DSP/Reverb.h"
#include <cmath>
#include <algorithm>

namespace DubSiren {

// ============================================================================
// Biquad Filter Implementation
// ============================================================================

void ReverbEffect::Biquad::setLowpass(float freq, float q, float sampleRate) {
    float omega = 2.0f * M_PI * freq / sampleRate;
    float cosOmega = std::cos(omega);
    float sinOmega = std::sin(omega);
    float alpha = sinOmega / (2.0f * q);

    float a0 = 1.0f + alpha;
    b0 = ((1.0f - cosOmega) / 2.0f) / a0;
    b1 = (1.0f - cosOmega) / a0;
    b2 = b0;
    a1 = (-2.0f * cosOmega) / a0;
    a2 = (1.0f - alpha) / a0;
}

void ReverbEffect::Biquad::setBandpass(float freq, float q, float sampleRate) {
    float omega = 2.0f * M_PI * freq / sampleRate;
    float cosOmega = std::cos(omega);
    float sinOmega = std::sin(omega);
    float alpha = sinOmega / (2.0f * q);

    float a0 = 1.0f + alpha;
    b0 = alpha / a0;
    b1 = 0.0f;
    b2 = -alpha / a0;
    a1 = (-2.0f * cosOmega) / a0;
    a2 = (1.0f - alpha) / a0;
}

void ReverbEffect::Biquad::setHighpass(float freq, float q, float sampleRate) {
    float omega = 2.0f * M_PI * freq / sampleRate;
    float cosOmega = std::cos(omega);
    float sinOmega = std::sin(omega);
    float alpha = sinOmega / (2.0f * q);

    float a0 = 1.0f + alpha;
    b0 = ((1.0f + cosOmega) / 2.0f) / a0;
    b1 = -(1.0f + cosOmega) / a0;
    b2 = b0;
    a1 = (-2.0f * cosOmega) / a0;
    a2 = (1.0f - alpha) / a0;
}

float ReverbEffect::Biquad::process(float input) {
    float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

    // Prevent denormals
    if (std::abs(output) < 1e-10f) {
        output = 0.0f;
    }

    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;

    return output;
}

void ReverbEffect::Biquad::reset() {
    x1 = x2 = y1 = y2 = 0.0f;
}

// ============================================================================
// Spring Line Implementation
// ============================================================================

void ReverbEffect::SpringLine::init(int length, float sampleRate, int springIndex) {
    delayLength = length;
    delayBuffer.resize(length, 0.0f);
    writeIndex = 0;
    feedback = 0.85f;  // Default, will be updated

    // Initialize tap filters (bandpass filters at different frequencies for dispersion)
    // Each tap responds to different frequency bands
    float tapFreqs[NUM_TAPS] = {200.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f};
    float tapQ = 1.5f;

    for (int i = 0; i < NUM_TAPS; ++i) {
        tapFilters[i].setBandpass(tapFreqs[i], tapQ, sampleRate);
    }

    // Modal resonances - spring natural frequencies
    // Different for each spring to avoid phase cancellation
    float baseFreq = 150.0f + springIndex * 50.0f;  // 150Hz, 200Hz, 250Hz for springs 0,1,2
    float modalFreqs[NUM_MODES] = {
        baseFreq,           // Fundamental
        baseFreq * 2.3f,    // Second mode (not harmonic - springs are dispersive)
        baseFreq * 3.8f     // Third mode
    };
    float modalQ = 12.0f;  // High Q for pronounced "boing"

    for (int i = 0; i < NUM_MODES; ++i) {
        modalFilters[i].setBandpass(modalFreqs[i], modalQ, sampleRate);
    }

    // Damping filter (lowpass for high-frequency absorption)
    dampingFilter.setLowpass(3500.0f, 0.7f, sampleRate);
}

float ReverbEffect::SpringLine::process(float input) {
    // Read from delay line
    float delayed = delayBuffer[writeIndex];

    // Apply modal resonances to create spring character
    float modal = delayed;
    for (int i = 0; i < NUM_MODES; ++i) {
        modal += modalFilters[i].process(delayed) * 0.06f;  // Reduced from 0.15 to prevent buildup
    }

    // Apply damping (high-frequency absorption in feedback)
    float damped = dampingFilter.process(modal);

    // Dispersive feedback: different frequencies decay at different rates
    // This creates the characteristic "drip" of spring reverb
    float dispersed = 0.0f;
    for (int i = 0; i < NUM_TAPS; ++i) {
        float tapGain = 0.08f / NUM_TAPS;  // Reduced from 0.15 to prevent feedback loop
        dispersed += tapFilters[i].process(damped) * tapGain;
    }

    // Write to delay line with feedback (with safety limiting)
    float feedbackSig = input + (damped * feedback) + dispersed;

    // Hard limit to prevent runaway feedback
    if (feedbackSig > 2.0f) feedbackSig = 2.0f;
    if (feedbackSig < -2.0f) feedbackSig = -2.0f;

    delayBuffer[writeIndex] = feedbackSig;

    // Prevent denormals
    if (std::abs(delayBuffer[writeIndex]) < 1e-10f) {
        delayBuffer[writeIndex] = 0.0f;
    }

    // Advance write position
    writeIndex = (writeIndex + 1) % delayLength;

    return modal;  // Return the modally-enhanced output
}

// ============================================================================
// Allpass Filter Implementation
// ============================================================================

void ReverbEffect::AllpassFilter::init(int size) {
    bufferSize = size;
    buffer.resize(size, 0.0f);
    index = 0;
}

float ReverbEffect::AllpassFilter::process(float input) {
    float bufOut = buffer[index];

    // Standard allpass with feedback coefficient of 0.5
    float output = -input + bufOut;
    buffer[index] = input + (bufOut * 0.5f);

    // Prevent denormals
    if (std::abs(buffer[index]) < 1e-10f) {
        buffer[index] = 0.0f;
    }

    index = (index + 1) % bufferSize;

    return output;
}

// ============================================================================
// ReverbEffect Implementation
// ============================================================================

ReverbEffect::ReverbEffect(int sampleRate)
    : sampleRate(sampleRate)
    , sampleRateInv(1.0f / sampleRate)
    , springDecay(0.65f)      // Default: moderate-long decay (safer)
    , damping(0.65f)          // Default: dark character
    , wet(0.35f)              // Default: 35% wet
    , dry(0.65f)
    , width(1.0f)             // Default: full stereo width
{
    // Scale delay lengths for sample rate
    float scale = static_cast<float>(sampleRate) / 48000.0f;

    // Initialize spring lines (left channel)
    for (int i = 0; i < NUM_SPRINGS; ++i) {
        int len = static_cast<int>(SPRING_LENGTHS[i] * scale);
        springsL[i].init(len, sampleRate, i);
        springsR[i].init(len + STEREO_SPREAD, sampleRate, i);  // Offset for stereo
    }

    // Initialize allpass filters for diffusion
    for (int i = 0; i < NUM_ALLPASS; ++i) {
        int len = static_cast<int>(ALLPASS_LENGTHS[i] * scale);
        allpassL[i].init(len);
        allpassR[i].init(len + STEREO_SPREAD);
    }

    // Initialize input transducer (lowpass ~4kHz, models mechanical bandwidth)
    inputTransducer.setLowpass(4000.0f, 0.7f, sampleRate);

    // Initialize output transducers (bandpass ~80Hz - 6kHz, models pickup coil)
    outputLowcut.setHighpass(80.0f, 0.7f, sampleRate);
    outputHighcut.setLowpass(6000.0f, 0.7f, sampleRate);

    updateCoefficients();
}

void ReverbEffect::updateCoefficients() {
    // Update spring feedback based on decay parameter
    // Higher decay = longer reverb tail
    // Reduced range to prevent feedback loops when combined with delay
    float feedbackAmount = 0.5f + (springDecay * 0.25f);  // Range: 0.5 - 0.75 (safer)
    feedbackAmount = std::min(feedbackAmount, 0.75f);

    for (int i = 0; i < NUM_SPRINGS; ++i) {
        // Slightly different feedback for each spring to avoid buildup
        springsL[i].feedback = feedbackAmount * (0.92f + i * 0.015f);
        springsR[i].feedback = feedbackAmount * (0.92f + i * 0.015f);

        // Update damping filter based on damping parameter
        float dampFreq = 2000.0f + (1.0f - damping) * 4000.0f;  // 2kHz - 6kHz
        springsL[i].dampingFilter.setLowpass(dampFreq, 0.7f, sampleRate);
        springsR[i].dampingFilter.setLowpass(dampFreq, 0.7f, sampleRate);
    }
}

void ReverbEffect::process(const float* input, float* output, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        float inSample = input[i];

        // Input transducer: lowpass filter + soft saturation
        float transduced = inputTransducer.process(inSample * INPUT_GAIN);
        transduced = softClip(transduced);

        // Process through spring lines (parallel)
        float springOutL = 0.0f;
        float springOutR = 0.0f;

        for (int s = 0; s < NUM_SPRINGS; ++s) {
            springOutL += springsL[s].process(transduced);
            springOutR += springsR[s].process(transduced);
        }

        // Average the spring outputs
        springOutL /= NUM_SPRINGS;
        springOutR /= NUM_SPRINGS;

        // Apply diffusion (series allpass filters)
        for (int a = 0; a < NUM_ALLPASS; ++a) {
            springOutL = allpassL[a].process(springOutL);
            springOutR = allpassR[a].process(springOutR);
        }

        // Output transducers (bandpass filtering)
        springOutL = outputLowcut.process(springOutL);
        springOutL = outputHighcut.process(springOutL);
        springOutR = outputLowcut.process(springOutR);
        springOutR = outputHighcut.process(springOutR);

        // Stereo width control
        float mid = (springOutL + springOutR) * 0.5f;
        float side = (springOutL - springOutR) * 0.5f * width;
        springOutL = mid + side;
        springOutR = mid - side;

        // Mix wet/dry (output is mono, so average L+R)
        float wetMix = (springOutL + springOutR) * 0.5f * wet * OUTPUT_GAIN;
        float dryMix = inSample * dry;

        float finalOut = wetMix + dryMix;

        // Safety limiter to prevent clipping from feedback loops
        if (finalOut > 1.0f) finalOut = 1.0f;
        if (finalOut < -1.0f) finalOut = -1.0f;

        output[i] = finalOut;
    }
}

void ReverbEffect::setSize(float size) {
    springDecay = std::clamp(size, 0.0f, 1.0f);
    updateCoefficients();
}

void ReverbEffect::setDryWet(float mix) {
    wet = std::clamp(mix, 0.0f, 1.0f);
    dry = 1.0f - wet;
}

void ReverbEffect::setDamping(float damp) {
    damping = std::clamp(damp, 0.0f, 1.0f);
    updateCoefficients();
}

void ReverbEffect::setWidth(float w) {
    width = std::clamp(w, 0.0f, 1.0f);
}

} // namespace DubSiren
