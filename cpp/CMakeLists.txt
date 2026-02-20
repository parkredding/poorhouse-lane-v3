cmake_minimum_required(VERSION 3.16)
project(DubSirenV2 VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(BUILD_WITH_JUCE "Build with JUCE framework (for full features)" OFF)
option(BUILD_FOR_PI "Build optimizations for Raspberry Pi" OFF)
option(BUILD_STANDALONE "Build standalone ALSA version (no JUCE)" ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Raspberry Pi specific optimizations
if(BUILD_FOR_PI)
    message(STATUS "Building with Raspberry Pi optimizations")
    # Detect architecture - 64-bit aarch64 vs 32-bit armhf
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        # 64-bit ARM (Pi Zero 2W with 64-bit OS, Pi 4/5 64-bit)
        # NEON is always available, no -mfpu/-mfloat-abi needed
        message(STATUS "Detected 64-bit ARM (aarch64)")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-a53")
    else()
        # 32-bit ARM (older Pi OS)
        message(STATUS "Detected 32-bit ARM")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard")
    endif()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math")
endif()

# Find threading support
# On some newer Debian systems, FindThreads fails due to CMake detection issues
# We handle this by falling back to manual pthread linking
find_package(Threads)
if(NOT Threads_FOUND)
    message(STATUS "CMake Threads detection failed, using manual pthread linking")
    set(CMAKE_THREAD_LIBS_INIT "-pthread")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()

# ALSA for standalone build
if(BUILD_STANDALONE)
    find_package(ALSA)
    if(ALSA_FOUND)
        message(STATUS "ALSA found: ${ALSA_LIBRARIES}")
    else()
        message(WARNING "ALSA not found - audio output will be simulated")
    endif()
endif()

# Source files
set(DSP_SOURCES
    src/DSP/Oscillator.cpp
    src/DSP/Envelope.cpp
    src/DSP/Filter.cpp
    src/DSP/Delay.cpp
    src/DSP/Reverb.cpp
    src/DSP/LFO.cpp
)

set(AUDIO_SOURCES
    src/Audio/AudioEngine.cpp
    src/Audio/AudioOutput.cpp
    src/Audio/AudioFilePlayer.cpp
)

set(HARDWARE_SOURCES
    src/Hardware/GPIOController.cpp
    src/Hardware/LEDController.cpp
)

set(MAIN_SOURCES
    src/main.cpp
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# Create the executable
add_executable(dubsiren
    ${DSP_SOURCES}
    ${AUDIO_SOURCES}
    ${HARDWARE_SOURCES}
    ${MAIN_SOURCES}
)

# Link libraries
target_link_libraries(dubsiren
    PRIVATE
    m  # math library
)

# Link pthreads (handle both CMake-detected and manual cases)
if(Threads_FOUND)
    target_link_libraries(dubsiren PRIVATE Threads::Threads)
else()
    target_link_libraries(dubsiren PRIVATE pthread)
    target_compile_options(dubsiren PRIVATE -pthread)
endif()

if(ALSA_FOUND)
    target_link_libraries(dubsiren PRIVATE ${ALSA_LIBRARIES})
    target_compile_definitions(dubsiren PRIVATE HAVE_ALSA=1)
    target_include_directories(dubsiren PRIVATE ${ALSA_INCLUDE_DIRS})
endif()

# Raspberry Pi GPIO library - use libgpiod (modern Linux GPIO interface)
if(BUILD_FOR_PI)
    # Try to find libgpiod (works on modern Raspberry Pi OS)
    find_library(GPIOD_LIBRARY gpiod)
    if(GPIOD_LIBRARY)
        message(STATUS "libgpiod found: ${GPIOD_LIBRARY}")
        target_link_libraries(dubsiren PRIVATE ${GPIOD_LIBRARY})
        target_compile_definitions(dubsiren PRIVATE HAVE_GPIOD=1)
    else()
        # Fallback to pigpio if available
        find_library(PIGPIO_LIBRARY pigpio)
        if(PIGPIO_LIBRARY)
            message(STATUS "pigpio found: ${PIGPIO_LIBRARY}")
            target_link_libraries(dubsiren PRIVATE ${PIGPIO_LIBRARY})
            target_compile_definitions(dubsiren PRIVATE HAVE_PIGPIO=1)
        else()
            message(WARNING "No GPIO library found - GPIO will be simulated")
        endif()
    endif()
    
    # WS2812 LED library (optional)
    find_library(WS2811_LIBRARY ws2811)
    if(WS2811_LIBRARY)
        message(STATUS "ws2811 (WS2812 LED) found: ${WS2811_LIBRARY}")
        target_link_libraries(dubsiren PRIVATE ${WS2811_LIBRARY})
        target_compile_definitions(dubsiren PRIVATE HAVE_WS2812=1)
    else()
        message(STATUS "ws2811 library not found - LED will be simulated")
        message(STATUS "  To enable WS2812 LED: sudo apt-get install libws2811-dev")
    endif()
endif()

# Install target
install(TARGETS dubsiren
    RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== DubSiren V2 Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build for Pi: ${BUILD_FOR_PI}")
message(STATUS "Build standalone: ${BUILD_STANDALONE}")
message(STATUS "ALSA support: ${ALSA_FOUND}")
message(STATUS "========================================")
message(STATUS "")
