# MP3 Playback Mode

This directory is for MP3 files that can be played in MP3 mode on the Dub Siren.

## How to Use

1. Place MP3 files in this directory (or in `/home/pi/dubsiren/mp3s` on the Raspberry Pi)
2. Activate MP3 mode by flipping the pitch envelope switch from OFF to ON 5 times within 2 seconds
3. The LED will start flashing slowly in white (or the color assigned to the first MP3)
4. Press the TRIGGER button to start playback
5. Press SHIFT to cycle through different MP3 files (LED changes color for each file)
6. The mode will automatically exit when the MP3 finishes playing

## File Format

- Supported format: MP3
- Files are automatically resampled to 48kHz stereo if needed
- Files are loaded in alphabetical order

## LED Colors

Each MP3 file is assigned a different color when selected:

1. White (first file)
2. Magenta
3. Cyan
4. Orange
5. Purple
6. Yellow
7. Spring Green
8. Rose

Colors cycle if you have more than 8 files.

## Notes

- This is a one-shot playback mode - after the MP3 finishes, the system returns to normal synthesis mode
- MP3 files are loaded into memory at mode activation, so very long files may use significant RAM
- The directory path on the Raspberry Pi should be `/home/pi/dubsiren/mp3s`

## Generating Test Files

To generate a simple test tone MP3 (requires ffmpeg):

```bash
cd /home/pi/dubsiren/mp3s
./test-tone.sh
```

Or manually:
```bash
ffmpeg -f lavfi -i "sine=frequency=440:duration=3" -af "volume=0.5" -b:a 128k test-tone.mp3
```
