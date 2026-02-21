#include "Hardware/GPIOController.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>

#ifdef HAVE_GPIOD
#include <gpiod.h>
#endif

#ifdef HAVE_PIGPIO
#include <pigpio.h>
#endif

namespace DubSiren {

// ============================================================================
// DEBUG: Set to 1 to enable verbose input logging for testing
// ============================================================================
#define DEBUG_INPUTS 1

// ============================================================================
// Platform-specific GPIO helpers
// ============================================================================

namespace {

bool gpioInitialized = false;

#ifdef HAVE_GPIOD
struct gpiod_chip* gpioChip = nullptr;
struct gpiod_line_request* lineRequest = nullptr;

// All GPIO pins we need to monitor
const unsigned int ALL_PINS[] = {
    2, 3, 4, 5, 9, 10, 13, 14, 15, 17, 20, 22, 23, 24, 26, 27
};
const size_t NUM_PINS = sizeof(ALL_PINS) / sizeof(ALL_PINS[0]);

// Lookup table: maps GPIO number -> index in ALL_PINS (-1 if not used)
// Max GPIO on Pi is 27, so array of 28 elements
int gpioToLineIndex[28] = {-1};

bool initPlatformGPIO() {
    if (gpioInitialized) return true;
    
    // Initialize GPIO-to-line-index lookup table
    for (int i = 0; i < 28; ++i) gpioToLineIndex[i] = -1;
    for (size_t i = 0; i < NUM_PINS; ++i) {
        gpioToLineIndex[ALL_PINS[i]] = static_cast<int>(i);
    }
    
    gpioChip = gpiod_chip_open("/dev/gpiochip0");
    if (!gpioChip) {
        std::cerr << "Failed to open GPIO chip" << std::endl;
        return false;
    }
    
    // Configure all pins at once for efficiency
    struct gpiod_line_settings* settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);
    
    struct gpiod_line_config* config = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(config, ALL_PINS, NUM_PINS, settings);
    
    struct gpiod_request_config* reqConfig = gpiod_request_config_new();
    gpiod_request_config_set_consumer(reqConfig, "dubsiren");
    
    lineRequest = gpiod_chip_request_lines(gpioChip, reqConfig, config);
    
    gpiod_request_config_free(reqConfig);
    gpiod_line_config_free(config);
    gpiod_line_settings_free(settings);
    
    if (!lineRequest) {
        std::cerr << "Failed to request GPIO lines" << std::endl;
        gpiod_chip_close(gpioChip);
        gpioChip = nullptr;
        return false;
    }
    
    gpioInitialized = true;
    std::cout << "libgpiod initialized successfully (" << NUM_PINS << " pins)" << std::endl;
    return true;
}

void cleanupPlatformGPIO() {
    if (lineRequest) {
        gpiod_line_request_release(lineRequest);
        lineRequest = nullptr;
    }
    if (gpioChip) {
        gpiod_chip_close(gpioChip);
        gpioChip = nullptr;
    }
    gpioInitialized = false;
}

int readPin(int pin) {
    if (!lineRequest) return 1;
    if (pin < 0 || pin > 27) return 1;

    // Verify GPIO is in our requested set
    int lineIndex = gpioToLineIndex[pin];
    if (lineIndex < 0) return 1;  // GPIO not in our requested set

    // Read using GPIO pin number (libgpiod uses chip offsets, not array indices)
    int value = gpiod_line_request_get_value(lineRequest, static_cast<unsigned int>(pin));

    // With pull-up bias: ACTIVE = HIGH (not pressed), INACTIVE = LOW (pressed/grounded)
    // Our button logic expects: 0 = pressed, 1 = not pressed
    return value == GPIOD_LINE_VALUE_ACTIVE ? 1 : 0;
}

#elif defined(HAVE_PIGPIO)

bool initPlatformGPIO() {
    if (!gpioInitialized) {
        if (gpioInitialise() < 0) {
            std::cerr << "Failed to initialize pigpio" << std::endl;
            return false;
        }
        gpioInitialized = true;
    }
    return true;
}

void cleanupPlatformGPIO() {
    if (gpioInitialized) {
        gpioTerminate();
        gpioInitialized = false;
    }
}

int readPin(int pin) {
    return gpioRead(pin);
}

void setupInputPin(int pin) {
    gpioSetMode(pin, PI_INPUT);
    gpioSetPullUpDown(pin, PI_PUD_UP);
}

#else

bool initPlatformGPIO() {
    std::cout << "GPIO not available - running in simulation mode" << std::endl;
    return false;
}

void cleanupPlatformGPIO() {
}

int readPin(int pin) {
    (void)pin;
    return 1;  // Simulated: pulled up (not pressed)
}

void setupInputPin(int pin) {
    (void)pin;
}

#endif

} // anonymous namespace

// ============================================================================
// RotaryEncoder Implementation
// ============================================================================

RotaryEncoder::RotaryEncoder(int clkPin, int dtPin, Callback callback)
    : clkPin(clkPin)
    , dtPin(dtPin)
    , callback(std::move(callback))
    , position(0)
    , running(false)
    , lastClk(1)
    , lastDt(1)
{
}

RotaryEncoder::~RotaryEncoder() {
    stop();
}

void RotaryEncoder::start() {
    if (running.load()) return;
    
#ifdef HAVE_PIGPIO
    setupInputPin(clkPin);
    setupInputPin(dtPin);
    lastClk = readPin(clkPin);
    lastDt = readPin(dtPin);
#endif
    
    running.store(true);
    pollThread = std::thread(&RotaryEncoder::pollLoop, this);
}

void RotaryEncoder::stop() {
    running.store(false);
    if (pollThread.joinable()) {
        pollThread.join();
    }
}

