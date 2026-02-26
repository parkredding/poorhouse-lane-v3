#pragma once

#include "Common.h"
#include "Audio/AudioEngine.h"
#include "Hardware/LEDController.h"
#include <functional>
#include <thread>
#include <atomic>
#include <array>
#include <map>
#include <mutex>
#include <memory>
#include <vector>
#include <chrono>

namespace DubSiren {

/**
 * GPIO pin assignments (BCM numbering).
 * These pins avoid I2S pins (18, 19, 21) used by PCM5102 DAC.
 */
namespace GPIO {
    // Encoder pins (CLK, DT) - swapped to match EC11 PCB footprint
    constexpr int ENCODER_1_CLK = 2;
    constexpr int ENCODER_1_DT = 17;
    constexpr int ENCODER_2_CLK = 22;
    constexpr int ENCODER_2_DT = 27;
    constexpr int ENCODER_3_CLK = 24;
    constexpr int ENCODER_3_DT = 23;
    constexpr int ENCODER_4_CLK = 26;
    constexpr int ENCODER_4_DT = 20;
    constexpr int ENCODER_5_CLK = 13;
    constexpr int ENCODER_5_DT = 14;
    
    // Button pins
    constexpr int TRIGGER_BTN = 4;
    constexpr int SHIFT_BTN = 15;
    constexpr int SHUTDOWN_BTN = 3;
    
    // 3-position switch pins (ON/OFF/ON for pitch envelope) - swapped to match PCB footprint
    constexpr int PITCH_ENV_UP = 9;     // Pin 21
    constexpr int PITCH_ENV_DOWN = 10;  // Pin 19
    
    // Waveform cycle button
    constexpr int WAVEFORM_BTN = 5;     // Pin 29 - cycles sine/square/saw/tri

    // Optional WS2812 LED data pin (supports PWM)
    constexpr int LED_DATA = 12;        // Pin 32 (PWM0)
}

/**
 * Parameter bank enumeration.
 */
enum class Bank {
    A,  // Normal mode
    B   // Shift held
};

/**
 * Rotary encoder handler with quadrature decoding.
 */
class RotaryEncoder {
public:
    using Callback = std::function<void(int direction)>;
    
    RotaryEncoder(int clkPin, int dtPin, Callback callback);
    ~RotaryEncoder();
    
    void start();
    void stop();
    int getPosition() const { return position.load(); }
    
private:
    int clkPin;
    int dtPin;
    Callback callback;
    std::atomic<int> position;
    std::atomic<bool> running;
    std::thread pollThread;
    
    int lastClk;
    int lastDt;
    
    void pollLoop();
    void update();
};

/**
 * Momentary switch handler with debouncing.
 */
class MomentarySwitch {
public:
    using PressCallback = std::function<void()>;
    using ReleaseCallback = std::function<void()>;
    
    MomentarySwitch(int pin, PressCallback onPress = nullptr, ReleaseCallback onRelease = nullptr);
    ~MomentarySwitch();
    
    void start();
    void stop();
    bool isPressed() const { return pressed.load(); }
    
private:
    int pin;
    PressCallback pressCallback;
    ReleaseCallback releaseCallback;
    std::atomic<bool> pressed;
    std::atomic<bool> running;
    std::thread pollThread;
    
    int lastState;
    std::chrono::steady_clock::time_point lastChange;
    std::chrono::steady_clock::time_point lastPressTime;
    
    static constexpr int DEBOUNCE_MS = 10;
    static constexpr int MIN_PRESS_MS = 30;
    
    void pollLoop();
};

/**
 * Three-position switch (ON/OFF/ON) handler with debouncing.
 * Used for pitch envelope selection: UP / OFF / DOWN
 */
enum class SwitchPosition {
    Off,    // Middle position (neither terminal connected)
    Up,     // Upper ON position
    Down    // Lower ON position
};

class ThreePositionSwitch {
public:
    using PositionCallback = std::function<void(SwitchPosition)>;
    
    ThreePositionSwitch(int upPin, int downPin, PositionCallback onChange = nullptr);
    ~ThreePositionSwitch();
    
    void start();
    void stop();
    SwitchPosition getPosition() const { return position.load(); }
    
private:
    int upPin;
    int downPin;
    PositionCallback callback;
    std::atomic<SwitchPosition> position;
    std::atomic<bool> running;
    std::thread pollThread;
    
    SwitchPosition lastPosition;
    std::chrono::steady_clock::time_point lastChange;
    
    static constexpr int DEBOUNCE_MS = 20;
    
