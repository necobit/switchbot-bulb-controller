# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a PlatformIO project for controlling SwitchBot bulbs using an M5Stack AtomS3 (ESP32-based) microcontroller.

## Build Commands

```bash
# Build the project
pio run

# Upload to device
pio run --target upload

# Clean build files
pio run --target clean

# Monitor serial output
pio device monitor

# Build and upload in one step
pio run --target upload && pio device monitor
```

## Project Structure

- `src/main.cpp` - Main application entry point with `setup()` and `loop()` functions
- `include/` - Project header files
- `lib/` - Project-specific private libraries
- `platformio.ini` - PlatformIO configuration (target: m5stack-atoms3, ESP32, Arduino framework)

## Hardware Target

- Board: M5Stack AtomS3
- Platform: Espressif32
- Framework: Arduino
