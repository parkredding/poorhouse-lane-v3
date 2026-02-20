#include "DSP/Delay.h"
#include <cmath>
#include <algorithm>

namespace DubSiren {

DelayEffect::DelayEffect(int sampleRate, float maxDelay)
    : sampleRate(sampleRate)
    , maxDelaySamples(static_cast<int>(maxDelay * sampleRate))
    , buffer(maxDelaySamples, 0.0f)
    , writePos(0)
    , delayTime(0.3f)
    , feedback(0.3f)
    , dryWet(0.0f)
    , currentDelaySamples(0.3f * sampleRate)
    , repitchRate(0.5f)
    , filterHpFreq(80.0f)
    , filterLpFreq(5000.0f)
    , hpState(0.0f)
    , lpState(0.0f)
    , modDepth(0.003f)
    , modRate(0.5f)
    , modPhase(0.0f)
    , flutterDepth(0.001f)
    , flutterRate(3.5f)
    , flutterPhase(0.0f)
    , tapeSaturation(0.3f)
{
    slewRate = calculateSlewRate();
}

float DelayEffect::calculateSlewRate() const {
    if (repitchRate <= 0.0f) {
        return std::numeric_limits<float>::infinity();
    }
    float maxSlewTime = 2.0f * repitchRate;
    return static_cast<float>(maxDelaySamples) / (maxSlewTime * static_cast<float>(sampleRate));
}

float DelayEffect::processFeedbackFilters(float sample) {
    // High-pass filter (removes mud/low-end buildup)
    float hpCutoffNorm = filterHpFreq / static_cast<float>(sampleRate);
    float hpCoeff = 1.0f - std::exp(-TWO_PI * hpCutoffNorm);
    hpState = clampSample(hpState + hpCoeff * (sample - hpState));
    float filtered = sample - hpState;
    
    // Low-pass filter (tape-like high-frequency loss)
    float lpCutoffNorm = filterLpFreq / static_cast<float>(sampleRate);
    float lpCoeff = 1.0f - std::exp(-TWO_PI * lpCutoffNorm);
    lpState = clampSample(lpState + lpCoeff * (filtered - lpState));
    
    // Tape-style saturation (gentle warmth)
    float saturated = fastTanh(lpState * (1.0f + tapeSaturation * 2.0f));
    float result = lpState * (1.0f - tapeSaturation) + saturated * tapeSaturation;
    
    return result;
}

float DelayEffect::lerpRead(float delaySamples) const {
    // Calculate read position (floating point for interpolation)
    float readPos = static_cast<float>(writePos) - delaySamples;
    if (readPos < 0) {
        readPos += static_cast<float>(maxDelaySamples);
    }
    
    // Integer and fractional parts for linear interpolation
    int readPosInt = static_cast<int>(readPos);
    float frac = readPos - static_cast<float>(readPosInt);
    
    // Get two adjacent samples
    int idx0 = readPosInt % maxDelaySamples;
    int idx1 = (readPosInt + 1) % maxDelaySamples;
    
    // Linear interpolation between samples
    return buffer[idx0] * (1.0f - frac) + buffer[idx1] * frac;
}

void DelayEffect::process(const float* input, float* output, int numSamples) {
    float targetDelaySamples = delayTime * static_cast<float>(sampleRate);
    
    for (int i = 0; i < numSamples; ++i) {
        // Analog behavior: smoothly slew toward target delay time
        if (std::isinf(slewRate)) {
            currentDelaySamples = targetDelaySamples;
        } else {
            float diff = targetDelaySamples - currentDelaySamples;
            if (std::abs(diff) > slewRate) {
                currentDelaySamples += (diff > 0) ? slewRate : -slewRate;
            } else {
                currentDelaySamples = targetDelaySamples;
            }
        }
        
        // Add tape wobble and flutter modulation
        float mod = std::sin(TWO_PI * modRate * modPhase / static_cast<float>(sampleRate));
        float modSamples = modDepth * static_cast<float>(sampleRate) * mod;
        
        float flutter = std::sin(TWO_PI * flutterRate * flutterPhase / static_cast<float>(sampleRate));
        float flutterSamples = flutterDepth * static_cast<float>(sampleRate) * flutter;
        
        float totalDelaySamples = currentDelaySamples + modSamples + flutterSamples;
        totalDelaySamples = std::clamp(totalDelaySamples, 1.0f, static_cast<float>(maxDelaySamples - 2));
        
        // Advance modulation phases
        modPhase += 1.0f;
        if (modPhase >= static_cast<float>(sampleRate)) {
            modPhase = 0.0f;
        }
        
        flutterPhase += 1.0f;
        if (flutterPhase >= static_cast<float>(sampleRate)) {
            flutterPhase = 0.0f;
        }
        
        // Read from delay buffer with interpolation
        float delayed = lerpRead(totalDelaySamples);
        
        // Process feedback through filters
        float feedbackSignal = processFeedbackFilters(delayed);
        
        // Write to buffer
        buffer[writePos] = clampSample(input[i] + feedbackSignal * feedback);
        
        // Advance write position
        writePos = (writePos + 1) % maxDelaySamples;
        
        // Mix dry and wet
        output[i] = input[i] * (1.0f - dryWet) + delayed * dryWet;
    }
}

void DelayEffect::setDelayTime(float timeSeconds) {
    delayTime = std::clamp(timeSeconds, 0.001f, 2.0f);
}

void DelayEffect::setFeedback(float fb) {
    feedback = std::clamp(fb, 0.0f, 0.95f);
}

void DelayEffect::setDryWet(float mix) {
    dryWet = std::clamp(mix, 0.0f, 1.0f);
}

void DelayEffect::setRepitchRate(float rate) {
    repitchRate = std::clamp(rate, 0.0f, 1.0f);
    slewRate = calculateSlewRate();
}

void DelayEffect::setModDepth(float depth) {
    modDepth = std::clamp(depth, 0.0f, 0.01f);
}

void DelayEffect::setModRate(float rate) {
    modRate = std::clamp(rate, 0.1f, 5.0f);
}

void DelayEffect::setTapeSaturation(float amount) {
    tapeSaturation = std::clamp(amount, 0.0f, 1.0f);
}

} // namespace DubSiren
