#include "Audio/AudioFilePlayer.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cstring>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include "../../external/minimp3_ex.h"

namespace fs = std::filesystem;

// Define colors for different MP3 files
const AudioFilePlayer::Color AudioFilePlayer::FILE_COLORS[] = {
    {255, 255, 255},  // White (first file)
    {255, 0, 255},    // Magenta
    {0, 255, 255},    // Cyan
    {255, 128, 0},    // Orange
    {128, 0, 255},    // Purple
    {255, 255, 0},    // Yellow
    {0, 255, 128},    // Spring Green
    {255, 0, 128},    // Rose
};

const int AudioFilePlayer::NUM_COLORS = sizeof(FILE_COLORS) / sizeof(FILE_COLORS[0]);

AudioFilePlayer::AudioFilePlayer() {
}

AudioFilePlayer::~AudioFilePlayer() {
}

bool AudioFilePlayer::loadFilesFromDirectory(const std::string& directory) {
    std::lock_guard<std::mutex> lock(filesMutex);

    audioFiles.clear();

    try {
        if (!fs::exists(directory) || !fs::is_directory(directory)) {
            std::cerr << "Directory does not exist: " << directory << std::endl;
            return false;
        }

        // Collect all MP3 files
        std::vector<std::string> mp3Files;
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                if (extension == ".mp3") {
                    mp3Files.push_back(entry.path().string());
                }
            }
        }

        // Sort files alphabetically
        std::sort(mp3Files.begin(), mp3Files.end());

        // Load each MP3 file
        for (const auto& filepath : mp3Files) {
            AudioFile audioFile;
            if (loadMP3File(filepath, audioFile)) {
                audioFile.filename = fs::path(filepath).filename().string();
                audioFiles.push_back(std::move(audioFile));
                std::cout << "Loaded MP3: " << audioFile.filename << std::endl;
            }
        }

        if (audioFiles.empty()) {
            std::cerr << "No MP3 files found in directory: " << directory << std::endl;
            return false;
        }

        currentFileIndex.store(0);
        playbackPosition.store(0);
        finished.store(false);

        std::cout << "Loaded " << audioFiles.size() << " MP3 file(s)" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error loading MP3 files: " << e.what() << std::endl;
        return false;
    }
}

bool AudioFilePlayer::loadMP3File(const std::string& filepath, AudioFile& audioFile) {
    mp3dec_t mp3d;
    mp3dec_file_info_t info;

    // Decode entire MP3 file
    if (mp3dec_load(&mp3d, filepath.c_str(), &info, NULL, NULL) != 0) {
        std::cerr << "Failed to decode MP3: " << filepath << std::endl;
        return false;
    }

    if (info.samples == 0) {
        std::cerr << "MP3 file is empty: " << filepath << std::endl;
        free(info.buffer);
        return false;
    }

    // Store original sample rate and channels
    int sourceSampleRate = info.hz;
    int sourceChannels = info.channels;

    // Copy samples (minimp3 with MINIMP3_FLOAT_OUTPUT gives us float samples)
    audioFile.samples.resize(info.samples);
    std::memcpy(audioFile.samples.data(), info.buffer, info.samples * sizeof(float));

    // Free the mp3dec buffer
    free(info.buffer);

    // Resample to 48kHz stereo if needed
    if (sourceSampleRate != 48000 || sourceChannels != 2) {
        resampleAudio(audioFile, sourceSampleRate, sourceChannels);
    }

    audioFile.sampleRate = 48000;
    audioFile.channels = 2;

    return true;
}

void AudioFilePlayer::resampleAudio(AudioFile& audioFile, int sourceSampleRate, int sourceChannels) {
    // Simple resampling: convert to stereo and/or change sample rate
    std::vector<float> resampled;

    size_t sourceFrames = audioFile.samples.size() / sourceChannels;

    // Calculate target number of frames for 48kHz
    size_t targetFrames;
    if (sourceSampleRate != 48000) {
        targetFrames = (sourceFrames * 48000) / sourceSampleRate;
    } else {
        targetFrames = sourceFrames;
    }

    resampled.resize(targetFrames * 2);  // Stereo output

    for (size_t i = 0; i < targetFrames; i++) {
        // Find corresponding source frame (simple linear interpolation)
        float sourcePos = (float)i * sourceSampleRate / 48000.0f;
        size_t sourceFrame = (size_t)sourcePos;

        if (sourceFrame >= sourceFrames) {
            sourceFrame = sourceFrames - 1;
        }

        float left, right;

        if (sourceChannels == 1) {
            // Mono to stereo: duplicate channel
            left = right = audioFile.samples[sourceFrame];
        } else {
            // Stereo: copy both channels
            left = audioFile.samples[sourceFrame * 2];
            right = audioFile.samples[sourceFrame * 2 + 1];
        }

        resampled[i * 2] = left;
        resampled[i * 2 + 1] = right;
    }

    audioFile.samples = std::move(resampled);
}

void AudioFilePlayer::selectFile(int index) {
    std::lock_guard<std::mutex> lock(filesMutex);

    if (index >= 0 && index < static_cast<int>(audioFiles.size())) {
        currentFileIndex.store(index);
        playbackPosition.store(0);
        finished.store(false);
        std::cout << "Selected MP3: " << audioFiles[index].filename << std::endl;
    }
}

std::string AudioFilePlayer::getCurrentFileName() const {
    std::lock_guard<std::mutex> lock(filesMutex);

    int index = currentFileIndex.load();
    if (index >= 0 && index < static_cast<int>(audioFiles.size())) {
        return audioFiles[index].filename;
    }
    return "";
}

void AudioFilePlayer::play() {
    playbackPosition.store(0);
    finished.store(false);
    playing.store(true);
}

void AudioFilePlayer::stop() {
    playing.store(false);
}

void AudioFilePlayer::fillBuffer(float* output, int numFrames) {
    if (!playing.load()) {
        // Silence
        std::memset(output, 0, numFrames * 2 * sizeof(float));
        return;
    }

    std::lock_guard<std::mutex> lock(filesMutex);

    int index = currentFileIndex.load();
    if (index < 0 || index >= static_cast<int>(audioFiles.size())) {
        std::memset(output, 0, numFrames * 2 * sizeof(float));
        return;
    }

    const AudioFile& file = audioFiles[index];
    size_t pos = playbackPosition.load();
    size_t totalSamples = file.samples.size();  // Interleaved stereo

    int samplesNeeded = numFrames * 2;  // Stereo
    int samplesCopied = 0;

    while (samplesCopied < samplesNeeded && pos < totalSamples) {
        output[samplesCopied++] = file.samples[pos++];
    }

    // Fill remaining with silence if we reached the end
    if (samplesCopied < samplesNeeded) {
        std::memset(output + samplesCopied, 0, (samplesNeeded - samplesCopied) * sizeof(float));
        playing.store(false);
        finished.store(true);
    }

    playbackPosition.store(pos);
}

AudioFilePlayer::Color AudioFilePlayer::getColorForCurrentFile() const {
    int index = currentFileIndex.load();
    return FILE_COLORS[index % NUM_COLORS];
}
