#include "wifi_setup.h"
#include <CaptivePortal.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

// Configuration
const char* ap_ssid = "Jester Scribe WiFi-Setup";
const char* ap_password = "12345678";
const char* CONFIG_FILE = "/config.json";
const int wifiConnectionTimeout = 10000;

// Internal state
// Note: CaptivePortal is created locally when needed to avoid port conflicts
String receivedSSID;
String receivedPassword;
String wifiList;
bool shouldStopAP = false;
bool wifiConnected = false;

// Saves WiFi credentials to LittleFS as JSON
bool saveCredentials(String ssid, String password) {
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["password"] = password;

    File configFile = LittleFS.open(CONFIG_FILE, "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return false;
    }

    if (serializeJson(doc, configFile) == 0) {
        Serial.println("Failed to write to config file");
        configFile.close();
        return false;
    }

    configFile.close();
    Serial.println("Credentials saved to config.json");
    return true;
}

// Loads WiFi credentials from LittleFS
bool loadCredentials(String &ssid, String &password) {
    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial.println("Config file does not exist");
        return false;
    }

    File configFile = LittleFS.open(CONFIG_FILE, "r");
    if (!configFile) {
        Serial.println("Failed to open config file for reading");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error) {
        Serial.println("Failed to parse config file");
        return false;
    }

    ssid = doc["ssid"].as<String>();
    password = doc["password"].as<String>();

    if (ssid.length() == 0) {
        Serial.println("Config file contains empty SSID");
        return false;
    }

    Serial.println("Credentials loaded from config.json");
    return true;
}

// Waits for the WiFi mode to switch to the target mode, or until timeout
void waitForWiFiMode(WiFiMode_t targetMode, unsigned long timeoutMs = 2000) {
    unsigned long start = millis();
    while (WiFi.getMode() != targetMode && (millis() - start) < timeoutMs) {
        delay(10);
    }
}

// Verifies internet connectivity by attempting to resolve google.com via DNS
// Returns true if internet is accessible, false otherwise
bool verifyInternetConnectivity() {
    Serial.println("Verifying internet connectivity...");

    // Attempt to resolve google.com to an IP address
    IPAddress resolvedIP;
    if (WiFi.hostByName("google.com", resolvedIP)) {
        Serial.print("Internet connectivity verified. google.com resolved to: ");
        Serial.println(resolvedIP);
        return true;
    } else {
        Serial.println("Internet connectivity check failed: Could not resolve google.com");
        return false;
    }
}

// Deletes the saved WiFi credentials file from LittleFS
// This forces the device to launch the captive portal on next connection attempt
void forgetCredentials() {
    if (LittleFS.exists(CONFIG_FILE)) {
        if (LittleFS.remove(CONFIG_FILE)) {
            Serial.println("Saved WiFi credentials have been deleted");
        } else {
            Serial.println("Warning: Failed to delete credentials file");
        }
    } else {
        Serial.println("No credentials file to delete");
    }
}

// Attempts to connect to WiFi with the given credentials
bool connectToWiFi(String ssid, String password) {
    Serial.println("Attempting to connect to WiFi...");
    Serial.println("SSID: " + ssid);

    WiFi.mode(WIFI_STA);
    waitForWiFiMode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < wifiConnectionTimeout) {
        Serial.print(".");
        delay(200);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println("Failed to connect to WiFi");
        return false;
    }
}

// Scans for available WiFi networks and returns a JSON array of SSIDs
String createWifiJson() {
    int n = WiFi.scanNetworks();
    String ssids[n];
    int count = 0;

    for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0 || ssid == ap_ssid || ssid.length() > 32) {
            continue;
        }
        bool duplicate = false;
        for (int j = 0; j < count; ++j) {
            if (ssids[j] == ssid) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            ssids[count++] = ssid;
        }
    }

    String list = "[";
    for (int i = 0; i < count; ++i) {
        if (i) list += ",";
        list += "\"" + ssids[i] + "\"";
    }
    list += "]";
    return list;
}

// Main loop for the Access Point (AP) mode
// Takes a reference to the CaptivePortal object
void apLoop(CaptivePortal &portal) {
    while (!shouldStopAP) {
        portal.processDNS();
        delay(50);
    }
    delay(2000);
    if (portal.stopAP()) {
        shouldStopAP = false;
        Serial.println("Captive Portal stopped.");
    }
}

// Checks if the provided SSID and password are valid
bool paramCheck(String ssid, String password) {
    if (ssid.length() < 1 || ssid.length() > 32) {
        return false;
    }
    if (password.length() < 8 || password.length() > 63) {
        return false;
    }
    return true;
}

