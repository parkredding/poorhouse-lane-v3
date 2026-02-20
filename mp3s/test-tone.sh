#!/bin/bash
# Generate a simple test MP3 using ffmpeg
# This creates a 3-second 440Hz sine wave (A4 note)

ffmpeg -f lavfi -i "sine=frequency=440:duration=3" -af "volume=0.5" -b:a 128k test-tone.mp3 2>/dev/null

echo "Generated test-tone.mp3 (3 second 440Hz sine wave)"
