#pragma once

#include "Common.h"
#include "DSP/Oscillator.h"
#include "DSP/Envelope.h"
#include "DSP/LFO.h"
#include "DSP/Filter.h"
#include "DSP/Delay.h"
#include "DSP/Reverb.h"
#include "Audio/AudioFilePlayer.h"
#include <memory>
#include <mutex>

namespace DubSiren {

/**
 * Audio mode enumeration.
 */
enum class AudioMode {
    Synthesis,      // Normal synthesis mode
    MP3Playback     // MP3 file playback mode
};

/**
 * Main Dub Siren Audio Engine.
 * 
 * Integrates all DSP components and provides a thread-safe interface
 * for parameter control from the GPIO controller.
 */
class AudioEngine {
public:
    explicit AudioEngine(int sampleRate = DEFAULT_SAMPLE_RATE, int bufferSize = DEFAULT_BUFFER_SIZE);
    ~AudioEngine() = default;
    
    // Non-copyable, non-movable (audio engine is stateful)
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    
    /**
     * Generate audio samples.
     * Called from the audio callback thread.
     * @param output Buffer to fill with stereo interleaved samples
     * @param numFrames Number of frames (samples per channel)
     */
    void process(float* output, int numFrames);
    
    /**
     * Trigger the siren sound.
     */
    void trigger();
    
    /**
     * Release the siren sound.
     */
    void release();
    
    /**
     * Cycle through pitch envelope modes.
     * @return The new pitch envelope mode name
     */
    const char* cyclePitchEnvelope();
    
    // ========================================================================
    // Parameter Setters (Thread-Safe)
    // ========================================================================
    
    // Master
    void setVolume(float volume);
    
    // Oscillator
    void setFrequency(float freq);
    void setWaveform(Waveform wf);
    void setWaveform(int index);
    
    // Envelope
    void setAttackTime(float seconds);
    void setReleaseTime(float seconds);
    
    // LFO
    void setLfoRate(float rate);
    void setLfoDepth(float depth);
    void setLfoPitchDepth(float depth);  // LFO modulation depth for pitch (0.0-1.0, where 1.0 = ±1 octave)
    void setLfoWaveform(Waveform wf);
    void setLfoWaveform(int index);
    
    // Delay
    void setDelayTime(float seconds);
    void setDelayFeedback(float feedback);
    void setDelayMix(float mix);
    
    // Reverb
    void setReverbSize(float size);
    void setReverbMix(float mix);
    void setReverbDamping(float damping);
    
    // Pitch Envelope
    void setPitchEnvelopeMode(PitchEnvelopeMode mode);

    // ========================================================================
    // MP3 Playback Mode
    // ========================================================================

    // Enable MP3 playback mode and load files from directory
    bool enableMP3Mode(const std::string& directory);

    // Disable MP3 mode and return to synthesis
    void disableMP3Mode();

    // Check if in MP3 mode
    bool isMP3Mode() const { return audioMode.get() == AudioMode::MP3Playback; }

    // Start MP3 playback
    void startMP3Playback();

    // Stop MP3 playback
    void stopMP3Playback();

    // Select MP3 file by index
    void selectMP3File(int index);

    // Get current MP3 file index
    int getCurrentMP3Index() const;

    // Get number of MP3 files loaded
    int getMP3FileCount() const;

    // Get current MP3 filename
    std::string getCurrentMP3FileName() const;

    // Check if MP3 playback finished
    bool hasMP3Finished() const;

    // Get LED color for current MP3
    AudioFilePlayer::Color getMP3Color() const;

    // ========================================================================
    // Getters
    // ========================================================================

    float getVolume() const { return volume.get(); }
    float getFrequency() const { return baseFrequency.get(); }
    bool isPlaying() const { return envelope.isActive() || envelope.getCurrentValue() > 0.001f; }
    PitchEnvelopeMode getPitchEnvelopeMode() const { return pitchEnvMode.get(); }
    
private:
    int sampleRate;
    int bufferSize;
    
    // DSP Components
    Oscillator oscillator;
    LFO lfo;
    Envelope envelope;
    DCBlocker dcBlocker;
    DelayEffect delay;
    ReverbEffect reverb;

    // MP3 Playback
    std::unique_ptr<AudioFilePlayer> mp3Player;
    
    // Thread-safe parameters
    AudioParameter<float> volume;
    AudioParameter<float> baseFrequency;
    AudioParameter<float> lfoPitchDepth;  // LFO pitch modulation depth
    AudioParameter<PitchEnvelopeMode> pitchEnvMode;
    AudioParameter<AudioMode> audioMode;
    
    // Internal state
    float currentFrequency;
    SmoothedValue frequencySmooth;
    
    // Pitch envelope state
    bool inReleasePhase;
    float pitchEnvStartLevel;  // Envelope level when release started
    
    // Temporary buffers (pre-allocated to avoid allocation in audio thread)
    std::vector<float> oscBuffer;
    std::vector<float> envBuffer;
    std::vector<float> lfoBuffer;
    std::vector<float> processBuffer;
    std::vector<float> delayBuffer;
    
    // Mutex for trigger/release operations
    std::mutex triggerMutex;
};

} // namespace DubSiren
