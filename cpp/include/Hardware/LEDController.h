#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>
#include <functional>
#include <random>

namespace DubSiren {

/**
 * RGB color structure for LED control
 */
struct Color {
    uint8_t r, g, b;
    
    Color() : r(0), g(0), b(0) {}
    Color(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
    
    // Predefined colors
    static Color Black()   { return Color(0, 0, 0); }
    static Color White()   { return Color(255, 255, 255); }
    static Color Red()     { return Color(255, 0, 0); }
    static Color Green()   { return Color(0, 255, 0); }
    static Color Blue()    { return Color(0, 0, 255); }
    static Color Yellow()  { return Color(255, 255, 0); }
    static Color Cyan()    { return Color(0, 255, 255); }
    static Color Magenta() { return Color(255, 0, 255); }
    static Color Orange()  { return Color(255, 165, 0); }
    static Color Purple()  { return Color(128, 0, 128); }
    
    // Startup colors
    static Color Amber()     { return Color(255, 191, 0); }
    static Color LimeGreen() { return Color(50, 205, 50); }
    
    // Rasta colors (for NJD mode)
    static Color RastaRed()    { return Color(255, 0, 0); }
    static Color RastaYellow() { return Color(255, 255, 0); }
    static Color RastaGreen()  { return Color(0, 128, 0); }
    
    // UFO colors
    static Color UFOGreen()  { return Color(57, 255, 20); }   // Neon green
    static Color UFOPurple() { return Color(138, 43, 226); }  // Blue violet
    static Color UFOCyan()   { return Color(0, 255, 255); }   // Alien cyan
    
    // Linear interpolation between colors
    static Color lerp(const Color& a, const Color& b, float t) {
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return Color(
            static_cast<uint8_t>(a.r + (b.r - a.r) * t),
            static_cast<uint8_t>(a.g + (b.g - a.g) * t),
            static_cast<uint8_t>(a.b + (b.b - a.b) * t)
        );
    }
    
    // Scale brightness (0.0 - 1.0)
    Color scaled(float brightness) const {
        brightness = brightness < 0.0f ? 0.0f : (brightness > 1.0f ? 1.0f : brightness);
        return Color(
            static_cast<uint8_t>(r * brightness),
            static_cast<uint8_t>(g * brightness),
            static_cast<uint8_t>(b * brightness)
        );
    }
};

/**
 * LED display mode
 */
enum class LEDMode {
    Startup,        // Amber during boot, lime green when ready
    Normal,         // Slow color cycling over minutes
    NJD,            // Rasta colors, faster cycling
    UFO,            // Green/purple alien theme
    MP3             // Slow flash pattern for MP3 mode
};

/**
 * Color cycle path - different color journeys for variety
 */
enum class ColorPath {
    SunsetToOcean,      // Orange → Pink → Purple → Blue → Cyan
    ForestMist,         // Green → Teal → Blue → Purple → Green
    FireAndIce,         // Red → Orange → White → Cyan → Blue
    NeonNights,         // Pink → Purple → Blue → Cyan → Green
    GoldenHour,         // Gold → Orange → Rose → Magenta → Violet
    DeepSpace,          // Blue → Purple → Black → Blue → Cyan
    TropicalDream,      // Cyan → Turquoise → Green → Yellow → Orange
    AuroraBorealis,     // Green → Cyan → Blue → Purple → Pink
    VolcanicGlow,       // Red → Orange → Yellow → White → Red
    MidnightBloom,      // Purple → Blue → Cyan → Pink → Purple
    
    COUNT               // Number of paths
};

/**
 * WS2812 LED Controller for status indication
 * 
 * Features:
 * - Startup indication (amber → lime green)
 * - Slow color cycling in normal mode (5-10 different paths)
 * - Sound-reactive pulsing
 * - Fast rasta cycling in NJD mode
 * - Green/purple cycling in UFO mode
 */
class LEDController {
public:
    // GPIO pin for WS2812 data (recommend GPIO 12 - supports PWM)
    static constexpr int DEFAULT_LED_PIN = 12;
    
    LEDController(int dataPin = DEFAULT_LED_PIN);
    ~LEDController();
    
    // Lifecycle
    bool init();
    void start();
    void stop();
    
    // Mode control
    void setMode(LEDMode mode);
    LEDMode getMode() const { return currentMode.load(); }
    
    // Sound reactivity - call this with audio level (0.0 - 1.0)
    void setAudioLevel(float level);
    
    // Startup sequence
    void showStartupColor();   // Amber - call when Pi boots
    void showReadyColor();     // Lime green - call when siren is ready
    
    // Direct color control (overrides cycling)
    void setColor(const Color& color);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void setColorWithPulse(const Color& color, float pulseIntensity);
    
    // Configuration
    void setBrightness(float brightness);  // 0.0 - 1.0
    void setCycleSpeed(float speedMultiplier);  // 1.0 = normal
    
    // Check if LED is available
    bool isAvailable() const { return ledAvailable; }
    
private:
    int dataPin;
    std::atomic<bool> running;
    std::atomic<bool> ledAvailable;
    std::thread updateThread;
    
    // Current state
    std::atomic<LEDMode> currentMode;
    std::atomic<float> audioLevel;
    std::atomic<float> brightness;
    std::atomic<float> cycleSpeed;
    
    // Platform-specific LED handle (must be before rng for initialization order)
    void* ledHandle;  // ws2811_t* on Pi, nullptr on other platforms
    
    // Color cycling state
    ColorPath currentPath;
    float cyclePosition;         // 0.0 - 1.0 within current cycle
    std::chrono::steady_clock::time_point lastUpdate;
    std::mt19937 rng;
    
    // Direct color override
    std::atomic<bool> colorOverride;
    Color overrideColor;
    
    // Startup transition timer (to avoid detached thread lifetime issues)
    std::atomic<bool> pendingReadyTransition;
    std::chrono::steady_clock::time_point readyTransitionTime;
    
    // Update loop
    void updateLoop();
    
    // Color calculation
    Color calculateColor();
    Color getNormalModeColor();
    Color getNJDModeColor();
    Color getUFOModeColor();
    Color getMP3ModeColor();
    
    // Apply audio pulse to color
    Color applyAudioPulse(const Color& baseColor);
    
    // Get color from path at position (0.0 - 1.0)
    Color getPathColor(ColorPath path, float position);
    
    // Select a new random path
    void selectRandomPath();
    
    // Platform-specific
    bool initPlatformLED();
    void cleanupPlatformLED();
    void writeLED(const Color& color);
    
    // Cycle timing (in seconds)
    static constexpr float NORMAL_CYCLE_DURATION = 180.0f;  // 3 minutes per full cycle
    static constexpr float NJD_CYCLE_DURATION = 3.0f;       // 3 seconds per rasta cycle
    static constexpr float UFO_CYCLE_DURATION = 5.0f;       // 5 seconds per UFO cycle
    static constexpr float MP3_CYCLE_DURATION = 2.0f;       // 2 seconds slow flash (1s on, 1s off)
    static constexpr float PATH_CHANGE_PROBABILITY = 0.1f;  // 10% chance to change path at cycle end
    
    // Audio pulse settings
    static constexpr float PULSE_ATTACK = 0.1f;   // Fast attack
    static constexpr float PULSE_DECAY = 0.3f;    // Slower decay
    float smoothedAudioLevel = 0.0f;
};

} // namespace DubSiren