    void pollLoop();
    SwitchPosition readPosition();
};

/**
 * Secret mode enumeration.
 * Triggered by rapidly pressing the shift button or toggling pitch envelope.
 */
enum class SecretMode {
    None,       // Normal operation
    PitchDelay, // Pitch-delay linked mode (3 rapid presses)
    NJD,        // Classic NJD siren mode (5 rapid presses)
    UFO,        // UFO/Sci-fi mode (10 rapid presses)
    MP3         // MP3 playback mode (5 rapid pitch envelope toggles)
};

/**
 * Control surface handler for the Dub Siren.
 *
 * 5 Encoders with bank switching:
 * - Bank A: LFO Depth, Base Freq, Filter Freq, Delay Feedback, Reverb Mix
 * - Bank B: LFO Rate, Delay Time, Filter Res, Osc Waveform, Reverb Size
 *
 * 5 Buttons: Trigger, Pitch Envelope, Shift, Shutdown, Waveform Cycle
 *
 * Secret Modes (triggered by rapid shift button presses):
 * - Pitch-Delay Mode: 3 rapid presses - Links pitch and delay inversely
 * - NJD Mode: 5 rapid presses - Classic dub siren presets
 * - UFO Mode: 10 rapid presses - Sci-fi UFO presets
 */
class GPIOController {
public:
    using ShutdownCallback = std::function<void()>;
    
    GPIOController(AudioEngine& engine, ShutdownCallback shutdownCb = nullptr);
    ~GPIOController();
    
    /**
     * Start the control surface.
     */
    void start();
    
    /**
     * Stop the control surface and cleanup GPIO.
     */
    void stop();
    
    /**
     * Get current bank.
     */
    Bank getCurrentBank() const { return currentBank.load(); }
    
    /**
     * Check if control surface is running.
     */
    bool isRunning() const { return running.load(); }
    
    /**
     * Update LED with current audio level (0.0 - 1.0) for sound-reactive pulsing.
     * Call this from the audio callback with the current output level.
     */
    void updateLEDAudioLevel(float level);

    /**
     * Check if MP3 playback has finished and auto-exit mode.
     * Should be called periodically from main loop or LED update thread.
     */
    void checkMP3PlaybackStatus();
    
    /**
     * Get the LED controller (may be nullptr if not available).
     */
    LEDController* getLEDController() { return ledController.get(); }
    
private:
    AudioEngine& engine;
    ShutdownCallback shutdownCallback;
    std::atomic<bool> running;
    std::atomic<Bank> currentBank;
    std::atomic<bool> shiftPressed;
    
    // Secret mode state
    std::atomic<SecretMode> secretMode;
    std::atomic<int> secretModePreset{0};  // Current preset within secret mode (0-indexed)

    // Shift button press tracking for secret mode activation
    // Protected by pressesMutex for thread-safe access
    mutable std::mutex pressesMutex;
    std::vector<std::chrono::steady_clock::time_point> recentShiftPresses;

    // Pitch envelope toggle tracking for MP3 mode activation
    // Protected by pitchEnvMutex for thread-safe access
    mutable std::mutex pitchEnvMutex;
    std::vector<std::chrono::steady_clock::time_point> recentPitchEnvToggles;
    SwitchPosition lastPitchEnvPosition{SwitchPosition::Off};
    
    // Parameter values
    struct Parameters {
        // Bank A (Auto Wail preset)
        float volume = 0.6f;       // Master volume (encoder 3, Bank A)
        float lfoDepth = 0.5f;     // LFO modulation depth
        float baseFreq = 440.0f;   // A4 - standard siren pitch
        float delayFeedback = 0.55f;  // Spacey dub echoes
        float reverbMix = 0.4f;    // Wet for atmosphere

        // Bank B (Auto Wail preset)
        float lfoRate = 0.35f;     // Slow swell - one full cycle ~2.9 seconds
        float delayTime = 0.375f;  // Dotted eighth - classic dub
        int oscWaveform = 1;  // Square for classic siren sound
        float reverbSize = 0.7f;   // Large dub space
        float release = 0.5f;      // Release time (encoder 3, Bank B, logarithmic 0.01s-3.0s)
    };
    Parameters params;
    
    // Hardware components
    std::array<std::unique_ptr<RotaryEncoder>, 5> encoders;
    std::array<std::unique_ptr<MomentarySwitch>, 4> buttons;  // Trigger, Shift, Shutdown, Waveform
    std::unique_ptr<ThreePositionSwitch> pitchEnvSwitch;      // 3-position pitch envelope
    std::unique_ptr<LEDController> ledController;             // Optional WS2812 status LED
    
    // Encoder handlers
    void handleEncoder(int encoderIndex, int direction);
    
    // Button handlers
    void onTriggerPress();
    void onTriggerRelease();
    void onShiftPress();
    void onShiftRelease();
    void onShutdownPress();
    void onWaveformPress();

    // Pitch envelope switch handler
    void onPitchEnvChange(SwitchPosition position);
    
    // Secret mode handling
    void checkSecretModeActivation();
    void checkPitchEnvMP3Activation();
    void activateSecretMode(SecretMode mode);
    void exitSecretMode();
    void cycleSecretModePreset();
    void applySecretModePreset();
    
    // Apply parameter to engine
    void applyParameter(const char* name, float value);
    
    // Initialize GPIO (platform-specific)
    bool initGPIO();
    void cleanupGPIO();
};

/**
 * Simulated control surface for testing without GPIO hardware.
 */
class SimulatedController {
public:
    SimulatedController(AudioEngine& engine);
    
    void start();
    void stop();
    void processCommand(char cmd);
    void printHelp();
    
private:
    AudioEngine& engine;
    std::atomic<bool> running;
};

} // namespace DubSiren