void RotaryEncoder::pollLoop() {
    while (running.load()) {
        update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void RotaryEncoder::update() {
    int clkState = readPin(clkPin);
    int dtState = readPin(dtPin);
    
#if DEBUG_INPUTS
    // Log only when state changes to avoid spam
    if (clkState != lastClk || dtState != lastDt) {
        std::cout << "[ENC " << clkPin << "/" << dtPin << "] CLK=" << clkState << " DT=" << dtState << std::endl;
    }
#endif
    
    if (clkState != lastClk) {
        int direction;
        if (dtState != clkState) {
            position.fetch_add(1);
            direction = 1;
        } else {
            position.fetch_sub(1);
            direction = -1;
        }
        
        if (callback) {
            callback(direction);
        }
    }
    
    lastClk = clkState;
    lastDt = dtState;
}

// ============================================================================
// MomentarySwitch Implementation
// ============================================================================

MomentarySwitch::MomentarySwitch(int pin, PressCallback onPress, ReleaseCallback onRelease)
    : pin(pin)
    , pressCallback(std::move(onPress))
    , releaseCallback(std::move(onRelease))
    , pressed(false)
    , running(false)
    , lastState(1)
{
    lastChange = std::chrono::steady_clock::now();
    lastPressTime = lastChange;
}

MomentarySwitch::~MomentarySwitch() {
    stop();
}

void MomentarySwitch::start() {
    if (running.load()) return;
    
#ifdef HAVE_PIGPIO
    setupInputPin(pin);
    lastState = readPin(pin);
#endif
    
    running.store(true);
    pollThread = std::thread(&MomentarySwitch::pollLoop, this);
}

void MomentarySwitch::stop() {
    running.store(false);
    if (pollThread.joinable()) {
        pollThread.join();
    }
}

void MomentarySwitch::pollLoop() {
    int lastLoggedState = -1;  // For debug logging
    
    while (running.load()) {
        int state = readPin(pin);
        auto now = std::chrono::steady_clock::now();
        
#if DEBUG_INPUTS
        if (state != lastLoggedState) {
            std::cout << "[BTN " << pin << "] state=" << state 
                      << (state == 0 ? " (PRESSED)" : " (released)") << std::endl;
            lastLoggedState = state;
        }
#endif
        
        // Debounce
        if (state != lastState) {
            lastState = state;
            lastChange = now;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastChange).count();
        if (elapsed < DEBOUNCE_MS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        
        // Button is active low (pressed when pin reads 0)
        if (state == 0 && !pressed.load()) {
            pressed.store(true);
            lastPressTime = now;
            if (pressCallback) {
                pressCallback();
            }
        } else if (state == 1 && pressed.load()) {
            // Enforce minimum press duration
            auto pressDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPressTime).count();
            if (pressDuration >= MIN_PRESS_MS) {
                pressed.store(false);
                if (releaseCallback) {
                    releaseCallback();
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

// ============================================================================
// ThreePositionSwitch Implementation
// ============================================================================

ThreePositionSwitch::ThreePositionSwitch(int upPin, int downPin, PositionCallback onChange)
    : upPin(upPin)
    , downPin(downPin)
    , callback(std::move(onChange))
    , position(SwitchPosition::Off)
    , running(false)
    , lastPosition(SwitchPosition::Off)
{
    lastChange = std::chrono::steady_clock::now();
}

ThreePositionSwitch::~ThreePositionSwitch() {
    stop();
}

void ThreePositionSwitch::start() {
    if (running.load()) return;
    
#ifdef HAVE_PIGPIO
    setupInputPin(upPin);
    setupInputPin(downPin);
#endif
    
    // Read initial position
    lastPosition = readPosition();
    position.store(lastPosition);
    
    running.store(true);
    pollThread = std::thread(&ThreePositionSwitch::pollLoop, this);
}

void ThreePositionSwitch::stop() {
    running.store(false);
    if (pollThread.joinable()) {
        pollThread.join();
    }
}

SwitchPosition ThreePositionSwitch::readPosition() {
    // With pull-ups enabled:
    // - Pin reads LOW (0) when connected to GND (switch in that position)
    // - Pin reads HIGH (1) when not connected (switch in other position)
    int upState = readPin(upPin);
    int downState = readPin(downPin);
    
#if DEBUG_INPUTS
    static int lastLoggedUp = -1, lastLoggedDown = -1;
    if (upState != lastLoggedUp || downState != lastLoggedDown) {
        const char* pos = (upState == 0) ? "UP" : (downState == 0) ? "DOWN" : "OFF";
        std::cout << "[PITCH SW] UP_pin(" << upPin << ")=" << upState 
                  << " DOWN_pin(" << downPin << ")=" << downState 
                  << " -> " << pos << std::endl;
        lastLoggedUp = upState;
        lastLoggedDown = downState;
    }
#endif
    
    if (upState == 0) {
        return SwitchPosition::Up;
    } else if (downState == 0) {
        return SwitchPosition::Down;
    }
    return SwitchPosition::Off;
}

void ThreePositionSwitch::pollLoop() {
    while (running.load()) {
        SwitchPosition currentPos = readPosition();
        auto now = std::chrono::steady_clock::now();
        
        // Debounce
        if (currentPos != lastPosition) {
            lastPosition = currentPos;
            lastChange = now;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastChange).count();
        if (elapsed >= DEBOUNCE_MS) {
            SwitchPosition storedPos = position.load();
            if (currentPos != storedPos) {
                position.store(currentPos);
                if (callback) {
                    callback(currentPos);
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// ============================================================================
// GPIOController Implementation
// ============================================================================

GPIOController::GPIOController(AudioEngine& engine, ShutdownCallback shutdownCb)
    : engine(engine)
    , shutdownCallback(std::move(shutdownCb))
    , running(false)
    , currentBank(Bank::A)
    , shiftPressed(false)
    , secretMode(SecretMode::None)
    // secretModePreset and lastPitchEnvPosition are initialized via brace-init in header
{
}

GPIOController::~GPIOController() {
    stop();
}

bool GPIOController::initGPIO() {
    return initPlatformGPIO();
}

void GPIOController::cleanupGPIO() {
    cleanupPlatformGPIO();
}

void GPIOController::start() {
    if (running.load()) return;
    
    std::cout << "Initializing control surface..." << std::endl;
    
    bool hasGPIO = initGPIO();
    
    if (hasGPIO) {
        // Create encoders
        const int encoderPins[5][2] = {
            {GPIO::ENCODER_1_CLK, GPIO::ENCODER_1_DT},
            {GPIO::ENCODER_2_CLK, GPIO::ENCODER_2_DT},
            {GPIO::ENCODER_3_CLK, GPIO::ENCODER_3_DT},
            {GPIO::ENCODER_4_CLK, GPIO::ENCODER_4_DT},
            {GPIO::ENCODER_5_CLK, GPIO::ENCODER_5_DT}
        };
        
        for (int i = 0; i < 5; ++i) {
            encoders[i] = std::make_unique<RotaryEncoder>(
                encoderPins[i][0], encoderPins[i][1],
                [this, i](int dir) { handleEncoder(i, dir); }
            );
            encoders[i]->start();
            std::cout << "  ✓ encoder_" << (i+1) << " initialized (GPIO " 
                      << encoderPins[i][0] << ", " << encoderPins[i][1] << ")" << std::endl;
        }
        
        // Create buttons
        buttons[0] = std::make_unique<MomentarySwitch>(
            GPIO::TRIGGER_BTN,
            [this]() { onTriggerPress(); },
            [this]() { onTriggerRelease(); }
        );
        buttons[0]->start();
        std::cout << "  ✓ trigger button initialized (GPIO " << GPIO::TRIGGER_BTN << ")" << std::endl;
        
        buttons[1] = std::make_unique<MomentarySwitch>(
            GPIO::SHIFT_BTN,
            [this]() { onShiftPress(); },
            [this]() { onShiftRelease(); }
        );
        buttons[1]->start();
        std::cout << "  ✓ shift button initialized (GPIO " << GPIO::SHIFT_BTN << ")" << std::endl;
        
        buttons[2] = std::make_unique<MomentarySwitch>(
            GPIO::SHUTDOWN_BTN,
            [this]() { onShutdownPress(); },
            nullptr
        );
        buttons[2]->start();
        std::cout << "  ✓ shutdown button initialized (GPIO " << GPIO::SHUTDOWN_BTN << ")" << std::endl;

        buttons[3] = std::make_unique<MomentarySwitch>(
            GPIO::WAVEFORM_BTN,
            [this]() { onWaveformPress(); },
            nullptr
        );
        buttons[3]->start();
        std::cout << "  ✓ waveform button initialized (GPIO " << GPIO::WAVEFORM_BTN << ")" << std::endl;

        // Create 3-position pitch envelope switch
        pitchEnvSwitch = std::make_unique<ThreePositionSwitch>(
            GPIO::PITCH_ENV_UP, GPIO::PITCH_ENV_DOWN,
            [this](SwitchPosition pos) { onPitchEnvChange(pos); }
        );
        pitchEnvSwitch->start();
        std::cout << "  ✓ pitch_env switch initialized (GPIO " << GPIO::PITCH_ENV_UP 
                  << ", " << GPIO::PITCH_ENV_DOWN << ")" << std::endl;
        
        // Apply initial pitch envelope from switch position
        onPitchEnvChange(pitchEnvSwitch->getPosition());
        
        // Initialize optional WS2812 LED controller
        ledController = std::make_unique<LEDController>(GPIO::LED_DATA);
        if (ledController->init()) {
            ledController->showStartupColor();  // Show amber during init
            std::cout << "  ✓ LED controller initialized (GPIO " << GPIO::LED_DATA << ")" << std::endl;
        } else {
            std::cout << "  ⚠ LED controller not available (optional)" << std::endl;
            ledController.reset();
        }
    }
    
    // Apply initial parameters (Auto Wail preset)
    engine.setVolume(params.volume);
    engine.setLfoDepth(params.lfoDepth);        // Filter modulation depth
    engine.setLfoPitchDepth(0.5f);              // Auto Wail pitch modulation (wee-woo)
    engine.setLfoRate(params.lfoRate);
    engine.setLfoWaveform(Waveform::Triangle);  // Smooth pitch transitions
    engine.setFilterCutoff(params.filterFreq);
    engine.setFrequency(params.baseFreq);
    engine.setFilterResonance(params.filterRes);
    engine.setDelayFeedback(params.delayFeedback);
    engine.setReverbMix(params.reverbMix);
    engine.setReleaseTime(params.release);
    engine.setDelayTime(params.delayTime);
    engine.setReverbSize(params.reverbSize);
    engine.setWaveform(params.oscWaveform);

    std::cout << "  Initial LFO: depth=" << params.lfoDepth << ", rate=" << params.lfoRate << "Hz" << std::endl;
    
    running.store(true);
    
    // Start LED controller and show ready color
    if (ledController) {
        ledController->start();
        ledController->showReadyColor();  // Show lime green when ready
    }
    
    std::cout << "\n";
    std::cout << "============================================================" << std::endl;
    std::cout << "  Control Surface Ready" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "\nBank A: LFO Depth, Base Freq, Filter Freq, Delay FB, Reverb Mix" << std::endl;
    std::cout << "Bank B: LFO Rate, Delay Time, Filter Res, Osc Wave, Reverb Size" << std::endl;
    std::cout << "\nMaster Volume: " << params.volume << " (fixed)" << std::endl;
    std::cout << "\nButtons: Trigger, Shift (Bank A/B), Shutdown, Waveform Cycle" << std::endl;
    std::cout << "Pitch Env Switch: UP=rise | OFF=none | DOWN=fall" << std::endl;
    if (ledController && ledController->isAvailable()) {
        std::cout << "Status LED: Active (GPIO " << GPIO::LED_DATA << ")" << std::endl;
    }
    std::cout << "============================================================" << std::endl;
}

void GPIOController::stop() {
    if (!running.load()) return;
    
    running.store(false);
    
    for (auto& encoder : encoders) {
        if (encoder) encoder->stop();
    }
    
    for (auto& button : buttons) {
        if (button) button->stop();
    }
    
    if (pitchEnvSwitch) pitchEnvSwitch->stop();
    
    if (ledController) ledController->stop();
    
    cleanupGPIO();
    
    std::cout << "Control surface stopped" << std::endl;
}

void GPIOController::handleEncoder(int encoderIndex, int direction) {
    Bank bank = currentBank.load();

    // Bank A parameters
    const char* bankAParams[] = {"lfo_depth", "base_freq", "filter_freq", "delay_feedback", "reverb_mix"};
    // Bank B parameters
    const char* bankBParams[] = {"lfo_rate", "delay_time", "filter_res", "osc_waveform", "reverb_size"};
    
    const char* paramName = (bank == Bank::A) ? bankAParams[encoderIndex] : bankBParams[encoderIndex];
    
    // Update parameter based on type
    float step;
    float newValue;

    if (strcmp(paramName, "lfo_depth") == 0) {
        step = 0.042f * direction;
        params.lfoDepth = clamp(params.lfoDepth + step, 0.0f, 1.0f);
        engine.setLfoDepth(params.lfoDepth);  // Filter modulation depth
        newValue = params.lfoDepth;
    }
    else if (strcmp(paramName, "filter_freq") == 0) {
        // Logarithmic control for full range in ~1 rotation (24 steps)
        float multiplier = (direction > 0) ? 1.32f : (1.0f / 1.32f);
        params.filterFreq = clamp(params.filterFreq * multiplier, 20.0f, 20000.0f);
        engine.setFilterCutoff(params.filterFreq);
        newValue = params.filterFreq;
    }
    else if (strcmp(paramName, "base_freq") == 0) {
        // Logarithmic frequency control for full range in ~1 rotation (24 steps)
        float multiplier = (direction > 0) ? 1.165f : (1.0f / 1.165f);
        params.baseFreq = clamp(params.baseFreq * multiplier, 50.0f, 2000.0f);
        engine.setFrequency(params.baseFreq);

        // Only modulate delay time inversely with pitch in PitchDelay secret mode
        // (higher pitch = shorter delay - creates harmonic echo patterns common in dub sirens)
        if (secretMode.load() == SecretMode::PitchDelay) {
            float refFreq = 440.0f;
            float scaledDelayTime = params.delayTime * (refFreq / params.baseFreq);
            scaledDelayTime = clamp(scaledDelayTime, 0.01f, 2.0f);
            engine.setDelayTime(scaledDelayTime);
        }

        newValue = params.baseFreq;
    }
    else if (strcmp(paramName, "filter_res") == 0) {
        step = 0.04f * direction;
        params.filterRes = clamp(params.filterRes + step, 0.0f, 0.95f);
        engine.setFilterResonance(params.filterRes);
        newValue = params.filterRes;
    }
    else if (strcmp(paramName, "delay_feedback") == 0) {
        step = 0.04f * direction;
        params.delayFeedback = clamp(params.delayFeedback + step, 0.0f, 0.95f);
        engine.setDelayFeedback(params.delayFeedback);
        newValue = params.delayFeedback;
    }
    else if (strcmp(paramName, "reverb_mix") == 0) {
        step = 0.042f * direction;
        params.reverbMix = clamp(params.reverbMix + step, 0.0f, 1.0f);
        engine.setReverbMix(params.reverbMix);
        newValue = params.reverbMix;
    }
    else if (strcmp(paramName, "lfo_rate") == 0) {
        // Logarithmic control for LFO rate (0.1 Hz to 20 Hz)
        float multiplier = (direction > 0) ? 1.15f : (1.0f / 1.15f);
        params.lfoRate = clamp(params.lfoRate * multiplier, 0.1f, 20.0f);
        engine.setLfoRate(params.lfoRate);
        newValue = params.lfoRate;
    }
    else if (strcmp(paramName, "delay_time") == 0) {
        step = 0.083f * direction;
        params.delayTime = clamp(params.delayTime + step, 0.001f, 2.0f);
        engine.setDelayTime(params.delayTime);
        newValue = params.delayTime;
    }
    else if (strcmp(paramName, "reverb_size") == 0) {
        step = 0.042f * direction;
        params.reverbSize = clamp(params.reverbSize + step, 0.0f, 1.0f);
        engine.setReverbSize(params.reverbSize);
        newValue = params.reverbSize;
    }
    else if (strcmp(paramName, "osc_waveform") == 0) {
        params.oscWaveform = (params.oscWaveform + direction + 4) % 4;
        engine.setWaveform(params.oscWaveform);
        newValue = static_cast<float>(params.oscWaveform);
    }
    else {
        return;
    }
    
    const char* bankName = (bank == Bank::A) ? "A" : "B";
    std::cout << "[Bank " << bankName << "] " << paramName << ": " << newValue << std::endl;
}

void GPIOController::onTriggerPress() {
    SecretMode currentMode = secretMode.load();

    if (currentMode == SecretMode::MP3) {
        // In MP3 mode, trigger starts playback
        std::cout << "Trigger: STARTING MP3 PLAYBACK" << std::endl;
        engine.startMP3Playback();
    } else {
        std::cout << "Trigger: PRESSED" << std::endl;
        engine.trigger();
    }
}

void GPIOController::onTriggerRelease() {
    SecretMode currentMode = secretMode.load();

    if (currentMode == SecretMode::MP3) {
        // In MP3 mode, release doesn't do anything (one-shot playback)
        // MP3 will auto-exit when finished
    } else {
        std::cout << "Trigger: RELEASED" << std::endl;
        engine.release();
    }
}

void GPIOController::onPitchEnvChange(SwitchPosition position) {
    // Track toggles for MP3 mode activation (off->on transitions count as toggles)
    {
        std::lock_guard<std::mutex> lock(pitchEnvMutex);

        // Check if this is a transition from Off to On (either Up or Down)
        if (lastPitchEnvPosition == SwitchPosition::Off && position != SwitchPosition::Off) {
            // Record this toggle
            auto now = std::chrono::steady_clock::now();
            recentPitchEnvToggles.push_back(now);

            // Check for MP3 mode activation
            checkPitchEnvMP3Activation();
        }

        lastPitchEnvPosition = position;
    }

    // Apply pitch envelope (works in normal and secret modes)
    PitchEnvelopeMode mode;
    const char* modeName;

    switch (position) {
        case SwitchPosition::Up:
            mode = PitchEnvelopeMode::Up;
            modeName = "up (rise)";
            break;
        case SwitchPosition::Down:
            mode = PitchEnvelopeMode::Down;
            modeName = "down (fall)";
            break;
        case SwitchPosition::Off:
        default:
            mode = PitchEnvelopeMode::None;
            modeName = "none";
            break;
    }

    engine.setPitchEnvelopeMode(mode);

    SecretMode currentMode = secretMode.load();
    if (currentMode != SecretMode::None) {
        const char* modeStr;
        if (currentMode == SecretMode::NJD) {
            modeStr = "NJD";
        } else if (currentMode == SecretMode::UFO) {
            modeStr = "UFO";
        } else if (currentMode == SecretMode::MP3) {
            modeStr = "MP3";
        } else {
            modeStr = "PITCH-DELAY";
        }
        std::cout << "[" << modeStr << " MODE] Pitch envelope: " << modeName << std::endl;
    } else {
        std::cout << "Pitch envelope: " << modeName << std::endl;
    }
}

void GPIOController::onShiftPress() {
    shiftPressed.store(true);

    // Track shift button presses for secret mode activation
    {
        std::lock_guard<std::mutex> lock(pressesMutex);
        recentShiftPresses.push_back(std::chrono::steady_clock::now());
    }
    checkSecretModeActivation();

    SecretMode currentMode = secretMode.load();
    if (currentMode != SecretMode::None) {
        // In NJD or UFO secret mode, shift cycles through presets
        // In PitchDelay mode, shift just switches banks (no presets to cycle)
        // In MP3 mode, shift cycles through MP3 files
        if (currentMode == SecretMode::NJD || currentMode == SecretMode::UFO) {
            cycleSecretModePreset();
        } else if (currentMode == SecretMode::MP3) {
            // Cycle to next MP3 file
            int currentIndex = engine.getCurrentMP3Index();
            int fileCount = engine.getMP3FileCount();
            int nextIndex = (currentIndex + 1) % fileCount;
            engine.selectMP3File(nextIndex);

            // Update LED color for new file
            if (ledController) {
                auto color = engine.getMP3Color();
                ledController->setColor(color.r, color.g, color.b);
            }

            std::cout << "[MP3] Selected: " << engine.getCurrentMP3FileName() << std::endl;
        } else {
            // PitchDelay mode - switch to Bank B
            currentBank.store(Bank::B);
            std::cout << "Bank B active" << std::endl;
        }
    } else {
        // Normal operation - switch to Bank B
        currentBank.store(Bank::B);
        std::cout << "Bank B active" << std::endl;
    }
}

void GPIOController::onShiftRelease() {
    shiftPressed.store(false);

    SecretMode currentMode = secretMode.load();
    if (currentMode == SecretMode::None || currentMode == SecretMode::PitchDelay) {
        // Normal operation or PitchDelay mode - switch back to Bank A
        currentBank.store(Bank::A);
        std::cout << "Bank A active" << std::endl;
    }
    // In NJD/UFO secret mode, don't change bank on release
}

void GPIOController::onShutdownPress() {
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  SHUTDOWN BUTTON PRESSED" << std::endl;
    std::cout << "  Safely shutting down the system..." << std::endl;
    std::cout << "============================================================" << std::endl;

    if (shutdownCallback) {
        shutdownCallback();
    }

    // Issue system shutdown command
    std::system("sudo shutdown -h now");
}

void GPIOController::onWaveformPress() {
    const char* waveformNames[] = {"Sine", "Square", "Saw", "Triangle"};

    // Cycle: 0 -> 1 -> 2 -> 3 -> 0 ...
    params.oscWaveform = (params.oscWaveform + 1) % 4;
    engine.setWaveform(params.oscWaveform);

    std::cout << "Waveform: " << waveformNames[params.oscWaveform] << std::endl;
}

// ============================================================================
// Secret Mode Implementation
// ============================================================================

void GPIOController::checkSecretModeActivation() {
    auto now = std::chrono::steady_clock::now();

    int pressCount = 0;
    bool activatePitchDelay = false;
    bool activateNJD = false;
    bool activateUFO = false;

    {
        std::lock_guard<std::mutex> lock(pressesMutex);

        // Remove old presses (older than 2 seconds)
        recentShiftPresses.erase(
            std::remove_if(recentShiftPresses.begin(), recentShiftPresses.end(),
                [&now](const auto& t) {
                    return std::chrono::duration_cast<std::chrono::milliseconds>(now - t).count() > 2000;
                }),
            recentShiftPresses.end()
        );

        // Count recent presses within 2 second window
        pressCount = static_cast<int>(recentShiftPresses.size());

        // Debug output for press counting
        if (pressCount >= 2) {
            std::cout << "[DEBUG] Shift presses in window: " << pressCount << " (need 3/5/10)" << std::endl;
        }

        // Check for UFO mode first (10 presses) - takes priority
        if (pressCount >= 10) {
            activateUFO = true;
        }
        // Check for NJD mode (5 presses)
        else if (pressCount >= 5) {
            activateNJD = true;
        }
        // Check for Pitch-Delay mode (3 presses)
        else if (pressCount >= 3) {
            activatePitchDelay = true;
        }
    }

    // Activate outside the lock to avoid potential deadlock with other callbacks
    if (activateUFO) {
        activateSecretMode(SecretMode::UFO);
    } else if (activateNJD) {
        activateSecretMode(SecretMode::NJD);
    } else if (activatePitchDelay) {
        activateSecretMode(SecretMode::PitchDelay);
    }
}

void GPIOController::checkPitchEnvMP3Activation() {
    auto now = std::chrono::steady_clock::now();

    // Remove old toggles (older than 2 seconds)
    recentPitchEnvToggles.erase(
        std::remove_if(recentPitchEnvToggles.begin(), recentPitchEnvToggles.end(),
            [&now](const auto& t) {
                return std::chrono::duration_cast<std::chrono::milliseconds>(now - t).count() > 2000;
            }),
        recentPitchEnvToggles.end()
    );

    int toggleCount = static_cast<int>(recentPitchEnvToggles.size());

    // Debug output for toggle counting
    if (toggleCount >= 2) {
        std::cout << "[DEBUG] Pitch envelope toggles in window: " << toggleCount << " (need 5+)" << std::endl;
    }

    // Check if we have 5 or more toggles in 2 seconds
    // Only activate if not already in MP3 mode (prevents re-triggering/exiting on extra toggles)
    if (toggleCount >= 5 && secretMode.load() != SecretMode::MP3) {
        // Activate MP3 mode (clearing happens in activateSecretMode)
        activateSecretMode(SecretMode::MP3);
    }
}

void GPIOController::activateSecretMode(SecretMode mode) {
    SecretMode currentMode = secretMode.load();

    // If activating same mode we're already in, toggle it off
    if (currentMode == mode) {
        exitSecretMode();
        return;
    }

    // If in a different secret mode, exit it first
    if (currentMode != SecretMode::None) {
        exitSecretMode();
    }

    // Enter the new mode
    secretMode.store(mode);
    secretModePreset.store(0);

    // Clear activation triggers to prevent re-triggering
    if (mode == SecretMode::MP3) {
        // MP3 mode is activated by pitch envelope toggles
        std::lock_guard<std::mutex> lock(pitchEnvMutex);
        recentPitchEnvToggles.clear();
    } else {
        // Other modes are activated by shift button presses
        std::lock_guard<std::mutex> lock(pressesMutex);
        recentShiftPresses.clear();
    }

    // Update LED mode for secret modes
    if (ledController) {
        if (mode == SecretMode::NJD) {
            ledController->setMode(LEDMode::NJD);
        } else if (mode == SecretMode::UFO) {
            ledController->setMode(LEDMode::UFO);
        } else if (mode == SecretMode::PitchDelay) {
            // Use a different LED color for PitchDelay mode (could use NJD or UFO, or Normal)
            ledController->setMode(LEDMode::Normal);
        } else if (mode == SecretMode::MP3) {
            ledController->setMode(LEDMode::MP3);
        }
    }

    const char* modeName;
    if (mode == SecretMode::NJD) {
        modeName = "NJD SIREN";
    } else if (mode == SecretMode::UFO) {
        modeName = "UFO";
    } else if (mode == SecretMode::MP3) {
        modeName = "MP3 PLAYER";
    } else {
        modeName = "PITCH-DELAY LINK";
    }

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║              🎵 SECRET MODE ACTIVATED! 🎵                ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║  Mode: " << modeName << std::string(49 - strlen(modeName), ' ') << "║" << std::endl;
    if (mode == SecretMode::PitchDelay) {
        std::cout << "║  Pitch and delay are now inversely linked                ║" << std::endl;
        std::cout << "║  (higher pitch = shorter delay)                          ║" << std::endl;
    } else if (mode == SecretMode::MP3) {
        // Load MP3 files from directory (try multiple locations)
        bool loaded = false;
        std::string mp3Dir;

        // Try Pi location first
        if (engine.enableMP3Mode("/home/pi/dubsiren/mp3s")) {
            loaded = true;
            mp3Dir = "/home/pi/dubsiren/mp3s";
        }
        // Try current user's home
        else if (engine.enableMP3Mode(std::string(getenv("HOME") ? getenv("HOME") : ".") + "/dubsiren/mp3s")) {
            loaded = true;
            mp3Dir = std::string(getenv("HOME") ? getenv("HOME") : ".") + "/dubsiren/mp3s";
        }
        // Try repo location (for development)
        else if (engine.enableMP3Mode("../mp3s")) {
            loaded = true;
            mp3Dir = "../mp3s";
        }

        if (loaded) {
            int fileCount = engine.getMP3FileCount();
            std::string fileName = engine.getCurrentMP3FileName();
            std::cout << "║  Loaded " << fileCount << " MP3 file(s)" << std::string(34 - std::to_string(fileCount).length(), ' ') << "║" << std::endl;
            std::cout << "║  Current: " << fileName << std::string(45 - fileName.length(), ' ') << "║" << std::endl;
            std::cout << "║  From: " << mp3Dir << std::string(51 - mp3Dir.length(), ' ') << "║" << std::endl;
            std::cout << "║  Press TRIGGER to play                                   ║" << std::endl;
            if (fileCount > 1) {
                std::cout << "║  Press SHIFT to cycle files                              ║" << std::endl;
            }
        } else {
            std::cout << "║  ERROR: Failed to load MP3 files                         ║" << std::endl;
            std::cout << "║  Tried: /home/pi/dubsiren/mp3s, ~/dubsiren/mp3s, ../mp3s║" << std::endl;
            std::cout << "║  Place MP3 files in one of these directories             ║" << std::endl;
        }
    } else {
        std::cout << "║  Press SHIFT to cycle presets                            ║" << std::endl;
    }
    if (mode != SecretMode::MP3) {
        std::cout << "║  Press SHIFT rapidly again to exit                       ║" << std::endl;
    }
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    // Only apply presets for NJD and UFO modes (not PitchDelay or MP3)
    if (mode == SecretMode::NJD || mode == SecretMode::UFO) {
        applySecretModePreset();
    }
}

void GPIOController::exitSecretMode() {
    SecretMode currentMode = secretMode.load();
    if (currentMode == SecretMode::None) return;

    const char* modeName;
    if (currentMode == SecretMode::NJD) {
        modeName = "NJD SIREN";
    } else if (currentMode == SecretMode::UFO) {
        modeName = "UFO";
    } else if (currentMode == SecretMode::MP3) {
        modeName = "MP3 PLAYER";
    } else {
        modeName = "PITCH-DELAY LINK";
    }

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║              SECRET MODE DEACTIVATED                     ║" << std::endl;
    std::cout << "║  Exiting " << modeName << " mode..." << std::string(43 - strlen(modeName), ' ') << "║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    // If exiting MP3 mode, disable it in the engine
    if (currentMode == SecretMode::MP3) {
        engine.disableMP3Mode();
    }

    secretMode.store(SecretMode::None);
    secretModePreset.store(0);

    // Return LED to normal mode
    if (ledController) {
        ledController->setMode(LEDMode::Normal);
    }

    // Only restore default parameters when exiting NJD or UFO modes
    // (PitchDelay mode doesn't change parameters, just behavior)
    if (currentMode == SecretMode::NJD || currentMode == SecretMode::UFO) {
        // Restore default parameters (Auto Wail preset)
        params.volume = 0.6f;
        params.lfoDepth = 0.5f;      // LFO filter modulation depth
        params.lfoRate = 2.0f;       // 2 Hz - wee-woo every 0.5 seconds
        params.filterFreq = 3000.0f; // Standard filter setting for siren
        params.baseFreq = 440.0f;    // A4 - standard siren pitch
        params.filterRes = 0.5f;     // Standard resonance
        params.delayFeedback = 0.55f;// Spacey dub echoes
        params.delayTime = 0.375f;   // Dotted eighth - classic dub
        params.reverbMix = 0.4f;     // Wet for atmosphere
        params.reverbSize = 0.7f;    // Large dub space
        params.release = 0.5f;       // Medium release
        params.oscWaveform = 1;      // Square for classic siren sound

        // Apply restored parameters (Auto Wail preset)
        engine.setVolume(params.volume);
        engine.setLfoDepth(params.lfoDepth);        // Filter modulation depth
        engine.setLfoPitchDepth(0.5f);              // Auto Wail pitch modulation (wee-woo)
        engine.setLfoRate(params.lfoRate);
        engine.setLfoWaveform(Waveform::Triangle);  // Smooth pitch transitions
        engine.setFilterCutoff(params.filterFreq);
        engine.setFrequency(params.baseFreq);
        engine.setFilterResonance(params.filterRes);
        engine.setDelayFeedback(params.delayFeedback);
        engine.setDelayTime(params.delayTime);
        engine.setReverbMix(params.reverbMix);
        engine.setReverbSize(params.reverbSize);
        engine.setReleaseTime(params.release);
        engine.setWaveform(params.oscWaveform);

        std::cout << "Parameters restored to defaults" << std::endl;
    }
}

void GPIOController::cycleSecretModePreset() {
    SecretMode currentMode = secretMode.load();

    int numPresets = (currentMode == SecretMode::NJD) ? 5 : 4;  // 5 NJD presets, 4 UFO presets
    int currentPreset = secretModePreset.load();
    secretModePreset.store((currentPreset + 1) % numPresets);
    
    applySecretModePreset();
}

void GPIOController::applySecretModePreset() {
    SecretMode currentMode = secretMode.load();
    int preset = secretModePreset.load();  // Load once for consistent use throughout
    
    // Preset parameters: baseFreq, filterFreq, filterRes, release, oscWaveform, delayTime, delayFeedback, reverbSize, reverbMix
    
    if (currentMode == SecretMode::NJD) {
        // NJD Classic Dub Siren Presets
        // These are inspired by the classic NJD siren sounds
        const char* presetNames[] = {"Auto Wail", "Classic", "Alert", "Bright", "Wobble"};

        switch (preset) {
            case 0: // Auto Wail - automatic pitch-alternating siren (wee-woo-wee-woo)
                params.baseFreq = 440.0f;     // A4 - standard siren pitch
                params.filterFreq = 3000.0f;  // Standard filter setting
                params.filterRes = 0.5f;      // Standard resonance
                params.release = 0.5f;        // Medium release
                params.oscWaveform = 1;       // Square for classic siren sound
                params.delayTime = 0.375f;    // Dotted eighth - classic dub
                params.delayFeedback = 0.55f; // Spacey dub echoes
                params.reverbSize = 0.7f;     // Large dub space
                params.reverbMix = 0.4f;      // Wet for atmosphere
                // Apply LFO pitch modulation for automatic wail
                engine.setLfoRate(2.0f);      // 2 Hz - wee-woo every 0.5 seconds
                engine.setLfoPitchDepth(0.5f); // ±0.5 octaves for noticeable pitch swing
                engine.setLfoWaveform(Waveform::Triangle); // Smooth pitch transitions
                break;

            case 1: // Classic NJD - the original dub siren sound
                params.baseFreq = 587.0f;     // D5 - classic siren note
                params.filterFreq = 3000.0f;
                params.filterRes = 0.5f;      // Increased for more character
                params.release = 0.8f;
                params.oscWaveform = 1;       // Square for more edge
                params.delayTime = 0.375f;    // Dotted eighth for reggae feel
                params.delayFeedback = 0.5f;  // Classic dub echoes
                params.reverbSize = 0.65f;    // Deep dub space
                params.reverbMix = 0.35f;
                // Reset LFO pitch modulation (not used in this preset)
                engine.setLfoPitchDepth(0.0f);
                break;

            case 2: // Alert - emergency siren for rapid on/off triggering
                params.baseFreq = 440.0f;     // A4 - mid-range wail
                params.filterFreq = 2500.0f;
                params.filterRes = 0.4f;
                params.release = 0.3f;        // Short for clean toggling
                params.oscWaveform = 1;       // Square for harsh siren sound
                params.delayTime = 0.375f;    // Dotted eighth - classic dub
                params.delayFeedback = 0.55f; // Spacey dub echoes
                params.reverbSize = 0.7f;     // Large dub space
                params.reverbMix = 0.4f;      // Wet for atmosphere
                // Reset LFO pitch modulation (not used in this preset)
                engine.setLfoPitchDepth(0.0f);
                break;

            case 3: // Bright - cutting through the mix
                params.baseFreq = 880.0f;     // A5 - bright and piercing
                params.filterFreq = 6000.0f;
                params.filterRes = 0.3f;
                params.release = 0.5f;
                params.oscWaveform = 1;       // Square for edge
                params.delayTime = 0.25f;
                params.delayFeedback = 0.55f;
                params.reverbSize = 0.4f;
                params.reverbMix = 0.35f;
                // Reset LFO pitch modulation (not used in this preset)
                engine.setLfoPitchDepth(0.0f);
                break;

            case 4: // Wobble - with heavy resonance
                params.baseFreq = 392.0f;     // G4
                params.filterFreq = 1500.0f;
                params.filterRes = 0.75f;     // Heavy resonance
                params.release = 1.0f;
                params.oscWaveform = 2;       // Sawtooth
                params.delayTime = 0.333f;    // Triplet feel
                params.delayFeedback = 0.6f;
                params.reverbSize = 0.5f;
                params.reverbMix = 0.4f;
                // Reset LFO pitch modulation (not used in this preset)
                engine.setLfoPitchDepth(0.0f);
                break;
        }

        std::cout << "[NJD MODE] Preset " << (preset + 1) << "/5: " 
                  << presetNames[preset] << std::endl;
                  
    } else if (currentMode == SecretMode::UFO) {
        // UFO Sci-Fi Presets
        const char* presetNames[] = {"Laser Blast", "Flying Saucer", "Alien Signal", "Warp Drive"};

        switch (preset) {
            case 0: // Laser Blast - Star Wars style pew pew
                params.baseFreq = 1600.0f;    // Bright, sharp
                params.filterFreq = 6000.0f;  // Very bright
                params.filterRes = 0.3f;
                params.release = 0.15f;       // Very short, snappy
                params.oscWaveform = 1;       // Square for harsh edge
                params.delayTime = 0.03f;     // Very short for texture
                params.delayFeedback = 0.4f;  // Moderate
                params.reverbSize = 0.2f;     // Minimal space
                params.reverbMix = 0.15f;     // Dry, punchy
                break;

            case 1: // Flying Saucer - classic UFO whoosh
                params.baseFreq = 1200.0f;    // High pitch
                params.filterFreq = 4000.0f;
                params.filterRes = 0.4f;
                params.release = 2.0f;        // Long decay
                params.oscWaveform = 0;       // Sine for clean tone
                params.delayTime = 0.1f;      // Short slapback
                params.delayFeedback = 0.7f;  // Lots of repeats
                params.reverbSize = 0.9f;     // Huge space
                params.reverbMix = 0.5f;
                break;

            case 2: // Alien Signal - digital beeps
                params.baseFreq = 1800.0f;    // Very high
                params.filterFreq = 8000.0f;
                params.filterRes = 0.6f;
                params.release = 0.3f;        // Quick
                params.oscWaveform = 1;       // Square for digital feel
                params.delayTime = 0.05f;     // Very short
                params.delayFeedback = 0.8f;  // Heavy feedback
                params.reverbSize = 0.3f;
                params.reverbMix = 0.6f;
                break;

            case 3: // Warp Drive - deep space rumble
                params.baseFreq = 80.0f;      // Sub bass
                params.filterFreq = 2000.0f;
                params.filterRes = 0.85f;     // Heavy resonance
                params.release = 3.0f;        // Very long
                params.oscWaveform = 2;       // Sawtooth for harmonics
                params.delayTime = 0.75f;
                params.delayFeedback = 0.5f;
                params.reverbSize = 0.95f;    // Maximum space
                params.reverbMix = 0.45f;
                break;
        }

        std::cout << "[UFO MODE] Preset " << (preset + 1) << "/4: "
                  << presetNames[preset] << std::endl;
    }
    
    // Apply all parameters to engine (delay and reverb always active)
    engine.setFrequency(params.baseFreq);
    engine.setFilterCutoff(params.filterFreq);
    engine.setFilterResonance(params.filterRes);
    engine.setReleaseTime(params.release);
    engine.setWaveform(params.oscWaveform);
    engine.setDelayTime(params.delayTime);
    engine.setDelayFeedback(params.delayFeedback);
    engine.setReverbSize(params.reverbSize);
    engine.setReverbMix(params.reverbMix);
    
    std::cout << "  Base: " << params.baseFreq << "Hz, Filter: " << params.filterFreq 
              << "Hz, Release: " << params.release << "s" << std::endl;
}

void GPIOController::updateLEDAudioLevel(float level) {
    if (ledController) {
        ledController->setAudioLevel(level);
    }
}

void GPIOController::checkMP3PlaybackStatus() {
    SecretMode currentMode = secretMode.load();

    if (currentMode == SecretMode::MP3) {
        // Check if MP3 has finished playing
        if (engine.hasMP3Finished()) {
            std::cout << "\n[MP3] Playback finished - returning to synthesis mode" << std::endl;
            exitSecretMode();
        }
    }
}

// ============================================================================
// SimulatedController Implementation
// ============================================================================

SimulatedController::SimulatedController(AudioEngine& engine)
    : engine(engine)
    , running(false)
{
}

void SimulatedController::start() {
    running.store(true);
    printHelp();
}

void SimulatedController::stop() {
    running.store(false);
}

void SimulatedController::processCommand(char cmd) {
    switch (cmd) {
        case 't':
            if (engine.isPlaying()) {
                std::cout << "Trigger: RELEASED" << std::endl;
                engine.release();
            } else {
                std::cout << "Trigger: PRESSED" << std::endl;
                engine.trigger();
            }
            break;
            
        case 'p': {
            const char* mode = engine.cyclePitchEnvelope();
            std::cout << "Pitch envelope: " << mode << std::endl;
            break;
        }
        
        case 's':
            std::cout << "\nStatus:" << std::endl;
            std::cout << "  Playing: " << (engine.isPlaying() ? "yes" : "no") << std::endl;
            std::cout << "  Volume: " << engine.getVolume() << std::endl;
            std::cout << "  Frequency: " << engine.getFrequency() << " Hz" << std::endl;
            break;
            
        case 'h':
        case '?':
            printHelp();
            break;
            
        case 'q':
            running.store(false);
            break;
            
        default:
            break;
    }
}

void SimulatedController::printHelp() {
    std::cout << "\nSimulated Control Surface" << std::endl;
    std::cout << "=========================" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  t - Trigger siren (toggle)" << std::endl;
    std::cout << "  p - Cycle pitch envelope mode" << std::endl;
    std::cout << "  s - Show status" << std::endl;
    std::cout << "  h - Show this help" << std::endl;
    std::cout << "  q - Quit" << std::endl;
    std::cout << std::endl;
}

} // namespace DubSiren
