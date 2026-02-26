#include "Audio/AudioEngine.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace DubSiren {

AudioEngine::AudioEngine(int sampleRate, int bufferSize)
    : sampleRate(sampleRate)
    , bufferSize(bufferSize)
    , oscillator(sampleRate)
    , lfo(sampleRate)
    , envelope(sampleRate)
    , delay(sampleRate)
    , reverb(sampleRate)
    , mp3Player(std::make_unique<AudioFilePlayer>())
    , volume(0.7f)
    , baseFrequency(440.0f)
    , lfoPitchDepth(0.0f)  // Default to 0 (no pitch modulation)
    , pitchEnvMode(PitchEnvelopeMode::Up)  // Default to UP for classic dub siren
    , audioMode(AudioMode::Synthesis)  // Default to synthesis mode
    , currentFrequency(440.0f)
    , frequencySmooth(440.0f, 0.08f)  // Increased smoothing to reduce zipper noise
    , inReleasePhase(false)
    , pitchEnvStartLevel(1.0f)
{
    // Pre-allocate buffers
    oscBuffer.resize(bufferSize);
    envBuffer.resize(bufferSize);
    lfoBuffer.resize(bufferSize);
    filterBuffer.resize(bufferSize);
    delayBuffer.resize(bufferSize);

    // Set initial parameters (Auto Wail preset)
    oscillator.setWaveform(Waveform::Square);  // Square for classic siren sound
    lfo.setFrequency(0.35f);     // Slow swell - rises and falls over ~3 seconds
    lfo.setDepth(0.5f);          // Filter modulation depth (controllable by encoder)
    lfo.setWaveform(Waveform::Triangle);  // Smooth pitch transitions
    envelope.setAttack(0.01f);
    envelope.setRelease(0.5f);
    delay.setDryWet(0.3f);
    delay.setFeedback(0.55f);    // Spacey dub echoes
    reverb.setDryWet(0.4f);      // Wet for atmosphere
}

