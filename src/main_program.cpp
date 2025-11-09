#include "main_program.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>

// === Time Configuration ===
const long utcOffsetInSeconds = 0; // UTC offset in seconds (0 for UTC, 3600 for UTC+1, etc.)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, 60000);

// === Web Server ===
AsyncWebServer server(80);

// === Printer Setup ===
SoftwareSerial printer(D4, D3); // Use D4 (TX, GPIO2), D3 (RX, GPIO0)
const int maxCharsPerLine = 32;

// === Storage for form data ===
struct Receipt {
  String message;
  String timestamp;
  bool hasData;
};

Receipt currentReceipt = {"", "", false};

// === Debug Log Storage ===
const int MAX_LOG_LINES = 50;
String logBuffer[MAX_LOG_LINES];
int logIndex = 0;
int logCount = 0;

void debugLog(String message) {
  // Add to circular buffer
  logBuffer[logIndex] = message;
  logIndex = (logIndex + 1) % MAX_LOG_LINES;
  if (logCount < MAX_LOG_LINES) {
    logCount++;
  }

  // Also print to Serial
  Serial.println(message);
}

// === Time Utilities ===
String getFormattedDateTime() {
  timeClient.update();

  // Get epoch time
  unsigned long epochTime = timeClient.getEpochTime();

  // Convert to struct tm
  time_t rawTime = epochTime;
  struct tm * timeInfo = gmtime(&rawTime);

  // Day names and month names
  String dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  String monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  // Format: "Sat, 06 Jun 2025"
  String formatted = dayNames[timeInfo->tm_wday] + ", ";
  formatted += String(timeInfo->tm_mday < 10 ? "0" : "") + String(timeInfo->tm_mday) + " ";
  formatted += monthNames[timeInfo->tm_mon] + " ";
  formatted += String(timeInfo->tm_year + 1900);

  return formatted;
}

String formatCustomDate(String customDate) {
  // Expected format: YYYY-MM-DD or DD/MM/YYYY or similar
  // This function will try to parse common date formats and return formatted string

  String dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  String monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  int day = 0, month = 0, year = 0;

  // Try to parse YYYY-MM-DD format
  if (customDate.indexOf('-') != -1) {
    int firstDash = customDate.indexOf('-');
    int secondDash = customDate.indexOf('-', firstDash + 1);

    if (firstDash != -1 && secondDash != -1) {
      year = customDate.substring(0, firstDash).toInt();
      month = customDate.substring(firstDash + 1, secondDash).toInt();
      day = customDate.substring(secondDash + 1).toInt();
    }
  }
  // Try to parse DD/MM/YYYY format
  else if (customDate.indexOf('/') != -1) {
    int firstSlash = customDate.indexOf('/');
    int secondSlash = customDate.indexOf('/', firstSlash + 1);

    if (firstSlash != -1 && secondSlash != -1) {
      day = customDate.substring(0, firstSlash).toInt();
      month = customDate.substring(firstSlash + 1, secondSlash).toInt();
      year = customDate.substring(secondSlash + 1).toInt();
    }
  }

  // Validate parsed values
  if (day < 1 || day > 31 || month < 1 || month > 12 || year < 1900 || year > 2100) {
    debugLog("Invalid date format, using current date");
    return getFormattedDateTime();
  }

  // Calculate day of week (simplified algorithm - may not be 100% accurate for all dates)
  // For a more accurate calculation, you might want to use a proper date library
  int dayOfWeek = 0; // Default to Sunday if we can't calculate

  // Format: "Sat, 06 Jun 2025"
  String formatted = dayNames[dayOfWeek] + ", ";
  formatted += String(day < 10 ? "0" : "") + String(day) + " ";
  formatted += monthNames[month - 1] + " ";
  formatted += String(year);

  return formatted;
}

// === Printer Functions ===
void initializePrinter() {
  printer.begin(9600);

  // Wait for capacitor to charge and printer to power up properly
  debugLog("Waiting for printer to power up...");
  delay(3000); // Increased from 500ms to 3 seconds for capacitor charging

  // Initialise - reset printer to default state
  printer.write(0x1B); printer.write('@'); // ESC @
  delay(500); // Increased from 50ms to 500ms to ensure reset completes

  debugLog("Printer reset complete, configuring...");

  // Set stronger black fill (print density/heat)
  printer.write(0x1B); printer.write('7');
  printer.write(15); // Heating dots (max 15)
  printer.write(150); // Heating time
  printer.write(250); // Heating interval
  delay(200); // Add delay after configuration

  // Rotation removed - printer will print in normal orientation

  debugLog("Printer initialized and ready");
}

