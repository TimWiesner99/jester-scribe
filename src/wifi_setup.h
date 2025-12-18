#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include <Arduino.h>

// Initialize WiFi setup system
void wifiSetupInit();

// Attempt to connect to WiFi, returns true if successful
// If no saved credentials or connection fails, starts captive portal
bool wifiSetupConnect();

// Check if WiFi is currently connected
bool isWifiConnected();

// Verify internet connectivity by attempting to resolve google.com via DNS
bool verifyInternetConnectivity();

#endif
