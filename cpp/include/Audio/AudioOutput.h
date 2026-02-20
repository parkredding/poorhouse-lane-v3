#pragma once

#include "Common.h"
#include "Audio/AudioEngine.h"
#include <thread>
#include <atomic>
#include <memory>
#include <string>

namespace DubSiren {

/**
 * ALSA audio output handler.
 * Manages real-time audio streaming to the PCM5102 I2S DAC.
 */
class AudioOutput {
public:
    AudioOutput(AudioEngine& engine, 
                int sampleRate = DEFAULT_SAMPLE_RATE,
                int bufferSize = DEFAULT_BUFFER_SIZE,
                int channels = DEFAULT_CHANNELS,
                const char* device = nullptr);
    
    ~AudioOutput();
    
    // Non-copyable
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    
    /**
     * Start audio output stream.
     * @return true if started successfully
     */
    bool start();
    
    /**
     * Stop audio output stream.
     */
    void stop();
    
    /**
     * Check if audio is running.
     */
    bool isRunning() const { return running.load(); }
    
    /**
     * Get audio statistics.
     */
    struct Stats {
        uint64_t totalBuffers;
        uint64_t underruns;
        float cpuUsage;  // Estimated CPU usage percentage
    };
    Stats getStats() const;
    
private:
    AudioEngine& engine;
    int sampleRate;
    int bufferSize;
    int channels;
    std::string deviceName;
    
    std::atomic<bool> running;
    std::thread audioThread;
    
    // Statistics
    std::atomic<uint64_t> totalBuffers;
    std::atomic<uint64_t> underruns;
    std::atomic<float> lastCpuUsage;
    
    void audioLoop();
};

/**
 * Simulated audio output for testing without hardware.
 */
class SimulatedAudioOutput {
public:
    SimulatedAudioOutput(AudioEngine& engine, int bufferSize = DEFAULT_BUFFER_SIZE);
    ~SimulatedAudioOutput();
    
    bool start();
    void stop();
    bool isRunning() const { return running.load(); }
    
private:
    AudioEngine& engine;
    int bufferSize;
    std::atomic<bool> running;
    std::thread simulationThread;
    std::vector<float> buffer;
    
    void simulationLoop();
};

} // namespace DubSiren
