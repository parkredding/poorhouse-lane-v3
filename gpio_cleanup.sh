#!/bin/bash
# GPIO Cleanup Script
# Clears kernel-level GPIO edge detection state before starting the dubsiren service
# This is necessary when the service crashes without proper GPIO cleanup

# All GPIO pins used by the Dub Siren (from gpio_controller.py)
# 5 Encoders (clk, dt pairs) + 4 Switches = 14 GPIO pins total
# NOTE: Avoids I2S pins (18, 19, 21) used by PCM5102 DAC

# Encoder pins: 5 encoders x 2 pins = 10 pins
ENCODER_PINS=(17 2 27 22 23 24 20 26 14 13)

# Switch pins: 4 switches
SWITCH_PINS=(4 10 15 3)

# Combine all pins
ALL_PINS=("${ENCODER_PINS[@]}" "${SWITCH_PINS[@]}")

# Function to cleanup a GPIO pin via sysfs
cleanup_gpio_pin() {
    local pin=$1
    local gpio_path="/sys/class/gpio/gpio${pin}"
    local export_path="/sys/class/gpio/export"
    local unexport_path="/sys/class/gpio/unexport"

    # If the pin is already exported, unexport it (this clears edge detection)
    if [ -d "$gpio_path" ]; then
        echo "$pin" > "$unexport_path" 2>/dev/null || true
    else
        # Try exporting and then unexporting to force a reset
        echo "$pin" > "$export_path" 2>/dev/null || true
        sleep 0.01  # Small delay to let kernel process the export
        echo "$pin" > "$unexport_path" 2>/dev/null || true
    fi
}

# Cleanup all GPIO pins used by the Dub Siren
for pin in "${ALL_PINS[@]}"; do
    cleanup_gpio_pin "$pin"
done

# Give the kernel a moment to fully release the GPIO resources
sleep 0.1

exit 0