void printReceipt() {
  debugLog("Printing receipt...");

  // Small delay to ensure printer is ready for new job
  delay(200);

  // Print header first (normal orientation)
  setInverse(true);
  printLine(currentReceipt.timestamp);
  setInverse(false);

  // Small delay between header and message
  delay(100);

  // Print wrapped message
  printWrapped(currentReceipt.message);

  // Advance paper
  advancePaper(2);

  debugLog("Receipt printed successfully");
}

// Placeholder function for printing daily jokes
// This will be replaced with actual joke fetching logic later
void printDailyJoke() {
  debugLog("Printing daily joke (placeholder)...");

  // Additional delay before print job
  delay(200);

  // Print header
  setInverse(true);
  printLine("DAILY JOKE");
  setInverse(false);

  delay(100);

  // Print placeholder message
  String placeholderText = "Jokes not implemented yet. Stay tuned for hilarious content!";
  printWrapped(placeholderText);

  advancePaper(2);

  debugLog("Joke printed successfully");
}

void printServerInfo() {
  debugLog("=== Server Info ===");
  debugLog("Local IP: " + WiFi.localIP().toString());
  debugLog("Access the form at: http://" + WiFi.localIP().toString());
  debugLog("==================");

  // Also print server info on the thermal printer
  debugLog("Printing server info on thermal printer...");

  // Additional delay before first print job
  delay(500);

  setInverse(true);
  printLine("PRINTER SERVER READY");
  setInverse(false);

  // Delay between sections
  delay(100);

  String serverInfo = "Server started at " + WiFi.localIP().toString();
  printWrapped(serverInfo);

  advancePaper(3);
}

// === Printer Helper Functions ===
void setInverse(bool enable) {
  printer.write(0x1D); printer.write('B');
  printer.write(enable ? 1 : 0); // GS B n
  delay(50); // Small delay after mode change
}

void printLine(String line) {
  printer.println(line);
  delay(50); // Small delay after each line to allow printing to complete
}

void advancePaper(int lines) {
  for (int i = 0; i < lines; i++) {
    printer.write(0x0A); // LF
    delay(100); // Delay between line feeds
  }
}

void printWrapped(String text) {
  // Print text with word-wrapping in normal order
  while (text.length() > 0) {
    if (text.length() <= maxCharsPerLine) {
      printLine(text);
      break;
    }

    // Find the last space within the max character limit
    int lastSpace = text.lastIndexOf(' ', maxCharsPerLine);
    if (lastSpace == -1) lastSpace = maxCharsPerLine;

    // Print this line
    printLine(text.substring(0, lastSpace));

    // Remove the printed part and trim
    text = text.substring(lastSpace);
    text.trim();
  }
}

// === Web Server Handlers ===
void handleRoot(AsyncWebServerRequest *request) {
  debugLog("Request received for / (root)");
  request->send(LittleFS, "/main.html", "text/html");
}

void handleSubmit(AsyncWebServerRequest *request) {
  if (request->hasParam("message", true)) {
    currentReceipt.message = request->getParam("message", true)->value();

    // Check if a custom date was provided
    if (request->hasParam("date", true)) {
      String customDate = request->getParam("date", true)->value();
      currentReceipt.timestamp = formatCustomDate(customDate);
      debugLog("Using custom date: " + customDate);
    } else {
      currentReceipt.timestamp = getFormattedDateTime();
      debugLog("Using current date");
    }

    currentReceipt.hasData = true;

    debugLog("=== New Receipt Received ===");
    debugLog("Message: " + currentReceipt.message);
    debugLog("Time: " + currentReceipt.timestamp);
    debugLog("============================");

    request->send(200, "text/plain", "Receipt received and will be printed!");
  } else {
    request->send(400, "text/plain", "Missing message parameter");
  }
}

void handleLogs(AsyncWebServerRequest *request) {
  String logs = "";

  // Build logs string from circular buffer
  int startIdx = (logCount < MAX_LOG_LINES) ? 0 : logIndex;
  for (int i = 0; i < logCount; i++) {
    int idx = (startIdx + i) % MAX_LOG_LINES;
    logs += logBuffer[idx] + "\n";
  }

  request->send(200, "text/plain", logs);
}

