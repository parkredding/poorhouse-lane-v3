/**
 * Dub Siren V2 - C++ Implementation
 * 
 * A professional dub siren synthesizer for Raspberry Pi Zero 2
 * with PCM5102 I2S DAC.
 * 
 * Usage:
 *   dubsiren [options]
 * 
 * Options:
 *   --sample-rate RATE    Audio sample rate (default: 48000)
 *   --buffer-size SIZE    Audio buffer size (default: 256)
 *   --device DEVICE       ALSA audio device (default: "default")
 *   --simulate           Run in simulation mode (no hardware)
 *   --interactive        Run in interactive mode (keyboard control)
 *   --help               Show this help message
 */

#include <iostream>
#include <csignal>
#include <atomic>
#include <cstring>
#include <cfenv>

#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

#ifdef __linux__
#include <sys/mman.h>
#endif

#include "Common.h"
#include "Audio/AudioEngine.h"
#include "Audio/AudioOutput.h"
#include "Hardware/GPIOController.h"

using namespace DubSiren;

// Enable flush-to-zero for denormal numbers (prevents CPU spikes in DSP)
void enableFlushToZero() {
#if defined(__aarch64__)
    // ARM64: Set FZ bit in FPCR
    uint64_t fpcr;
    asm volatile("mrs %0, fpcr" : "=r"(fpcr));
    fpcr |= (1 << 24);  // FZ bit
    asm volatile("msr fpcr, %0" : : "r"(fpcr));
#elif defined(__arm__)
    // ARM32: Set FZ bit in FPSCR
    uint32_t fpscr;
    asm volatile("vmrs %0, fpscr" : "=r"(fpscr));
    fpscr |= (1 << 24);
    asm volatile("vmsr fpscr, %0" : : "r"(fpscr));
#elif defined(__x86_64__) || defined(__i386__)
    // x86: Use MXCSR for SSE flush-to-zero
    _mm_setcsr(_mm_getcsr() | 0x8040);  // FZ and DAZ bits
#endif
}

// Global flag for signal handling
std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    (void)signal;
    std::cout << "\nShutting down..." << std::endl;
    g_running.store(false);
}

void printBanner() {
    std::cout << "\n";
    std::cout << "============================================================" << std::endl;
    std::cout << "  Poor House Dub v2" << std::endl;
    std::cout << "  Raspberry Pi Zero 2 + PCM5102 DAC" << std::endl;
    std::cout << "  C++ Edition" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "\n";
}

void printHelp(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --sample-rate RATE    Audio sample rate (default: 48000)\n";
    std::cout << "  --buffer-size SIZE    Audio buffer size (default: 256)\n";
    std::cout << "  --device DEVICE       ALSA audio device (default: \"default\")\n";
    std::cout << "  --simulate           Run in simulation mode (no hardware)\n";
    std::cout << "  --interactive        Run in interactive mode (keyboard control)\n";
    std::cout << "  --help               Show this help message\n";
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    // Default configuration
    int sampleRate = DEFAULT_SAMPLE_RATE;
    int bufferSize = DEFAULT_BUFFER_SIZE;
    const char* device = nullptr;
    bool simulate = false;
    bool interactive = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printHelp(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--sample-rate") == 0 && i + 1 < argc) {
            sampleRate = std::atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--buffer-size") == 0 && i + 1 < argc) {
            bufferSize = std::atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            device = argv[++i];
        }
        else if (strcmp(argv[i], "--simulate") == 0) {
            simulate = true;
        }
        else if (strcmp(argv[i], "--interactive") == 0) {
            interactive = true;
        }
        else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printHelp(argv[0]);
            return 1;
        }
    }
    
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Enable flush-to-zero to prevent denormal CPU spikes
    enableFlushToZero();

    // Lock all current and future memory pages to prevent page faults in the
    // audio thread.  A single page fault can stall the thread for milliseconds,
    // long enough to drain the ALSA ring buffer and cause an audible glitch.
#ifdef __linux__
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        std::cerr << "Warning: mlockall() failed (run as root or set memlock in "
                     "/etc/security/limits.conf)" << std::endl;
    }
#endif

    printBanner();
    
    std::cout << "Initializing Dub Siren..." << std::endl;
    std::cout << "  Sample rate: " << sampleRate << " Hz" << std::endl;
    std::cout << "  Buffer size: " << bufferSize << " samples" << std::endl;
    std::cout << "  Mode: " << (simulate ? "Simulation" : "Hardware") << std::endl;
    std::cout << "\n";
    
    // Create audio engine
    AudioEngine engine(sampleRate, bufferSize);
    
    // Create audio output
    std::unique_ptr<AudioOutput> audioOutput;
    std::unique_ptr<SimulatedAudioOutput> simAudioOutput;
    
    if (simulate) {
        simAudioOutput = std::make_unique<SimulatedAudioOutput>(engine, bufferSize);
        if (!simAudioOutput->start()) {
            std::cerr << "Failed to start simulated audio output" << std::endl;
            return 1;
        }
    } else {
        audioOutput = std::make_unique<AudioOutput>(engine, sampleRate, bufferSize, DEFAULT_CHANNELS, device);
        if (!audioOutput->start()) {
            std::cerr << "Failed to start audio output" << std::endl;
            std::cerr << "\nTroubleshooting:" << std::endl;
            std::cerr << "  1. Check ALSA config: cat /etc/asound.conf" << std::endl;
            std::cerr << "  2. Test ALSA directly: aplay -l" << std::endl;
            std::cerr << "  3. Check audio group: groups (should include 'audio')" << std::endl;
            return 1;
        }
    }
    
    // Create control surface
    std::unique_ptr<GPIOController> gpioController;
    std::unique_ptr<SimulatedController> simController;
    
    if (simulate || interactive) {
        simController = std::make_unique<SimulatedController>(engine);
        simController->start();
    } else {
        gpioController = std::make_unique<GPIOController>(engine, [&]() {
            g_running.store(false);
        });
        gpioController->start();
    }
    
    std::cout << "\n✓ Dub Siren is running!" << std::endl;
    std::cout << "\n";
    
    // Main loop
    if (interactive) {
        std::cout << "Interactive mode - press 't' to trigger, 'q' to quit" << std::endl;
        
        while (g_running.load()) {
            char cmd;
            std::cin >> cmd;
            
            if (simController) {
                simController->processCommand(cmd);
            }
            
            if (cmd == 'q') {
                g_running.store(false);
            }
        }
    } else {
        std::cout << "Press Ctrl+C to exit" << std::endl;

        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Check MP3 playback status for auto-exit
            if (gpioController) {
                gpioController->checkMP3PlaybackStatus();
            }
        }
    }
    
    // Cleanup
    std::cout << "\nShutting down..." << std::endl;
    
    if (gpioController) {
        gpioController->stop();
    }
    if (simController) {
        simController->stop();
    }
    
    if (audioOutput) {
        audioOutput->stop();
    }
    if (simAudioOutput) {
        simAudioOutput->stop();
    }
    
    std::cout << "Goodbye!" << std::endl;
    
    return 0;
}
