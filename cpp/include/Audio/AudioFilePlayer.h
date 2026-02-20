#pragma once

#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

struct AudioFile {
    std::vector<float> samples;  // Interleaved stereo samples
    std::string filename;
    int sampleRate;
    int channels;

    AudioFile() : sampleRate(48000), channels(2) {}
};

class AudioFilePlayer {
public:
    AudioFilePlayer();
    ~AudioFilePlayer();

    // Load all MP3 files from a directory
    bool loadFilesFromDirectory(const std::string& directory);

    // Get number of loaded files
    int getFileCount() const { return static_cast<int>(audioFiles.size()); }

    // Select which file to play (0-based index)
    void selectFile(int index);

    // Get current file index
    int getCurrentFileIndex() const { return currentFileIndex.load(); }

    // Get current file name
    std::string getCurrentFileName() const;

    // Start playback from beginning
    void play();

    // Stop playback
    void stop();

    // Check if currently playing
    bool isPlaying() const { return playing.load(); }

    // Check if playback finished (reached end of file)
    bool hasFinished() const { return finished.load(); }

    // Reset finished flag
    void resetFinished() { finished.store(false); }

    // Fill audio buffer with samples (called from audio thread)
    void fillBuffer(float* output, int numFrames);

    // Get LED color for current file
    struct Color {
        uint8_t r, g, b;
    };
    Color getColorForCurrentFile() const;

private:
    std::vector<AudioFile> audioFiles;
    std::atomic<int> currentFileIndex{0};
    std::atomic<size_t> playbackPosition{0};
    std::atomic<bool> playing{false};
    std::atomic<bool> finished{false};

    mutable std::mutex filesMutex;

    // Load a single MP3 file
    bool loadMP3File(const std::string& filepath, AudioFile& audioFile);

    // Resample audio if needed (convert to 48kHz stereo)
    void resampleAudio(AudioFile& audioFile, int sourceSampleRate, int sourceChannels);

    // Predefined colors for different files
    static const Color FILE_COLORS[];
    static const int NUM_COLORS;
};
