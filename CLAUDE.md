# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Jester Scribe is an ESP8266-based thermal printer project that sends daily jokes printed on thermal paper. The project uses PlatformIO and targets ESP8266 boards (d1_mini_lite and esp12e). It includes a WiFi setup system with captive portal for initial configuration, and thermal printer functionality for printing receipts.

## Build Commands

```bash
# Build the project
pio run

# Build and upload to device
pio run --target upload

# Build and upload for specific board
pio run -e d1_mini_lite --target upload
pio run -e esp12e --target upload

# Upload filesystem (LittleFS) containing web interface files
pio run --target uploadfs

# Monitor serial output (115200 baud)
pio device monitor

# Build, upload, and monitor in sequence
pio run --target upload && pio device monitor

# Clean build files
pio run --target clean
```

## Architecture

### Two-Phase Execution Model

The codebase separates WiFi setup from the main application:

1. **WiFi Setup Phase** (`wifi_setup.cpp/h`)
   - Attempts to load saved credentials from LittleFS (`/config.json`)
   - If no credentials or connection fails, launches captive portal
   - Captive portal runs as open AP "ESP8266-WiFi-Setup"
   - Web interface allows users to scan and select WiFi networks
   - Validates SSID (1-32 chars) and password (8-63 chars) before saving
   - Once connected, control passes to user program

2. **User Program Phase** (`user_program.cpp/h`)
   - Only executes after successful WiFi connection
   - Currently serves basic web server on port 80
   - **Integration Point**: Thermal printer functionality from `firmware.ino` should be integrated here

### Main Entry Point (`main.cpp`)

The `setup()` function orchestrates both phases:
```cpp
wifiSetupInit();      // Initialize LittleFS and WiFi subsystem
wifiSetupConnect();   // Connect or launch captive portal (blocking)
userProgramSetup();   // Start main application
```

The `loop()` function monitors WiFi connection (restarts on disconnect) and calls `userProgramLoop()`.

### Thermal Printer Integration (from firmware.ino)

The `firmware.ino` file contains the reference implementation that needs to be integrated into `user_program.cpp`:

**Hardware Setup:**
- Thermal printer connected via SoftwareSerial
- TX: D4 (GPIO2), RX: D3 (GPIO0)
- Baud rate: 9600
- Max line width: 32 characters

**Key Components:**
- **NTPClient**: Fetches current date/time from `pool.ntp.org` for timestamping receipts
- **Printer Initialization**: ESC/POS commands to reset, configure heat settings, and prepare printer
- **Receipt Structure**: Stores message and timestamp for printing
- **Print Functions**:
  - `printReceipt()` - Prints timestamped message with word wrapping
  - `printWrapped()` - Word wraps text at 32 chars per line
  - `setInverse()` - Toggles inverse mode for headers
  - `advancePaper()` - Advances paper after printing
- **Web Interface**: Form-based message submission with character counter (max 200 chars)

**ESC/POS Commands Used:**
- `ESC @` (0x1B '@') - Reset printer
- `ESC 7` (0x1B '7') - Configure heat settings (dots, time, interval)
- `GS B n` (0x1D 'B' n) - Toggle inverse/white-on-black mode
- `LF` (0x0A) - Line feed for paper advance

**Integration Notes:**
- Add NTPClient initialization in `userProgramSetup()`
- Move printer initialization to `userProgramSetup()`
- Adapt web server routes from firmware.ino to AsyncWebServer (current code uses ESP8266WebServer)
- Implement periodic joke fetching in `userProgramLoop()`
- Consider adding additional LittleFS dependency: `NTPClient` library needs to be added to `platformio.ini`

### Filesystem Structure

The `data/` directory contains files uploaded to LittleFS:
- `index.html` - Captive portal WiFi setup interface
- `main.html` - Main application web interface (currently minimal)
- `style.css`, `script.js` - Assets for web interfaces
- `/config.json` - WiFi credentials (created at runtime by wifi_setup)

Note: `firmware.ino` embeds its HTML in code as a raw string literal. This could be moved to `main.html` or kept embedded.

### Key Dependencies

Current (platformio.ini):
- `ESPAsyncWebServer` - Async web server
- `CaptivePortal` (lennart080) - Captive portal implementation
- `ArduinoJson` - JSON parsing for config storage
- `LittleFS` - Filesystem for ESP8266

Required for thermal printer (from firmware.ino):
- `NTPClient` - Time synchronization for receipt timestamps
- `SoftwareSerial` - Software UART for thermal printer (built-in to Arduino framework)
- Standard ESP8266WiFi and ESP8266WebServer libraries

### Printer Timing and Delays

The thermal printer requires careful timing (lessons from firmware.ino):
- 3 second delay on initialization for capacitor charging
- 500ms delay after ESC @ reset command
- 50ms delays after mode changes (inverse, etc.)
- 100ms delays between line feeds
- Small delays (50-200ms) between print commands to prevent buffer overflow

### Debug Logging System

The firmware.ino implements a circular buffer logging system that stores the last 50 log messages and serves them via `/logs` endpoint. This is useful for debugging without serial monitor access.

## Development Workflow

To add thermal printer functionality:
1. Add NTPClient library to `platformio.ini`: `arduino-libraries/NTPClient`
2. Move printer initialization code from firmware.ino to `user_program.cpp`
3. Adapt web server handlers from ESP8266WebServer to ESPAsyncWebServer
4. Implement joke-fetching logic (API integration TBD)
5. Wire up printer functions to joke delivery system
6. Test printer timing and heat settings for your specific hardware
