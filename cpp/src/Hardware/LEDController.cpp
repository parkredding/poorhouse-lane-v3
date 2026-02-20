#include "Hardware/LEDController.h"
#include <iostream>
#include <cmath>
#include <cstring>
#include <algorithm>

// Include ws2811 library only if CMake found it and defined HAVE_WS2812
#ifdef HAVE_WS2812
    #include <ws2811/ws2811.h>
#endif

namespace DubSiren {

// ============================================================================
// Constructor / Destructor
// ============================================================================

LEDController::LEDController(int pin)
    : dataPin(pin)
    , running(false)
    , ledAvailable(false)
    , currentMode(LEDMode::Startup)
    , audioLevel(0.0f)
    , brightness(0.2f)  // 25% brightness (dimmed another 50% from 0.4)
    , cycleSpeed(1.0f)
    , currentPath(ColorPath::SunsetToOcean)
    , cyclePosition(0.0f)
    , colorOverride(false)
    , pendingReadyTransition(false)
    , ledHandle(nullptr)
    , rng(std::random_device{}())
{
}

LEDController::~LEDController() {
    stop();
    cleanupPlatformLED();
}

// ============================================================================
// Platform-specific LED control
// ============================================================================

bool LEDController::initPlatformLED() {
#ifdef HAVE_WS2812
    ws2811_t* led = new ws2811_t();
    memset(led, 0, sizeof(ws2811_t));
    
    led->freq = WS2811_TARGET_FREQ;
    led->dmanum = 10;  // DMA channel
    
    // Channel 0 configuration (we only have 1 LED)
    led->channel[0].gpionum = dataPin;
    led->channel[0].invert = 0;
    led->channel[0].count = 1;
    led->channel[0].strip_type = WS2812_STRIP;
    led->channel[0].brightness = 255;
    
    // Channel 1 not used
    led->channel[1].gpionum = 0;
    led->channel[1].invert = 0;
    led->channel[1].count = 0;
    led->channel[1].brightness = 0;
    
    ws2811_return_t result = ws2811_init(led);
    if (result != WS2811_SUCCESS) {
        std::cerr << "LED: WS2811 init failed: " << ws2811_get_return_t_str(result) << std::endl;
        delete led;
        return false;
    }
    
    ledHandle = led;
    std::cout << "LED: WS2812 initialized on GPIO " << dataPin << std::endl;
    return true;
#else
    std::cout << "LED: WS2812 not available on this platform (simulated)" << std::endl;
    return true;  // Return true to allow simulation
#endif
}

void LEDController::cleanupPlatformLED() {
#ifdef HAVE_WS2812
    if (ledHandle) {
        ws2811_t* led = static_cast<ws2811_t*>(ledHandle);
        
        // Turn off LED
        led->channel[0].leds[0] = 0;
        ws2811_render(led);
        
        ws2811_fini(led);
        delete led;
        ledHandle = nullptr;
    }
#endif
}

void LEDController::writeLED(const Color& color) {
#ifdef HAVE_WS2812
    if (ledHandle) {
        ws2811_t* led = static_cast<ws2811_t*>(ledHandle);
        
        // WS2812 uses GRB format internally, but ws2811 library handles this
        // Pack as 0x00RRGGBB
        uint32_t packedColor = (static_cast<uint32_t>(color.r) << 16) |
                               (static_cast<uint32_t>(color.g) << 8) |
                               static_cast<uint32_t>(color.b);
        
        led->channel[0].leds[0] = packedColor;
        ws2811_render(led);
    }
#else
    // Simulation mode - just log occasionally
    static int logCounter = 0;
    if (++logCounter >= 100) {  // Log every ~1 second at 100Hz
        std::cout << "LED (sim): RGB(" << (int)color.r << ", " 
                  << (int)color.g << ", " << (int)color.b << ")" << std::endl;
        logCounter = 0;
    }
#endif
}

// ============================================================================
// Lifecycle
// ============================================================================

bool LEDController::init() {
    ledAvailable = initPlatformLED();
    
    // Show startup color immediately
    if (ledAvailable) {
        showStartupColor();
    }
    
    return ledAvailable;
}

void LEDController::start() {
    if (running.load()) return;
    
    running.store(true);
    lastUpdate = std::chrono::steady_clock::now();
    
    // Select initial random path
    selectRandomPath();
    
    updateThread = std::thread(&LEDController::updateLoop, this);
    
    std::cout << "LED: Controller started" << std::endl;
}

void LEDController::stop() {
    if (!running.load()) return;
    
    running.store(false);
    
    // Cancel any pending transitions
    pendingReadyTransition.store(false);
    
    if (updateThread.joinable()) {
        updateThread.join();
    }
    
    // Turn off LED
    writeLED(Color::Black());
    
    std::cout << "LED: Controller stopped" << std::endl;
}

// ============================================================================
// Mode Control
// ============================================================================

void LEDController::setMode(LEDMode mode) {
    LEDMode oldMode = currentMode.exchange(mode);
    
    if (oldMode != mode) {
        // Reset cycle position when mode changes
        cyclePosition = 0.0f;
        colorOverride.store(false);
        
        const char* modeName = "Unknown";
        switch (mode) {
            case LEDMode::Startup: modeName = "Startup"; break;
            case LEDMode::Normal: modeName = "Normal"; break;
            case LEDMode::NJD: modeName = "NJD (Rasta)"; break;
            case LEDMode::UFO: modeName = "UFO (Alien)"; break;
            case LEDMode::MP3: modeName = "MP3 (Flash)"; break;
        }
        std::cout << "LED: Mode changed to " << modeName << std::endl;
    }
}

void LEDController::setAudioLevel(float level) {
    audioLevel.store(std::clamp(level, 0.0f, 1.0f));
}

void LEDController::showStartupColor() {
    colorOverride.store(true);
    overrideColor = Color::Amber();
    writeLED(overrideColor.scaled(brightness.load()));
}

void LEDController::showReadyColor() {
    colorOverride.store(true);
    overrideColor = Color::LimeGreen();
    writeLED(overrideColor.scaled(brightness.load()));
    
    // Schedule transition to normal mode after 2 seconds
    // This is handled in updateLoop() to avoid detached thread lifetime issues
    readyTransitionTime = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    pendingReadyTransition.store(true);
}

void LEDController::setColor(const Color& color) {
    colorOverride.store(true);
    overrideColor = color;
}

void LEDController::setColor(uint8_t r, uint8_t g, uint8_t b) {
    setColor(Color(r, g, b));
}

void LEDController::setColorWithPulse(const Color& color, float pulseIntensity) {
    colorOverride.store(true);
    overrideColor = color;
    audioLevel.store(pulseIntensity);
}

void LEDController::setBrightness(float b) {
    brightness.store(std::clamp(b, 0.0f, 1.0f));
}

void LEDController::setCycleSpeed(float speed) {
    cycleSpeed.store(std::max(0.1f, speed));
}

// ============================================================================
// Update Loop
// ============================================================================

void LEDController::updateLoop() {
    constexpr auto updateInterval = std::chrono::milliseconds(10);  // 100Hz update rate
    
    while (running.load()) {
        auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastUpdate).count();
        lastUpdate = now;
        
        // Check for pending ready transition (from showReadyColor)
        if (pendingReadyTransition.load() && now >= readyTransitionTime) {
            colorOverride.store(false);
            currentMode.store(LEDMode::Normal);
            pendingReadyTransition.store(false);
            std::cout << "LED: Transition to normal mode complete" << std::endl;
        }
        
        // Calculate and apply color
        Color finalColor = calculateColor();
        finalColor = finalColor.scaled(brightness.load());
        writeLED(finalColor);
        
        // Update cycle position based on mode
        LEDMode mode = currentMode.load();
        float cycleDuration = NORMAL_CYCLE_DURATION;
        
        switch (mode) {
            case LEDMode::NJD:
                cycleDuration = NJD_CYCLE_DURATION;
                break;
            case LEDMode::UFO:
                cycleDuration = UFO_CYCLE_DURATION;
                break;
            case LEDMode::MP3:
                cycleDuration = MP3_CYCLE_DURATION;
                break;
            default:
                cycleDuration = NORMAL_CYCLE_DURATION;
                break;
        }
        
        // Advance cycle position
        cyclePosition += (deltaTime / cycleDuration) * cycleSpeed.load();
        
        // Handle cycle wrap and path changes
        if (cyclePosition >= 1.0f) {
            cyclePosition = std::fmod(cyclePosition, 1.0f);
            
            // In normal mode, potentially change to a new color path
            if (mode == LEDMode::Normal) {
                std::uniform_real_distribution<float> dist(0.0f, 1.0f);
                if (dist(rng) < PATH_CHANGE_PROBABILITY) {
                    selectRandomPath();
                }
            }
        }
        
        // Smooth audio level for pulse effect
        float targetAudio = audioLevel.load();
        if (targetAudio > smoothedAudioLevel) {
            smoothedAudioLevel += (targetAudio - smoothedAudioLevel) * PULSE_ATTACK * 60.0f * deltaTime;
        } else {
            smoothedAudioLevel += (targetAudio - smoothedAudioLevel) * PULSE_DECAY * 60.0f * deltaTime;
        }
        smoothedAudioLevel = std::clamp(smoothedAudioLevel, 0.0f, 1.0f);
        
        std::this_thread::sleep_for(updateInterval);
    }
}