// Sets up the ESP8266 as an open WiFi Access Point with a captive portal
// Takes a reference to the CaptivePortal object
void apSetup(CaptivePortal &portal) {
    WiFi.mode(WIFI_AP);
    waitForWiFiMode(WIFI_AP);

    wifiList = createWifiJson();

    portal.initializeOpen(ap_ssid, "index.html");

    portal.getServer().on("/api/setupWiFi", HTTP_POST, [&](AsyncWebServerRequest *request) {
        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            receivedSSID = request->getParam("ssid", true)->value();
            receivedPassword = request->getParam("password", true)->value();

            if (paramCheck(receivedSSID, receivedPassword)) {
                saveCredentials(receivedSSID, receivedPassword);
                request->send(200, "text/plain", "WiFi credentials received and saved. This Portal will close now.");
                shouldStopAP = true;
            } else {
                request->send(400, "text/plain", "Invalid parameters");
            }
        } else {
            request->send(400, "text/plain", "Missing parameters");
        }
    });

    portal.getServer().on("/api/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", wifiList);
    });

    portal.startAP();
    Serial.println("Captive Portal started. Waiting for WiFi credentials...");
    apLoop(portal);
}

// Public API functions

void wifiSetupInit() {
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount LittleFS filesystem");
        Serial.println("Formatting LittleFS...");
        if (LittleFS.format()) {
            Serial.println("LittleFS formatted successfully");
            if (!LittleFS.begin()) {
                Serial.println("Failed to mount LittleFS after formatting");
                return;
            }
        } else {
            Serial.println("Failed to format LittleFS");
            return;
        }
    }
    Serial.println("LittleFS mounted successfully");
}

bool wifiSetupConnect() {
    String savedSSID, savedPassword;

    // Attempt to load and use saved credentials
    if (loadCredentials(savedSSID, savedPassword)) {
        Serial.println("Found saved WiFi credentials");

        // Try to connect to the saved WiFi network
        if (connectToWiFi(savedSSID, savedPassword)) {
            // WiFi connection established, now verify internet connectivity
            // This catches cases where WiFi connects but router configuration has changed
            // preventing actual internet access
            if (verifyInternetConnectivity()) {
                // Both WiFi and internet are working - success!
                wifiConnected = true;
                return true;
            } else {
                // WiFi connected but no internet access detected
                // This likely means router configuration changed
                Serial.println("WiFi connected but no internet access detected");
                Serial.println("Router configuration may have changed");
                Serial.println("Forgetting saved credentials and launching captive portal...");

                // Disconnect from WiFi before forgetting credentials
                WiFi.disconnect(true);
                delay(1000);

                // Delete the saved credentials so we don't get stuck in this state
                forgetCredentials();
            }
        } else {
            // Failed to connect with saved credentials
            Serial.println("Saved credentials failed to connect, starting captive portal...");
        }
    } else {
        Serial.println("No saved credentials found");
    }

    // Launch captive portal for new WiFi setup
    // This happens if: no saved credentials, connection failed, or internet check failed
    Serial.println("Starting captive portal for WiFi setup...");

    // Create CaptivePortal locally to avoid port conflicts with user program's web server
    // The portal and its internal AsyncWebServer will be destroyed when this function exits
    CaptivePortal portal;
    apSetup(portal);

    Serial.println("Credentials received from portal:");
    Serial.println("SSID: " + receivedSSID);

    // Wait for captive portal to fully shut down and release resources
    // This ensures the web server on port 80 is fully stopped before we try to connect
    Serial.println("Waiting for captive portal to fully shut down...");
    delay(2000);

    // Attempt to connect with the newly provided credentials
    if (connectToWiFi(receivedSSID, receivedPassword)) {
        // Verify internet connectivity for new credentials as well
        if (verifyInternetConnectivity()) {
            wifiConnected = true;
            return true;
        } else {
            Serial.println("Connected to WiFi but no internet access detected");
            Serial.println("Please check your router's internet connection");
            Serial.println("Device will restart and try again...");
            delay(5000);
            ESP.restart();
            return false;
        }
    } else {
        Serial.println("Failed to connect with provided credentials");
        Serial.println("Device will restart and try captive portal again...");
        delay(3000);
        ESP.restart();
        return false;
    }
}

bool isWifiConnected() {
    return wifiConnected && (WiFi.status() == WL_CONNECTED);
}
