#include "user_program.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// Web server for main program
AsyncWebServer server(80);

void userProgramSetup() {
    Serial.println("=================================");
    Serial.println("Main Program Starting...");
    Serial.println("=================================");

    // Setup minimal web server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/main.html", "text/html");
    });

    // Serve static files
    server.serveStatic("/style.css", LittleFS, "/style.css");
    server.serveStatic("/script.js", LittleFS, "/script.js");

    server.begin();
    Serial.println("Web server started on port 80");

    // TODO: Add your program initialization here
    // For example:
    // - Initialize sensors
    // - Configure GPIO pins
    // - Setup additional API endpoints
}

void userProgramLoop() {
    // TODO: Add your main program logic here
    // This will run continuously after WiFi connection is established
}
