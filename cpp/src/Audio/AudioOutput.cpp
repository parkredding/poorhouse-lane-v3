#include "Audio/AudioOutput.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <cerrno>

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

namespace DubSiren {

// ============================================================================
// AudioOutput Implementation (ALSA)
// ============================================================================

AudioOutput::AudioOutput(AudioEngine& engine, 
                         int sampleRate,
                         int bufferSize,
                         int channels,
                         const char* device)
    : engine(engine)
    , sampleRate(sampleRate)
    , bufferSize(bufferSize)
    , channels(channels)
    , deviceName(device ? device : "default")
    , running(false)
    , totalBuffers(0)
    , underruns(0)
    , lastCpuUsage(0.0f)
{
}

AudioOutput::~AudioOutput() {
    stop();
}

bool AudioOutput::start() {
#ifdef HAVE_ALSA
    if (running.load()) {
        std::cout << "Audio output already running" << std::endl;
        return true;
    }
    
    running.store(true);
    audioThread = std::thread(&AudioOutput::audioLoop, this);
    
    std::cout << "Audio output started: " << sampleRate << "Hz, " 
              << bufferSize << " samples, " << channels << " channels, "
              << "device=" << deviceName << std::endl;
    
    return true;
#else
    std::cerr << "ALSA not available - audio output disabled" << std::endl;
    return false;
#endif
}

void AudioOutput::stop() {
    if (!running.load()) {
        return;
    }
    
    running.store(false);
    
    if (audioThread.joinable()) {
        audioThread.join();
    }
    
    // Print statistics
    uint64_t total = totalBuffers.load();
    uint64_t under = underruns.load();
    
    if (total > 0) {
        float underrunRate = static_cast<float>(under) / static_cast<float>(total) * 100.0f;
        std::cout << "\nAudio performance:" << std::endl;
        std::cout << "  Total buffers: " << total << std::endl;
        std::cout << "  Buffer underruns: " << under << " (" << underrunRate << "%)" << std::endl;
    }
    
    std::cout << "Audio output stopped" << std::endl;
}

void AudioOutput::audioLoop() {
#ifdef HAVE_ALSA
    snd_pcm_t* pcm = nullptr;
    int err;
    
    // Open PCM device
    err = snd_pcm_open(&pcm, deviceName.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        std::cerr << "Cannot open audio device " << deviceName << ": " 
                  << snd_strerror(err) << std::endl;
        running.store(false);
        return;
    }
    
    // Use snd_pcm_set_params for ALSA configuration
    // 50ms latency - good balance of responsiveness and stability
    err = snd_pcm_set_params(pcm,
                              SND_PCM_FORMAT_S16_LE,
                              SND_PCM_ACCESS_RW_INTERLEAVED,
                              channels,
                              sampleRate,
                              1,  // allow resampling
                              50000);  // latency in us (50ms)
    if (err < 0) {
        std::cerr << "Cannot set PCM parameters: " << snd_strerror(err) << std::endl;
        snd_pcm_close(pcm);
        running.store(false);
        return;
    }
    
    // PCM configured successfully
    
    // Allocate buffers
    std::vector<float> floatBuffer(bufferSize * channels);
    std::vector<int16_t> intBuffer(bufferSize * channels);
    
    // Calculate expected buffer duration for CPU usage estimation
    double bufferDuration = static_cast<double>(bufferSize) / static_cast<double>(sampleRate);
    
    // CPU logging variables (logs every 10 seconds)
    float cpuSum = 0.0f;
    float cpuMax = 0.0f;
    int cpuSamples = 0;
    auto lastLogTime = std::chrono::steady_clock::now();
    const auto logInterval = std::chrono::seconds(10);
    
    while (running.load()) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Generate audio
        engine.process(floatBuffer.data(), bufferSize);
        
        // Convert to int16
        for (size_t i = 0; i < floatBuffer.size(); ++i) {
            float sample = clamp(floatBuffer[i], -1.0f, 1.0f);
            intBuffer[i] = static_cast<int16_t>(sample * 32767.0f);
        }
        
        auto processTime = std::chrono::high_resolution_clock::now();
        
        // Write to ALSA
        snd_pcm_sframes_t frames = snd_pcm_writei(pcm, intBuffer.data(), bufferSize);
        
        if (frames < 0) {
            // Handle underrun
            underruns.fetch_add(1);
            std::cerr << "[ALSA] Underrun! " << snd_strerror(static_cast<int>(frames)) << std::endl;
            frames = snd_pcm_recover(pcm, static_cast<int>(frames), 0);
            if (frames < 0) {
                std::cerr << "[ALSA] Recovery failed: " << snd_strerror(static_cast<int>(frames)) << std::endl;
            }
        }
        
        totalBuffers.fetch_add(1);
        
        // Calculate CPU usage (processing time vs available time)
        std::chrono::duration<double> processDuration = processTime - startTime;
        float cpuUsage = static_cast<float>(processDuration.count() / bufferDuration * 100.0);
        lastCpuUsage.store(cpuUsage);
        
        // Accumulate for logging
        cpuSum += cpuUsage;
        if (cpuUsage > cpuMax) cpuMax = cpuUsage;
        cpuSamples++;
        
        // Log CPU usage every 2 seconds
        auto now = std::chrono::steady_clock::now();
        if (now - lastLogTime >= logInterval && cpuSamples > 0) {
            float avgCpu = cpuSum / cpuSamples;
            std::cout << "[CPU] avg=" << std::fixed << std::setprecision(1) << avgCpu 
                      << "% max=" << cpuMax << "% (headroom: " << (100.0f - cpuMax) << "%)" << std::endl;
            cpuSum = 0.0f;
            cpuMax = 0.0f;
            cpuSamples = 0;
            lastLogTime = now;
        }
    }
    
    // Drain and close
    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);
#endif
}

AudioOutput::Stats AudioOutput::getStats() const {
    return {
        totalBuffers.load(),
        underruns.load(),
        lastCpuUsage.load()
    };
}

// ============================================================================
// SimulatedAudioOutput Implementation
// ============================================================================

SimulatedAudioOutput::SimulatedAudioOutput(AudioEngine& engine, int bufferSize)
    : engine(engine)
    , bufferSize(bufferSize)
    , running(false)
    , buffer(bufferSize * 2)  // Stereo
{
    std::cout << "Running in SIMULATION mode (no audio output)" << std::endl;
}

SimulatedAudioOutput::~SimulatedAudioOutput() {
    stop();
}

bool SimulatedAudioOutput::start() {
    if (running.load()) {
        return true;
    }
    
    running.store(true);
    simulationThread = std::thread(&SimulatedAudioOutput::simulationLoop, this);
    
    std::cout << "Simulated audio output started" << std::endl;
    return true;
}

void SimulatedAudioOutput::stop() {
    if (!running.load()) {
        return;
    }
    
    running.store(false);
    
    if (simulationThread.joinable()) {
        simulationThread.join();
    }
    
    std::cout << "Simulated audio output stopped" << std::endl;
}

void SimulatedAudioOutput::simulationLoop() {
    // Simulate audio callback at regular intervals
    double bufferDuration = static_cast<double>(bufferSize) / static_cast<double>(DEFAULT_SAMPLE_RATE);
    auto sleepDuration = std::chrono::duration<double>(bufferDuration);
    
    while (running.load()) {
        // Generate audio (but don't output it)
        engine.process(buffer.data(), bufferSize);
        
        // Sleep to simulate real-time behavior
        std::this_thread::sleep_for(sleepDuration);
    }
}

} // namespace DubSiren
