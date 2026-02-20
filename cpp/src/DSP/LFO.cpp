#include "DSP/LFO.h"
#include <cmath>

namespace DubSiren {

LFO::LFO(int sampleRate)
    : sampleRate(sampleRate)
    , frequency(5.0f)    // 5 Hz default rate
    , phase(0.0f)
    , waveform(Waveform::Sine)
    , depth(0.0f)        // Disabled by default
{
}

void LFO::generate(float* output, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        output[i] = generateSample();
    }
}

float LFO::generateSample() {
    float value = 0.0f;
    float dt = frequency / static_cast<float>(sampleRate);
    
    switch (waveform) {
        case Waveform::Sine:
            value = std::sin(TWO_PI * phase);
            break;
            
        case Waveform::Square:
            value = (phase < 0.5f) ? 1.0f : -1.0f;
            break;
            
        case Waveform::Saw:
            value = 2.0f * phase - 1.0f;
            break;
            
        case Waveform::Triangle:
            if (phase < 0.5f) {
                value = 4.0f * phase - 1.0f;
            } else {
                value = 3.0f - 4.0f * phase;
            }
            break;
    }
    
    // Advance phase
    phase += dt;
    if (phase >= 1.0f) {
        phase -= 1.0f;
    }
    
    return value * depth;
}

void LFO::setFrequency(float freq) {
    frequency = clamp(freq, 0.1f, 20.0f);
}

void LFO::setWaveform(Waveform wf) {
    waveform = wf;
}

void LFO::setDepth(float d) {
    depth = clamp(d, 0.0f, 1.0f);
}

} // namespace DubSiren