void AudioEngine::process(float* output, int numFrames) {
    // Check if in MP3 playback mode
    if (audioMode.get() == AudioMode::MP3Playback && mp3Player) {
        mp3Player->fillBuffer(output, numFrames);
        return;
    }

    // Normal synthesis mode
    // Get pitch envelope mode
    PitchEnvelopeMode pitchMode = pitchEnvMode.get();
    float baseFreq = baseFrequency.get();
    float pitchDepth = lfoPitchDepth.get();

    // Generate envelope first (we need it for pitch envelope calculation)
    envelope.generate(envBuffer.data(), numFrames);

    // Generate LFO modulation (needed for pitch modulation)
    lfo.generate(lfoBuffer.data(), numFrames);

    // Generate oscillator with pitch envelope and LFO pitch modulation
    for (int i = 0; i < numFrames; ++i) {
        float targetFreq = baseFreq;
        
        // Apply pitch envelope during release phase
        if (inReleasePhase && pitchMode != PitchEnvelopeMode::None) {
            float envValue = envBuffer[i];
            
            // Calculate how far through release we are (0 = just started, 1 = finished)
            // envValue goes from pitchEnvStartLevel down to 0
            float releaseProgress = 0.0f;
            if (pitchEnvStartLevel > 0.001f) {
                releaseProgress = 1.0f - (envValue / pitchEnvStartLevel);
                releaseProgress = clamp(releaseProgress, 0.0f, 1.0f);
            }
            
            // Apply pitch shift (2 octaves = multiply by 4 at max)
            // Use exponential curve for musical pitch sweep
            if (pitchMode == PitchEnvelopeMode::Up) {
                // Pitch goes UP: multiply by 1.0 to 4.0 (2 octaves up)
                float pitchMult = std::pow(4.0f, releaseProgress);
                targetFreq = baseFreq * pitchMult;
            } else if (pitchMode == PitchEnvelopeMode::Down) {
                // Pitch goes DOWN: multiply by 1.0 to 0.25 (2 octaves down)
                float pitchMult = std::pow(0.25f, releaseProgress);
                targetFreq = baseFreq * pitchMult;
            }
            
            // End release phase when envelope is essentially done
            if (envValue < 0.001f) {
                inReleasePhase = false;
            }
        }

        // Apply LFO pitch modulation (if enabled)
        if (pitchDepth > 0.001f) {
            // LFO modulates pitch by ±N octaves where N = pitchDepth
            // lfoBuffer[i] ranges from -1 to +1, so we multiply by pitchDepth to get the octave range
            float octaveShift = lfoBuffer[i] * pitchDepth;
            float pitchMult = std::pow(2.0f, octaveShift);
            targetFreq *= pitchMult;
        }

        // Smooth frequency changes to avoid clicks
        frequencySmooth.setTarget(targetFreq);
        currentFrequency = frequencySmooth.getNext();
        oscillator.setFrequency(currentFrequency);
        oscBuffer[i] = oscillator.generateSample();
    }

    // Copy oscillator output to working buffer
    std::copy(oscBuffer.begin(), oscBuffer.begin() + numFrames, filterBuffer.begin());

    // Apply envelope
    for (int i = 0; i < numFrames; ++i) {
        if (envBuffer[i] < 0.001f) {
            filterBuffer[i] = 0.0f;
        } else {
            filterBuffer[i] *= envBuffer[i];
        }
    }
    
    // Apply delay
    delay.process(filterBuffer.data(), delayBuffer.data(), numFrames);
    std::copy(delayBuffer.begin(), delayBuffer.begin() + numFrames, filterBuffer.begin());
    
    // Apply reverb
    reverb.process(filterBuffer.data(), delayBuffer.data(), numFrames);
    std::copy(delayBuffer.begin(), delayBuffer.begin() + numFrames, filterBuffer.begin());
    
    // Apply DC blocking
    dcBlocker.process(filterBuffer.data(), filterBuffer.data(), numFrames);
    
    // Apply volume and convert to stereo interleaved
    float vol = volume.get();
    for (int i = 0; i < numFrames; ++i) {
        float sample = clamp(filterBuffer[i] * vol, -1.0f, 1.0f);
        output[i * 2] = sample;      // Left
        output[i * 2 + 1] = sample;  // Right
    }
}

void AudioEngine::trigger() {
    std::lock_guard<std::mutex> lock(triggerMutex);
    oscillator.resetPhase();
    envelope.trigger();
    inReleasePhase = false;  // We're in attack/sustain phase
}

void AudioEngine::release() {
    std::lock_guard<std::mutex> lock(triggerMutex);
    // Capture envelope level at start of release for pitch envelope
    pitchEnvStartLevel = envelope.getCurrentValue();
    inReleasePhase = true;  // Start release phase (enables pitch envelope)
    envelope.release();
}

const char* AudioEngine::cyclePitchEnvelope() {
    PitchEnvelopeMode current = pitchEnvMode.get();
    PitchEnvelopeMode next;
    
    switch (current) {
        case PitchEnvelopeMode::None:
            next = PitchEnvelopeMode::Up;
            break;
        case PitchEnvelopeMode::Up:
            next = PitchEnvelopeMode::Down;
            break;
        case PitchEnvelopeMode::Down:
        default:
            next = PitchEnvelopeMode::None;
            break;
    }
    
    pitchEnvMode.set(next);
    
    switch (next) {
        case PitchEnvelopeMode::None: return "none";
        case PitchEnvelopeMode::Up: return "up";
        case PitchEnvelopeMode::Down: return "down";
        default: return "none";
    }
}

// ============================================================================
// Parameter Setters
// ============================================================================

void AudioEngine::setVolume(float vol) {
    volume.set(clamp(vol, 0.0f, 1.0f));
}

void AudioEngine::setFrequency(float freq) {
    baseFrequency.set(clamp(freq, 20.0f, 20000.0f));
}

