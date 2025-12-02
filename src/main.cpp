#include <Arduino.h>
#include <LittleFS.h>
#include "wifi_setup.h"
#include "main_program.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=================================");
    Serial.println("ESP8266 Starting...");
    Serial.println("=================================");

    // Mount LittleFS once at program start - main.cpp owns filesystem lifecycle
    Serial.println("Mounting LittleFS filesystem...");
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed, formatting...");
        if (!LittleFS.format()) {
            Serial.println("CRITICAL: LittleFS format failed!");
            // Continue anyway - will fail later but won't crash
        } else {
            Serial.println("LittleFS formatted successfully");
            if (!LittleFS.begin()) {
                Serial.println("CRITICAL: Cannot mount LittleFS after format!");
            } else {
                Serial.println("LittleFS mounted after format");
            }
        }
    } else {
        Serial.println("LittleFS mounted successfully");
    }

    // Initialize WiFi setup system
    wifiSetupInit();

    // Connect to WiFi (may start captive portal if needed)
    if (wifiSetupConnect()) {
        // Wait for WiFi subsystem to fully stabilize after mode transition
        // This ensures clean transition from AP mode to STA mode
        Serial.println("WiFi connected, waiting for subsystem to stabilize...");
        delay(2000);

        // Remount LittleFS (CaptivePortal calls LittleFS.end() on exit)
        Serial.println("Remounting LittleFS after WiFi setup...");
        if (!LittleFS.begin()) {
            Serial.println("ERROR: Failed to remount LittleFS!");
        } else {
            Serial.println("LittleFS remounted successfully");
        }

        // Start user program after successful WiFi connection
        mainProgramSetup();
    }
}

void loop() {
    // Check WiFi connection status
    if (!isWifiConnected()) {
        Serial.println("WiFi connection lost! Restarting...");
        delay(1000);
        ESP.restart();
    }

    // Run user program loop
    mainProgramLoop();

    delay(100);
}