// ============================================================================
// Color Calculation
// ============================================================================

Color LEDController::calculateColor() {
    // Check for color override
    if (colorOverride.load()) {
        return applyAudioPulse(overrideColor);
    }
    
    Color baseColor;
    
    switch (currentMode.load()) {
        case LEDMode::Startup:
            baseColor = Color::Amber();
            break;
        case LEDMode::Normal:
            baseColor = getNormalModeColor();
            break;
        case LEDMode::NJD:
            baseColor = getNJDModeColor();
            break;
        case LEDMode::UFO:
            baseColor = getUFOModeColor();
            break;
        case LEDMode::MP3:
            baseColor = getMP3ModeColor();
            break;
    }
    
    return applyAudioPulse(baseColor);
}

Color LEDController::getNormalModeColor() {
    return getPathColor(currentPath, cyclePosition);
}

Color LEDController::getNJDModeColor() {
    // Rasta colors: Red → Yellow → Green → Yellow → Red
    // Creates a smooth loop through rasta flag colors
    
    float pos = cyclePosition;
    
    if (pos < 0.25f) {
        // Red to Yellow
        return Color::lerp(Color::RastaRed(), Color::RastaYellow(), pos * 4.0f);
    } else if (pos < 0.5f) {
        // Yellow to Green
        return Color::lerp(Color::RastaYellow(), Color::RastaGreen(), (pos - 0.25f) * 4.0f);
    } else if (pos < 0.75f) {
        // Green to Yellow
        return Color::lerp(Color::RastaGreen(), Color::RastaYellow(), (pos - 0.5f) * 4.0f);
    } else {
        // Yellow to Red
        return Color::lerp(Color::RastaYellow(), Color::RastaRed(), (pos - 0.75f) * 4.0f);
    }
}

