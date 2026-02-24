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

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
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

void AudioOutput::setRealtimePriority() {
#ifdef __linux__
    // Set SCHED_FIFO real-time scheduling for the audio thread.
    // This prevents normal-priority processes from preempting audio,
    // which is the primary cause of buffer underruns on Linux.
    struct sched_param param;
    param.sched_priority = 80;  // High RT priority (range 1-99)

    int err = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (err != 0) {
        // Fall back to SCHED_RR if FIFO fails
        err = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
        if (err != 0) {
            std::cerr << "[Audio] Warning: Could not set real-time priority (error "
                      << err << "). Run as root or set rtprio in /etc/security/limits.conf"
                      << std::endl;
        } else {
            std::cout << "[Audio] Using SCHED_RR priority " << param.sched_priority << std::endl;
        }
    } else {
        std::cout << "[Audio] Using SCHED_FIFO priority " << param.sched_priority << std::endl;
    }
#endif
}

#ifdef HAVE_ALSA
bool AudioOutput::configureAlsa(snd_pcm_t* pcm) {
    int err;
    snd_pcm_hw_params_t* hwParams = nullptr;
    snd_pcm_sw_params_t* swParams = nullptr;

    // ---- Hardware parameters ----
    snd_pcm_hw_params_alloca(&hwParams);

    err = snd_pcm_hw_params_any(pcm, hwParams);
    if (err < 0) {
        std::cerr << "[ALSA] Cannot get hw_params: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_hw_params_set_access(pcm, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        std::cerr << "[ALSA] Cannot set access type: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_hw_params_set_format(pcm, hwParams, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        std::cerr << "[ALSA] Cannot set format: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_hw_params_set_channels(pcm, hwParams, channels);
    if (err < 0) {
        std::cerr << "[ALSA] Cannot set channels: " << snd_strerror(err) << std::endl;
        return false;
    }

    unsigned int actualRate = static_cast<unsigned int>(sampleRate);
    err = snd_pcm_hw_params_set_rate_near(pcm, hwParams, &actualRate, nullptr);
    if (err < 0) {
        std::cerr << "[ALSA] Cannot set sample rate: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Set period size to match our processing buffer size.
    // This ensures each snd_pcm_writei call fills exactly one period,
    // giving predictable wake-up timing.
    snd_pcm_uframes_t periodSize = static_cast<snd_pcm_uframes_t>(bufferSize);
    err = snd_pcm_hw_params_set_period_size_near(pcm, hwParams, &periodSize, nullptr);
    if (err < 0) {
        std::cerr << "[ALSA] Cannot set period size: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Use 3 periods for the ring buffer: gives ~16ms of safety margin at
    // 256 samples/period @ 48kHz while keeping latency reasonable.
    unsigned int periods = 3;
    err = snd_pcm_hw_params_set_periods_near(pcm, hwParams, &periods, nullptr);
    if (err < 0) {
        std::cerr << "[ALSA] Cannot set period count: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_hw_params(pcm, hwParams);
    if (err < 0) {
        std::cerr << "[ALSA] Cannot apply hw_params: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Read back actual values for logging
    snd_pcm_uframes_t actualPeriod, actualBuffer;
    snd_pcm_hw_params_get_period_size(hwParams, &actualPeriod, nullptr);
    snd_pcm_hw_params_get_buffer_size(hwParams, &actualBuffer);

    std::cout << "[ALSA] Period size: " << actualPeriod
              << " frames, Buffer: " << actualBuffer << " frames ("
              << periods << " periods), Rate: " << actualRate << " Hz" << std::endl;

    // ---- Software parameters ----
    snd_pcm_sw_params_alloca(&swParams);

    err = snd_pcm_sw_params_current(pcm, swParams);
    if (err < 0) {
        std::cerr << "[ALSA] Cannot get sw_params: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Start playback when the buffer is nearly full (all but one period).
    // This pre-fills the buffer before the DAC starts consuming, preventing
    // an immediate underrun at startup.
    err = snd_pcm_sw_params_set_start_threshold(pcm, swParams, actualBuffer - actualPeriod);
    if (err < 0) {
        std::cerr << "[ALSA] Cannot set start threshold: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Wake up the writer when at least one period of space is available
    err = snd_pcm_sw_params_set_avail_min(pcm, swParams, actualPeriod);
    if (err < 0) {
        std::cerr << "[ALSA] Cannot set avail_min: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_sw_params(pcm, swParams);
    if (err < 0) {
        std::cerr << "[ALSA] Cannot apply sw_params: " << snd_strerror(err) << std::endl;
        return false;
    }

    return true;
}
#endif

void AudioOutput::audioLoop() {
#ifdef HAVE_ALSA
    // Promote this thread to real-time priority before touching any audio
    setRealtimePriority();

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

    // Configure ALSA with explicit period/buffer control
    if (!configureAlsa(pcm)) {
        std::cerr << "[ALSA] Configuration failed, falling back to snd_pcm_set_params" << std::endl;
        err = snd_pcm_set_params(pcm,
                                  SND_PCM_FORMAT_S16_LE,
                                  SND_PCM_ACCESS_RW_INTERLEAVED,
                                  channels,
                                  sampleRate,
                                  1,
                                  50000);  // 50ms latency fallback
        if (err < 0) {
            std::cerr << "Cannot set PCM parameters: " << snd_strerror(err) << std::endl;
            snd_pcm_close(pcm);
            running.store(false);
            return;
        }
    }

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

    // Track consecutive underruns for burst logging (avoid flooding stderr)
    int consecutiveUnderruns = 0;

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
            // Handle underrun — increment counter but avoid blocking I/O here.
            // Printing to stderr from the audio thread can itself cause the next
            // buffer to be late, creating a cascade of underruns.
            underruns.fetch_add(1);
            consecutiveUnderruns++;
            frames = snd_pcm_recover(pcm, static_cast<int>(frames), 1);  // silent recovery
            if (frames < 0) {
                // Recovery failed — this is serious, log it
                std::cerr << "[ALSA] Recovery failed: " << snd_strerror(static_cast<int>(frames)) << std::endl;
            }
        } else {
            // Log after a burst of underruns ends (not during)
            if (consecutiveUnderruns > 0) {
                std::cerr << "[ALSA] " << consecutiveUnderruns << " underrun(s) recovered" << std::endl;
                consecutiveUnderruns = 0;
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

        // Log CPU usage periodically
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
