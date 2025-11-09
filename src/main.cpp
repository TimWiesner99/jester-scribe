#include <Arduino.h>
#include "wifi_setup.h"
#include "user_program.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=================================");
    Serial.println("ESP8266 Starting...");
    Serial.println("=================================");

    // Initialize WiFi setup system
    wifiSetupInit();

    // Connect to WiFi (may start captive portal if needed)
    if (wifiSetupConnect()) {
        // Wait for WiFi subsystem to fully stabilize after mode transition
        // This ensures clean transition from AP mode to STA mode
        Serial.println("WiFi connected, waiting for subsystem to stabilize...");
        delay(2000);

        // Start user program after successful WiFi connection
        userProgramSetup();
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
    userProgramLoop();

    delay(100);
}