Color LEDController::getUFOModeColor() {
    // UFO theme: Neon Green → Purple → Cyan → Purple → Green
    // Alien, sci-fi feel
    
    float pos = cyclePosition;
    
    if (pos < 0.2f) {
        // Green to Purple
        return Color::lerp(Color::UFOGreen(), Color::UFOPurple(), pos * 5.0f);
    } else if (pos < 0.4f) {
        // Purple to Cyan
        return Color::lerp(Color::UFOPurple(), Color::UFOCyan(), (pos - 0.2f) * 5.0f);
    } else if (pos < 0.6f) {
        // Cyan to Purple
        return Color::lerp(Color::UFOCyan(), Color::UFOPurple(), (pos - 0.4f) * 5.0f);
    } else if (pos < 0.8f) {
        // Purple to Green (with slight blue tint)
        Color blueGreen(0, 200, 150);
        return Color::lerp(Color::UFOPurple(), blueGreen, (pos - 0.6f) * 5.0f);
    } else {
        // Blue-green back to neon green
        Color blueGreen(0, 200, 150);
        return Color::lerp(blueGreen, Color::UFOGreen(), (pos - 0.8f) * 5.0f);
    }
}

Color LEDController::getMP3ModeColor() {
    // MP3 mode: Slow flash between full brightness and off (using override color or white)
    // Cycle position goes from 0.0 to 1.0
    // Flash pattern: fade in for 1 second, fade out for 1 second

    float pos = cyclePosition;
    float brightness;

    if (pos < 0.5f) {
        // First half: fade in from 0 to 1
        brightness = pos * 2.0f;
    } else {
        // Second half: fade out from 1 to 0
        brightness = (1.0f - pos) * 2.0f;
    }

    // Use override color if set, otherwise white
    Color baseColor = colorOverride.load() ? overrideColor : Color::White();
    return baseColor.scaled(brightness);
}

