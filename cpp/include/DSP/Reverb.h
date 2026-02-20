#pragma once

#include "Common.h"
#include <vector>
#include <array>

namespace DubSiren {

/**
 * Physically-modeled spring reverb effect.
 *
 * Authentic spring reverb characteristics:
 * - 3 parallel spring lines with dispersive delay
 * - Modal resonances for metallic "boing" character
 * - Input/output transducer modeling
 * - Diffusion network for smooth decay
 * - Perfect for drippy dub reverb tones
 */
class ReverbEffect {
public:
    explicit ReverbEffect(int sampleRate = DEFAULT_SAMPLE_RATE);

    void process(const float* input, float* output, int numSamples);

    // Parameters (same interface as before)
    void setSize(float size);      // Spring decay time (0.0 - 1.0)
    void setDryWet(float mix);     // Dry/wet mix (0.0 - 1.0)
    void setDamping(float damp);   // High-frequency damping (0.0 - 1.0)
    void setWidth(float width);    // Stereo width (0.0 - 1.0)

    float getSize() const { return springDecay; }
    float getDryWet() const { return wet; }

private:
    int sampleRate;
    float sampleRateInv;

    // Spring reverb configuration
    static constexpr int NUM_SPRINGS = 3;
    static constexpr int NUM_ALLPASS = 4;

    // Spring delay lengths (in samples at 48kHz) - tuned for dub character
    static constexpr int SPRING_LENGTHS[NUM_SPRINGS] = {
        3491, 4177, 4831  // ~72ms, ~87ms, ~100ms - gives nice drippy decay
    };

    // Allpass lengths for diffusion
    static constexpr int ALLPASS_LENGTHS[NUM_ALLPASS] = {
        347, 431, 521, 619
    };

    // Biquad filter (for transducers and modal resonances)
    struct Biquad {
        float b0, b1, b2, a1, a2;
        float x1, x2, y1, y2;

        Biquad() : b0(1), b1(0), b2(0), a1(0), a2(0), x1(0), x2(0), y1(0), y2(0) {}

        void setLowpass(float freq, float q, float sampleRate);
        void setBandpass(float freq, float q, float sampleRate);
        void setHighpass(float freq, float q, float sampleRate);
        float process(float input);
        void reset();
    };

    // Spring line with dispersive delay and modal resonances
    struct SpringLine {
        std::vector<float> delayBuffer;
        int delayLength;
        int writeIndex;

        // Multi-tap delays for dispersion (high freqs travel faster)
        static constexpr int NUM_TAPS = 5;
        Biquad tapFilters[NUM_TAPS];  // Bandpass filters for each tap

        // Modal resonances (spring natural frequencies)
        static constexpr int NUM_MODES = 3;
        Biquad modalFilters[NUM_MODES];

        // Damping filter in feedback path
        Biquad dampingFilter;

        float feedback;

        SpringLine() : delayLength(0), writeIndex(0), feedback(0.0f) {}
        void init(int length, float sampleRate, int springIndex);
        float process(float input);
    };

    // Allpass filter for diffusion
    struct AllpassFilter {
        std::vector<float> buffer;
        int bufferSize;
        int index;

        AllpassFilter() : bufferSize(0), index(0) {}
        void init(int size);
        float process(float input);
    };

    // Input/output transducers
    Biquad inputTransducer;   // Lowpass ~4kHz
    Biquad outputLowcut;      // Highpass ~80Hz
    Biquad outputHighcut;     // Lowpass ~6kHz

    // Spring lines (stereo)
    std::array<SpringLine, NUM_SPRINGS> springsL;
    std::array<SpringLine, NUM_SPRINGS> springsR;

    // Diffusion network
    std::array<AllpassFilter, NUM_ALLPASS> allpassL;
    std::array<AllpassFilter, NUM_ALLPASS> allpassR;

    // Parameters
    float springDecay;
    float damping;
    float wet;
    float dry;
    float width;

    // Soft saturation for input transducer
    inline float softClip(float x) {
        // Soft clipping using tanh approximation
        if (x > 1.5f) return 1.0f;
        if (x < -1.5f) return -1.0f;
        return x - (x * x * x) / 3.0f;
    }

    void updateCoefficients();

    // Stereo spread
    static constexpr int STEREO_SPREAD = 47;

    // Gains (tuned for spring reverb)
    static constexpr float INPUT_GAIN = 0.8f;
    static constexpr float OUTPUT_GAIN = 0.35f;
};

} // namespace DubSiren
