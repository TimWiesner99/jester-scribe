# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Jester Scribe** is an ESP8266-based thermal printer project that prints jokes (or any messages) on thermal paper. It's a fork/adaptation of the "Scribe" project by UrbanCircles. The device hosts a web server where users can submit messages to be printed on a CSN-A4L thermal panel printer.

## Hardware Setup

- **Microcontroller**: ESP8266 (WiFi-enabled)
- **Printer Model**: CSN-A4L Micro Panel Printer (Xiamen Cashino Technology)
  - Connection: Software Serial on D4 (TX/GPIO2) and D3 (RX/GPIO0)
  - Baud Rate: 9600
  - Print Resolution: 203dpi (8dots/mm)
  - Max Print Width: 48mm
  - Paper Width: 57.5±0.5mm
- **Communication**: Serial interface (RS232/TTL/USB options available on printer)

## Firmware Architecture

### Main Components (`firmware/firmware.ino`)

1. **WiFi Configuration**
   - SSID and password are hardcoded in the firmware (lines 26-27)
   - Connects on startup with 30-second timeout
   - Uses ESP8266WiFi library

2. **Web Server** (port 80)
   - `/` - Main form interface (HTML with Tailwind CSS)
   - `/submit` - Handles POST/GET submissions
   - `/logs` - Real-time debug log viewer (auto-refreshes every 1s)
   - Built with ESP8266WebServer library

3. **Time Synchronization**
   - NTP client for automatic time sync (pool.ntp.org)
   - UTC offset configurable (default: 0)
   - Formats dates as "Day, DD Mon YYYY" (e.g., "Sat, 06 Jun 2025")

4. **Printer Communication**
   - Software Serial interface at 9600 baud
   - Implements ESC/POS-like commands from CSN-A4L manual
   - Key features:
     - Text wrapping at 32 characters per line
     - Inverse/white-on-black printing for headers
     - Custom paper advance with delays for reliability
     - Initialization delays for capacitor charging (values still being tuned)

### Printing Workflow

1. Server receives message via web form
2. Message stored in `currentReceipt` struct with timestamp
3. `printReceipt()` prints:
   - Inverted header with timestamp
   - Word-wrapped message body
   - Paper advance (2 lines)
4. Receipt flag reset for next message

## Printer Command Reference

The CSN-A4L uses ESC/POS-compatible commands. Key commands used in firmware:

- **Reset**: `ESC @` (0x1B 0x40) - Initialize printer to default state
- **Inverse Mode**: `GS B n` (0x1D 0x42 n) - White-on-black printing (n=1 enable, n=0 disable)
- **Line Feed**: `LF` (0x0A) - Advance paper one line
- **Heat Settings**: `ESC 7` (0x1B 0x37) + 3 bytes - Configure print density

Full command set documented in `csna4l.pdf` (pages 8-48).

## Development Commands

### Building and Uploading

This is an Arduino-compatible project for ESP8266:

```bash
# Using Arduino IDE
# 1. Install ESP8266 board support via Board Manager
# 2. Select board: "Generic ESP8266 Module" or "NodeMCU 1.0"
# 3. Set upload speed: 115200
# 4. Select correct COM port
# 5. Click Upload button

# Using Arduino CLI (if installed)
arduino-cli compile --fqbn esp8266:esp8266:generic firmware/firmware.ino
arduino-cli upload -p COM3 --fqbn esp8266:esp8266:generic firmware/firmware.ino
```

### Required Libraries

Install via Arduino Library Manager:
- ESP8266WiFi (included with ESP8266 board support)
- ESP8266WebServer (included with ESP8266 board support)
- NTPClient (by Fabrice Weinberg)

### Debugging

- Serial monitor at 115200 baud for debug output
- Web-based log viewer at `http://<device-ip>/logs`
- 50-line circular buffer for runtime logs

### WiFi Configuration

To change WiFi credentials, edit lines 26-27 in `firmware/firmware.ino`:
```cpp
const char* ssid = "your-network-name";
const char* password = "your-password";
```

## Important Implementation Notes

1. **Timing is Critical**: The printer requires careful delay management between commands. Current implementation includes delays for:
   - Initial power-up (capacitor charging)
   - After reset command
   - Between print operations
   - Between print jobs

   **Note**: Exact timing values are still experimental and being tuned. Insufficient delays cause communication failures and print quality issues.

2. **Print Orientation**: Version 2 firmware prints in normal orientation (not rotated). Previous versions used 180° rotation.

3. **Character Limits**:
   - Web form enforces 200 character max
   - Printer auto-wraps at 32 characters per line

4. **Paper Management**: The printer uses 57.5mm thermal paper rolls (max 30mm diameter)

5. **Error Handling**: The printer has LED indicators for status:
   - Flash 1x: Self-test normal
   - Flash 2x: No printer detected
   - Flash 3x: No paper
   - Flash 5x: Overheating
   - Flash 10x: No Chinese font chip

## File Structure

```
jester-scribe/
├── firmware/
│   └── firmware.ino       # Main ESP8266 firmware
├── csna4l.pdf             # Printer hardware manual (48 pages)
├── README.md              # Project description
└── LICENSE                # License information
```

## Testing

1. **Hardware Test**: After upload, printer should print "PRINTER SERVER READY" with IP address
2. **Web Interface**: Navigate to device IP in browser
3. **Print Test**: Submit a short message via web form
4. **Debug**: Check `/logs` endpoint for system status

## Common Issues

- **Printer not responding**: Check RX/TX connections (D3/D4), verify 9600 baud rate
- **Weak/faded prints**: Adjust heating parameters in `initializePrinter()` (lines 407-410)
- **Paper jams**: Ensure correct paper width (57.5mm) and proper loading
- **WiFi connection fails**: Verify SSID/password, check 2.4GHz network availability (ESP8266 doesn't support 5GHz)