Color LEDController::applyAudioPulse(const Color& baseColor) {
    if (smoothedAudioLevel < 0.01f) {
        return baseColor;
    }
    
    // Pulse makes the color brighter based on audio level
    // At full audio, color becomes nearly white
    float pulseAmount = smoothedAudioLevel * 0.5f;  // Max 50% toward white
    
    return Color::lerp(baseColor, Color::White(), pulseAmount);
}

Color LEDController::getPathColor(ColorPath path, float position) {
    // Each path is a journey through 5 colors
    // Position 0.0 - 1.0 moves through all 5 colors and back to start
    
    // Define color waypoints for each path
    Color colors[5];
    
    switch (path) {
        case ColorPath::SunsetToOcean:
            colors[0] = Color(255, 100, 0);    // Orange
            colors[1] = Color(255, 105, 180);  // Pink
            colors[2] = Color(148, 0, 211);    // Purple
            colors[3] = Color(0, 0, 255);      // Blue
            colors[4] = Color(0, 255, 255);    // Cyan
            break;
            
        case ColorPath::ForestMist:
            colors[0] = Color(34, 139, 34);    // Forest green
            colors[1] = Color(0, 128, 128);    // Teal
            colors[2] = Color(70, 130, 180);   // Steel blue
            colors[3] = Color(138, 43, 226);   // Blue violet
            colors[4] = Color(50, 205, 50);    // Lime green
            break;
            
        case ColorPath::FireAndIce:
            colors[0] = Color(255, 0, 0);      // Red
            colors[1] = Color(255, 140, 0);    // Dark orange
            colors[2] = Color(255, 255, 255);  // White
            colors[3] = Color(0, 255, 255);    // Cyan
            colors[4] = Color(0, 0, 255);      // Blue
            break;
            
        case ColorPath::NeonNights:
            colors[0] = Color(255, 20, 147);   // Deep pink
            colors[1] = Color(148, 0, 211);    // Dark violet
            colors[2] = Color(0, 0, 255);      // Blue
            colors[3] = Color(0, 255, 255);    // Cyan
            colors[4] = Color(0, 255, 127);    // Spring green
            break;
            
        case ColorPath::GoldenHour:
            colors[0] = Color(255, 215, 0);    // Gold
            colors[1] = Color(255, 140, 0);    // Dark orange
            colors[2] = Color(255, 182, 193);  // Light pink
            colors[3] = Color(255, 0, 255);    // Magenta
            colors[4] = Color(238, 130, 238);  // Violet
            break;
            
        case ColorPath::DeepSpace:
            colors[0] = Color(0, 0, 139);      // Dark blue
            colors[1] = Color(75, 0, 130);     // Indigo
            colors[2] = Color(25, 25, 112);    // Midnight blue
            colors[3] = Color(65, 105, 225);   // Royal blue
            colors[4] = Color(0, 191, 255);    // Deep sky blue
            break;
            
        case ColorPath::TropicalDream:
            colors[0] = Color(0, 255, 255);    // Cyan
            colors[1] = Color(64, 224, 208);   // Turquoise
            colors[2] = Color(0, 255, 127);    // Spring green
            colors[3] = Color(255, 255, 0);    // Yellow
            colors[4] = Color(255, 165, 0);    // Orange
            break;
            
        case ColorPath::AuroraBorealis:
            colors[0] = Color(0, 255, 127);    // Spring green
            colors[1] = Color(0, 255, 255);    // Cyan
            colors[2] = Color(0, 191, 255);    // Deep sky blue
            colors[3] = Color(138, 43, 226);   // Blue violet
            colors[4] = Color(255, 105, 180);  // Hot pink
            break;
            
        case ColorPath::VolcanicGlow:
            colors[0] = Color(255, 0, 0);      // Red
            colors[1] = Color(255, 69, 0);     // Red-orange
            colors[2] = Color(255, 215, 0);    // Gold
            colors[3] = Color(255, 255, 224);  // Light yellow
            colors[4] = Color(255, 99, 71);    // Tomato
            break;
            
        case ColorPath::MidnightBloom:
            colors[0] = Color(128, 0, 128);    // Purple
            colors[1] = Color(0, 0, 205);      // Medium blue
            colors[2] = Color(0, 206, 209);    // Dark turquoise
            colors[3] = Color(255, 20, 147);   // Deep pink
            colors[4] = Color(186, 85, 211);   // Medium orchid
            break;
            
        default:
            // Fallback to simple rainbow
            colors[0] = Color::Red();
            colors[1] = Color::Yellow();
            colors[2] = Color::Green();
            colors[3] = Color::Cyan();
            colors[4] = Color::Blue();
            break;
    }
    
    // Map position to color segments
    // We have 5 colors, creating 5 segments (last connects back to first)
    float scaledPos = position * 5.0f;
    int segment = static_cast<int>(scaledPos) % 5;
    float segmentPos = std::fmod(scaledPos, 1.0f);
    
    int nextSegment = (segment + 1) % 5;
    
    return Color::lerp(colors[segment], colors[nextSegment], segmentPos);
}