void AudioEngine::setWaveform(Waveform wf) {
    oscillator.setWaveform(wf);
}

void AudioEngine::setWaveform(int index) {
    setWaveform(static_cast<Waveform>(index % 4));
}

void AudioEngine::setAttackTime(float seconds) {
    envelope.setAttack(seconds);
}

void AudioEngine::setReleaseTime(float seconds) {
    envelope.setRelease(seconds);
}

void AudioEngine::setLfoRate(float rate) {
    lfo.setFrequency(rate);
}

void AudioEngine::setLfoDepth(float depth) {
    lfo.setDepth(depth);
}

void AudioEngine::setLfoPitchDepth(float depth) {
    lfoPitchDepth.set(clamp(depth, 0.0f, 1.0f));
}

void AudioEngine::setLfoWaveform(Waveform wf) {
    lfo.setWaveform(wf);
}

void AudioEngine::setLfoWaveform(int index) {
    setLfoWaveform(static_cast<Waveform>(index % 4));
}

void AudioEngine::setDelayTime(float seconds) {
    delay.setDelayTime(seconds);
}

void AudioEngine::setDelayFeedback(float feedback) {
    delay.setFeedback(feedback);
}

void AudioEngine::setDelayMix(float mix) {
    delay.setDryWet(mix);
}

void AudioEngine::setReverbSize(float size) {
    reverb.setSize(size);
}

void AudioEngine::setReverbMix(float mix) {
    reverb.setDryWet(mix);
}

void AudioEngine::setReverbDamping(float damping) {
    reverb.setDamping(damping);
}

void AudioEngine::setPitchEnvelopeMode(PitchEnvelopeMode mode) {
    pitchEnvMode.set(mode);
}

// ============================================================================
// MP3 Playback Methods
// ============================================================================

bool AudioEngine::enableMP3Mode(const std::string& directory) {
    if (!mp3Player) {
        mp3Player = std::make_unique<AudioFilePlayer>();
    }

    if (mp3Player->loadFilesFromDirectory(directory)) {
        audioMode.set(AudioMode::MP3Playback);
        std::cout << "MP3 mode enabled with " << mp3Player->getFileCount() << " file(s)" << std::endl;
        return true;
    }

    std::cerr << "Failed to load MP3 files from: " << directory << std::endl;
    return false;
}

void AudioEngine::disableMP3Mode() {
    audioMode.set(AudioMode::Synthesis);
    if (mp3Player) {
        mp3Player->stop();
    }
    std::cout << "MP3 mode disabled, returning to synthesis" << std::endl;
}

void AudioEngine::startMP3Playback() {
    if (mp3Player && audioMode.get() == AudioMode::MP3Playback) {
        mp3Player->play();
        std::cout << "Playing MP3: " << mp3Player->getCurrentFileName() << std::endl;
    }
}

void AudioEngine::stopMP3Playback() {
    if (mp3Player) {
        mp3Player->stop();
    }
}

void AudioEngine::selectMP3File(int index) {
    if (mp3Player) {
        mp3Player->selectFile(index);
    }
}

int AudioEngine::getCurrentMP3Index() const {
    if (mp3Player) {
        return mp3Player->getCurrentFileIndex();
    }
    return 0;
}

int AudioEngine::getMP3FileCount() const {
    if (mp3Player) {
        return mp3Player->getFileCount();
    }
    return 0;
}

std::string AudioEngine::getCurrentMP3FileName() const {
    if (mp3Player) {
        return mp3Player->getCurrentFileName();
    }
    return "";
}

bool AudioEngine::hasMP3Finished() const {
    if (mp3Player) {
        return mp3Player->hasFinished();
    }
    return false;
}

AudioFilePlayer::Color AudioEngine::getMP3Color() const {
    if (mp3Player) {
        return mp3Player->getColorForCurrentFile();
    }
    return {255, 255, 255};  // Default white
}

} // namespace DubSiren
