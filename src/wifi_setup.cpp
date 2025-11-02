#include "wifi_setup.h"
#include <CaptivePortal.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

// Configuration
const char* ap_ssid = "ESP8266-WiFi-Setup";
const char* ap_password = "12345678";
const char* CONFIG_FILE = "/config.json";
const int wifiConnectionTimeout = 10000;

// Internal state
CaptivePortal portal;
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
void apLoop() {
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
void apSetup() {
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
    apLoop();
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
    if (loadCredentials(savedSSID, savedPassword)) {
        Serial.println("Found saved WiFi credentials");
        if (connectToWiFi(savedSSID, savedPassword)) {
            wifiConnected = true;
            return true;
        } else {
            Serial.println("Saved credentials failed, starting captive portal...");
        }
    } else {
        Serial.println("No saved credentials found");
    }

    Serial.println("Starting captive portal for WiFi setup...");
    apSetup();

    Serial.println("Credentials received from portal:");
    Serial.println("SSID: " + receivedSSID);

    if (connectToWiFi(receivedSSID, receivedPassword)) {
        wifiConnected = true;
        return true;
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