void LEDController::selectRandomPath() {
    std::uniform_int_distribution<int> dist(0, static_cast<int>(ColorPath::COUNT) - 1);
    ColorPath newPath = static_cast<ColorPath>(dist(rng));
    
    // Avoid selecting the same path twice in a row
    while (newPath == currentPath && static_cast<int>(ColorPath::COUNT) > 1) {
        newPath = static_cast<ColorPath>(dist(rng));
    }
    
    currentPath = newPath;
    
    const char* pathName = "Unknown";
    switch (currentPath) {
        case ColorPath::SunsetToOcean: pathName = "Sunset to Ocean"; break;
        case ColorPath::ForestMist: pathName = "Forest Mist"; break;
        case ColorPath::FireAndIce: pathName = "Fire and Ice"; break;
        case ColorPath::NeonNights: pathName = "Neon Nights"; break;
        case ColorPath::GoldenHour: pathName = "Golden Hour"; break;
        case ColorPath::DeepSpace: pathName = "Deep Space"; break;
        case ColorPath::TropicalDream: pathName = "Tropical Dream"; break;
        case ColorPath::AuroraBorealis: pathName = "Aurora Borealis"; break;
        case ColorPath::VolcanicGlow: pathName = "Volcanic Glow"; break;
        case ColorPath::MidnightBloom: pathName = "Midnight Bloom"; break;
        default: break;
    }
    
    std::cout << "LED: Color path changed to '" << pathName << "'" << std::endl;
}

} // namespace DubSiren
