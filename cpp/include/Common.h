#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>
#include <vector>
#include <atomic>

namespace DubSiren {

// Audio configuration constants
constexpr int DEFAULT_SAMPLE_RATE = 48000;
constexpr int DEFAULT_BUFFER_SIZE = 256;
constexpr int DEFAULT_CHANNELS = 2;

// DSP constants
constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;
constexpr float MAX_SAFE_AMPLITUDE = 10.0f;

// Waveform types
enum class Waveform {
    Sine = 0,
    Square = 1,
    Saw = 2,
    Triangle = 3
};

// Pitch envelope modes
enum class PitchEnvelopeMode {
    None = 0,
    Up = 1,
    Down = 2
};

// Utility functions
inline float clamp(float value, float min, float max) {
    return std::max(min, std::min(max, value));
}

inline float clampSample(float value) {
    if (value > MAX_SAFE_AMPLITUDE) return MAX_SAFE_AMPLITUDE;
    if (value < -MAX_SAFE_AMPLITUDE) return -MAX_SAFE_AMPLITUDE;
    return value;
}

// Linear interpolation
inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Fast approximation of tanh for saturation
inline float fastTanh(float x) {
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Convert frequency to angular velocity
inline float freqToOmega(float freq, float sampleRate) {
    return TWO_PI * freq / sampleRate;
}

// Parameter smoothing helper (one-pole filter)
class SmoothedValue {
public:
    SmoothedValue(float initialValue = 0.0f, float smoothingCoeff = 0.01f)
        : target(initialValue), current(initialValue), coeff(smoothingCoeff) {}
    
    void setTarget(float newTarget) {
        target = newTarget;
    }
    
    void setImmediate(float value) {
        target = value;
        current = value;
    }
    
    float getNext() {
        current += (target - current) * coeff;
        return current;
    }
    
    float getCurrent() const { return current; }
    float getTarget() const { return target; }
    bool isSmoothing() const { return std::abs(target - current) > 0.0001f; }
    
private:
    float target;
    float current;
    float coeff;
};

// Thread-safe parameter for real-time audio
template<typename T>
class AudioParameter {
public:
    AudioParameter(T initialValue = T()) : value(initialValue) {}
    
    void set(T newValue) {
        value.store(newValue, std::memory_order_relaxed);
    }
    
    T get() const {
        return value.load(std::memory_order_relaxed);
    }
    
private:
    std::atomic<T> value;
};

} // namespace DubSiren