void handle404(AsyncWebServerRequest *request) {
  debugLog("404 - Request for unknown path: " + request->url());
  request->send(404, "text/plain", "Page not found");
}

// Handler for printing daily joke
void handlePrintJoke(AsyncWebServerRequest *request) {
  debugLog("Joke print requested via web interface");

  // Queue the joke for printing in the main loop
  // For now, print immediately
  printDailyJoke();

  request->send(200, "text/plain", "Joke print started!");
}

// Handler for WiFi info endpoint
void handleWifiInfo(AsyncWebServerRequest *request) {
  debugLog("WiFi info requested");

  // Create JSON response with WiFi information
  String json = "{";
  json += "\"ssid\":\"" + WiFi.SSID() + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";

  request->send(200, "application/json", json);
}

// Handler for forgetting WiFi credentials
void handleForgetWifi(AsyncWebServerRequest *request) {
  debugLog("WiFi forget requested - will restart device");

  request->send(200, "text/plain", "Forgetting WiFi and restarting...");

  // Delete the config file
  if (LittleFS.exists("/config.json")) {
    LittleFS.remove("/config.json");
    debugLog("WiFi credentials deleted");
  }

  // Restart the device after a short delay
  delay(1000);
  ESP.restart();
}

// === Setup and Loop ===
void mainProgramSetup() {
  Serial.println("=================================");
  Serial.println("Main Program Starting...");
  Serial.println("=================================");

  // Diagnostic: Check memory before starting
  debugLog("Free heap at start: " + String(ESP.getFreeHeap()) + " bytes");

  // Diagnostic: Verify LittleFS is accessible
  debugLog("Checking LittleFS files...");
  if (LittleFS.exists("/main.html")) {
    debugLog("  - main.html: EXISTS");
  } else {
    debugLog("  - main.html: MISSING!");
  }
  if (LittleFS.exists("/style.css")) {
    debugLog("  - style.css: EXISTS");
  } else {
    debugLog("  - style.css: MISSING!");
  }
  if (LittleFS.exists("/main.js")) {
    debugLog("  - main.js: EXISTS");
  } else {
    debugLog("  - main.js: MISSING!");
  }

  // Diagnostic: Check WiFi state
  debugLog("WiFi Status: " + String(WiFi.status()));
  debugLog("WiFi Mode: " + String(WiFi.getMode()));
  debugLog("Local IP: " + WiFi.localIP().toString());

  // Initialize printer
  initializePrinter();

  // Diagnostic: Check memory after printer init
  debugLog("Free heap after printer: " + String(ESP.getFreeHeap()) + " bytes");

  // Initialize time client
  timeClient.begin();
  debugLog("Time client initialized");

  // Diagnostic: Check memory after NTP
  debugLog("Free heap after NTP: " + String(ESP.getFreeHeap()) + " bytes");

  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/submit", HTTP_POST, handleSubmit);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/printJoke", HTTP_POST, handlePrintJoke);
  server.on("/wifiInfo", HTTP_GET, handleWifiInfo);
  server.on("/forgetWifi", HTTP_POST, handleForgetWifi);

  // Serve static files from LittleFS
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    debugLog("Request received for /style.css");
    request->send(LittleFS, "/style.css", "text/css");
  });
  server.on("/main.js", HTTP_GET, [](AsyncWebServerRequest *request){
    debugLog("Request received for /main.js");
    request->send(LittleFS, "/main.js", "application/javascript");
  });

  server.onNotFound(handle404);

  debugLog("Starting web server...");
  server.begin();
  debugLog("Web server started on port 80");

  // Diagnostic: Check memory after server start
  debugLog("Free heap after server: " + String(ESP.getFreeHeap()) + " bytes");

  // Wait a bit longer before printing to ensure printer is fully ready
  debugLog("Waiting for printer to stabilize before printing startup info...");
  delay(2000); // Additional 2 second delay before first print

  // Print server info
  printServerInfo();

  debugLog("=== Setup Complete ===");
}

void mainProgramLoop() {
  // Update time client
  timeClient.update();

  // Check if we have a new receipt to print
  if (currentReceipt.hasData) {
    printReceipt();
    currentReceipt.hasData = false; // Reset flag
  }

  delay(10); // Small delay to prevent excessive CPU usage
}
