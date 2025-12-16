#include "main_program.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

// === JOKE SOURCE ===
const String JOKE_SOURCE = "https://www.hahaha.de/witze/witzdestages.txt";

// === Time Configuration ===
// Germany: UTC+1 (CET - Central European Time) = 3600 seconds
// During daylight saving time (late March to late October): UTC+2 (CEST) = 7200 seconds
const long utcOffsetInSeconds = 3600; // German standard time (UTC+1)
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

// === Joke Request Structure ===
struct JokeRequest {
  bool shouldFetchAndPrint;  // Flag: should we fetch a joke and print it?
  bool readyToPrint;         // Flag: joke downloaded to filesystem, ready to print
  bool isScheduled;          // Flag: true if auto-scheduled, false if manual
};

JokeRequest currentJoke = {false, false, false};

// === Schedule State ===
struct ScheduleState {
  String dailyPrintTime;        // e.g., "09:00"
  String lastJokePrintDate;     // e.g., "2025-12-16"
  unsigned long lastCheckMillis; // Throttle checks to once per minute
};

ScheduleState scheduleState = {"09:00", "", 0};

// Joke cache file path
const char* JOKE_CACHE_FILE = "/joke_cache.txt";

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
  String dayNames[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  String monthNames[] = {"Januar", "Februar", "Maerz", "April", "Mai", "Juni",
                        "Juli", "August", "September", "Okotber", "November", "Dezember"};

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

  String dayNames[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  String monthNames[] = {"Januar", "Februar", "Maerz", "April", "Mai", "Juni",
                        "Juli", "August", "September", "Okotber", "November", "Dezember"};

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
  formatted += String(day) + ". ";
  formatted += monthNames[month - 1] + " ";
  formatted += String(year);

  return formatted;
}

// Get current date in YYYY-MM-DD format
String getCurrentDate() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  time_t rawTime = epochTime;
  struct tm * timeInfo = gmtime(&rawTime);

  char buffer[11]; // "YYYY-MM-DD\0"
  sprintf(buffer, "%04d-%02d-%02d",
          timeInfo->tm_year + 1900,
          timeInfo->tm_mon + 1,
          timeInfo->tm_mday);
  return String(buffer);
}

// Get current time in HH:MM format
String getCurrentTime() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  time_t rawTime = epochTime;
  struct tm * timeInfo = gmtime(&rawTime);

  char buffer[6]; // "HH:MM\0"
  sprintf(buffer, "%02d:%02d",
          timeInfo->tm_hour,
          timeInfo->tm_min);
  return String(buffer);
}

// === Schedule Configuration Functions ===
// Load schedule settings from config.json
bool loadScheduleConfig(String &dailyPrintTime, String &lastJokePrintDate) {
  if (!LittleFS.exists("/config.json")) {
    debugLog("Config file does not exist, using defaults");
    dailyPrintTime = "09:00";
    lastJokePrintDate = "";
    return false;
  }

  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    debugLog("Failed to open config file for reading");
    dailyPrintTime = "09:00";
    lastJokePrintDate = "";
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    debugLog("Failed to parse config file");
    dailyPrintTime = "09:00";
    lastJokePrintDate = "";
    return false;
  }

  // Load with defaults if fields don't exist
  dailyPrintTime = doc["dailyPrintTime"] | "09:00";
  lastJokePrintDate = doc["lastJokePrintDate"] | "";

  return true;
}

// Save complete schedule settings
bool saveScheduleConfig(String dailyPrintTime, String lastJokePrintDate) {
  // Load existing config to preserve WiFi credentials
  JsonDocument doc;

  if (LittleFS.exists("/config.json")) {
    File configFile = LittleFS.open("/config.json", "r");
    if (configFile) {
      deserializeJson(doc, configFile);
      configFile.close();
    }
  }

  // Update schedule fields
  doc["dailyPrintTime"] = dailyPrintTime;
  doc["lastJokePrintDate"] = lastJokePrintDate;

  // Save back to file
  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    debugLog("Failed to open config file for writing");
    return false;
  }

  if (serializeJson(doc, configFile) == 0) {
    debugLog("Failed to write to config file");
    configFile.close();
    return false;
  }

  configFile.close();
  debugLog("Schedule config saved");
  return true;
}

// Update only last print date (optimized for daily writes)
bool updateLastPrintDate(String date) {
  // Load existing config
  JsonDocument doc;

  if (LittleFS.exists("/config.json")) {
    File configFile = LittleFS.open("/config.json", "r");
    if (configFile) {
      deserializeJson(doc, configFile);
      configFile.close();
    }
  }

  // Update only lastJokePrintDate field
  doc["lastJokePrintDate"] = date;

  // Save back to file
  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    debugLog("Failed to open config file for writing");
    return false;
  }

  if (serializeJson(doc, configFile) == 0) {
    debugLog("Failed to write to config file");
    configFile.close();
    return false;
  }

  configFile.close();
  return true;
}

// === Scheduler Functions ===
// Check if we should print scheduled joke
bool shouldPrintScheduledJoke() {
  // Throttle: only check once per 60 seconds
  if (millis() - scheduleState.lastCheckMillis < 60000) {
    return false;
  }
  scheduleState.lastCheckMillis = millis();

  String currentDate = getCurrentDate();
  String currentTime = getCurrentTime();

  // Already printed today?
  if (scheduleState.lastJokePrintDate == currentDate) {
    return false;
  }

  // Past scheduled time?
  if (currentTime >= scheduleState.dailyPrintTime) {
    return true;
  }

  return false;
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
  delay(1500);

  // Print header first (normal orientation)
  setInverse(true);
  printLine(currentReceipt.timestamp);
  setInverse(false);

  // Small delay between header and message
  delay(500);

  // Print wrapped message
  printWrapped(currentReceipt.message);

  // Advance paper
  advancePaper(2);

  debugLog("Receipt printed successfully");
}

// Helper function to decode HTML entities (German characters)
String decodeHTMLEntities(String text) {
  // Common German HTML entities
  text.replace("&quot;", "\"");
  text.replace("&amp;", "&");
  text.replace("&lt;", "<");
  text.replace("&gt;", ">");
  text.replace("&ouml;", "oe");
  text.replace("&auml;", "ae");
  text.replace("&uuml;", "ue");
  text.replace("&Ouml;", "Oe");
  text.replace("&Auml;", "Ae");
  text.replace("&Uuml;", "Ue");
  text.replace("&szlig;", "ss");
  text.replace("&nbsp;", " ");

  return text;
}

// Helper function to remove HTML tags
String stripHTMLTags(String text) {
  String result = "";
  bool inTag = false;

  for (int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);

    if (c == '<') {
      inTag = true;
      // Check if it's a <br> tag and convert to space
      // Check bounds before substring
      if (i + 4 <= text.length()) {
        String tag = text.substring(i, i + 4);
        if (tag.equals("<br>")) {
          result += " ";
        }
      }
      if (i + 5 <= text.length()) {
        String tag = text.substring(i, i + 5);
        if (tag.equals("<br/>")) {
          result += " ";
        }
      }
      // Also check for <br /> with space
      if (i + 6 <= text.length()) {
        String tag = text.substring(i, i + 6);
        if (tag.equals("<br />")) {
          result += " ";
        }
      }
    } else if (c == '>') {
      inTag = false;
    } else if (!inTag) {
      result += c;
    }
  }

  return result;
}

// Function to fetch joke from server and save to filesystem
// Returns true if successful, false if failed
// This runs in main loop where blocking HTTP requests are safe
bool fetchJokeFromAPI() {
  debugLog("Fetching joke from server...");

  String jokeURL = JOKE_SOURCE;

  // Use WiFiClientSecure for HTTPS connections
  WiFiClientSecure client;
  HTTPClient http;

  // Skip certificate validation (insecure but necessary for simple ESP8266 HTTPS)
  client.setInsecure();

  // Reduce buffer sizes to save memory (default is 16KB each!)
  // Current jokes are ~1KB, but allow room for variation (shorter or longer jokes)
  // Using 2KB buffers (1KB RX + 1KB TX) instead of default 32KB (16KB each)
  client.setBufferSizes(1024, 1024);  // RX and TX buffers: 1KB each (still saves 30KB!)

  // Allow some time for any pending operations to complete and memory to be freed
  delay(100);
  yield();

  // Begin HTTP connection
  debugLog("Connecting to: " + jokeURL);
  bool beginResult = http.begin(client, jokeURL);

  if (!beginResult) {
    debugLog("ERROR: http.begin() failed!");
    http.end();
    return false;
  }
  debugLog("HTTP connection initialized");

  // Set timeout (10 seconds for HTTPS - can be slower)
  http.setTimeout(10000);

  // Add headers that the server expects (some servers reject requests without these)
  http.addHeader("User-Agent", "Mozilla/5.0 (ESP8266)");
  http.addHeader("Accept", "text/plain, text/html, */*");
  http.addHeader("Connection", "close");

  // Make GET request
  int httpCode = http.GET();
  debugLog("HTTP response code: " + String(httpCode));

  bool success = false;

  if (httpCode == HTTP_CODE_OK) {  // 200
    debugLog("HTTP 200 OK - Streaming to file...");

    // Check content length
    int contentLength = http.getSize();
    if (contentLength > 0) {
      debugLog("Content size: " + String(contentLength) + " bytes");
    } else {
      debugLog("Content size: Unknown (chunked transfer)");
    }

    // Open file for writing (this will overwrite any existing file)
    File file = LittleFS.open(JOKE_CACHE_FILE, "w");
    if (!file) {
      debugLog("ERROR: Failed to open file for writing!");
      http.end();
      return false;
    }

    // Stream the response directly to file in chunks (saves memory!)
    WiFiClient* stream = http.getStreamPtr();
    int bytesWritten = 0;
    uint8_t buffer[128]; // Small buffer - only 128 bytes in RAM at a time
    const int MAX_FILE_SIZE = 5000; // Safety limit: max 5KB (jokes should be < 2KB)

    while (http.connected() && (contentLength > 0 || contentLength == -1)) {
      // Safety check: prevent downloading huge files
      if (bytesWritten >= MAX_FILE_SIZE) {
        debugLog("WARNING: File size exceeded " + String(MAX_FILE_SIZE) + " bytes, stopping download");
        break;
      }
      size_t availableSize = stream->available();

      if (availableSize) {
        // Read chunk into buffer
        int bytesToRead = ((availableSize > sizeof(buffer)) ? sizeof(buffer) : availableSize);
        int bytesRead = stream->readBytes(buffer, bytesToRead);

        // Write chunk to file
        file.write(buffer, bytesRead);
        bytesWritten += bytesRead;

        if (contentLength > 0) {
          contentLength -= bytesRead;
        }

        // Yield to allow ESP8266 to handle background tasks
        yield();
      }
      delay(1);
    }

    file.close();
    debugLog("Downloaded " + String(bytesWritten) + " bytes to file");

    success = true;

  } else if (httpCode > 0) {
    // HTTP error code
    debugLog("HTTP request failed with code: " + String(httpCode));
  } else {
    // Connection failed
    debugLog("HTTP connection failed: " + http.errorToString(httpCode));
  }

  // Close HTTP connection to free memory ASAP
  http.end();

  return success;
}

// Function to process cached joke file and extract clean text
// Returns the joke text, or error message if processing failed
String processJokeFromFile() {
  debugLog("Processing joke from file...");

  // Read the entire file into a String
  // File is small (~1000 bytes) so this is safe after closing HTTP connection
  File file = LittleFS.open(JOKE_CACHE_FILE, "r");
  if (!file) {
    debugLog("ERROR: Failed to open joke cache file for reading!");
    return "Error: Could not read joke cache";
  }

  String htmlContent = file.readString();
  file.close();

  debugLog("Read " + String(htmlContent.length()) + " chars from file");

  String joke = "";

  // Extract content from <div id="witzdestages">
  int divStart = htmlContent.indexOf("<div id=\"witzdestages\">");
  if (divStart == -1) {
    divStart = htmlContent.indexOf("<div id='witzdestages'>");
  }

  if (divStart != -1) {
    // Find the closing </div>
    int divEnd = htmlContent.indexOf("</div>", divStart);

    if (divEnd != -1) {
      // Extract content between tags
      joke = htmlContent.substring(divStart, divEnd);

      // Remove the opening div tag
      int contentStart = joke.indexOf(">") + 1;
      joke = joke.substring(contentStart);

      // Remove the footer link span (if present)
      int linkStart = joke.indexOf("<span id=\"witzdestageslink\">");
      if (linkStart == -1) {
        linkStart = joke.indexOf("<span id='witzdestageslink'>");
      }
      if (linkStart != -1) {
        joke = joke.substring(0, linkStart);
      }

      // Decode HTML entities (ö, ä, ü, ß, etc.)
      joke = decodeHTMLEntities(joke);

      // Remove remaining HTML tags (<br />, etc.)
      joke = stripHTMLTags(joke);

      // Clean up whitespace
      joke.trim();

      // Replace multiple spaces with single space
      while (joke.indexOf("  ") != -1) {
        joke.replace("  ", " ");
      }

      debugLog("Final joke: " + String(joke.length()) + " chars");

      if (joke.length() == 0) {
        return "Error: Joke extraction resulted in empty text";
      }

    } else {
      return "Error: Could not find closing div tag";
    }
  } else {
    return "Error: Could not find witzdestages div";
  }

  return joke;
}

// Function for printing jokes
void printDailyJoke(String jokeText) {
  debugLog("Printing joke...");

  advancePaper(2);

  // Small delay to ensure printer is ready for new job
  delay(500);

  // Print header
  setInverse(true);
  printLine(getFormattedDateTime());
  setInverse(false);

  delay(1000);

  // Print the joke text
  printWrapped(jokeText);

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

  // Additional 5s delay before first print job
  delay(5000);

  //setInverse(true);
  printLine("PRINTER SERVER READY");
  //setInverse(false);

  // Delay between sections
  delay(500);

  String serverInfo = "Server started at " + WiFi.localIP().toString();
  printWrapped(serverInfo);

  // Print schedule information
  delay(500);
  printWrapped("Daily print: " + scheduleState.dailyPrintTime);
  if (scheduleState.lastJokePrintDate.length() > 0) {
    printWrapped("Last printed: " + scheduleState.lastJokePrintDate);
  } else {
    printWrapped("Last printed: Never");
  }

  advancePaper(3);
}

// === Printer Helper Functions ===
void setInverse(bool enable) {
  printer.write(0x1D); printer.write('B');
  printer.write(enable ? 1 : 0); // GS B n
  delay(100); // Small delay after mode change
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
  request->send(404, "text/plain", "Page not found");
}

// Handler for printing daily joke
void handlePrintJoke(AsyncWebServerRequest *request) {
  debugLog("Joke print requested via web interface");

  // Queue the joke for fetching and printing in the main loop (async-safe)
  currentJoke.shouldFetchAndPrint = true;

  request->send(200, "text/plain", "Joke will be fetched and printed!");
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

  // Initialize printer
  initializePrinter();

  // Load schedule configuration
  loadScheduleConfig(scheduleState.dailyPrintTime, scheduleState.lastJokePrintDate);
  debugLog("Schedule loaded: time=" + scheduleState.dailyPrintTime +
           ", lastPrint=" + scheduleState.lastJokePrintDate);

  // Initialize time client
  timeClient.begin();
  debugLog("Time client initialized");

  // Setup web server routes
  // Serve static files from LittleFS using serveStatic (more efficient)
  server.serveStatic("/", LittleFS, "/").setDefaultFile("main.html");

  server.on("/submit", HTTP_POST, handleSubmit);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/printJoke", HTTP_POST, handlePrintJoke);
  server.on("/wifiInfo", HTTP_GET, handleWifiInfo);
  server.on("/forgetWifi", HTTP_POST, handleForgetWifi);

  // Schedule API endpoints
  server.on("/api/schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"dailyPrintTime\":\"" + scheduleState.dailyPrintTime + "\",";
    json += "\"lastJokePrintDate\":\"" + scheduleState.lastJokePrintDate + "\"";
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/api/schedule", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("dailyPrintTime", true)) {
      String newTime = request->getParam("dailyPrintTime", true)->value();

      // Validate HH:MM format
      if (newTime.length() == 5 && newTime.charAt(2) == ':') {
        scheduleState.dailyPrintTime = newTime;
        saveScheduleConfig(scheduleState.dailyPrintTime, scheduleState.lastJokePrintDate);
        debugLog("Schedule time updated to: " + newTime);
        request->send(200, "application/json", "{\"success\":true}");
      } else {
        request->send(400, "text/plain", "Invalid time format (use HH:MM)");
      }
    } else {
      request->send(400, "text/plain", "Missing dailyPrintTime parameter");
    }
  });

  server.on("/api/lastPrint", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"lastJokePrintDate\":\"" + scheduleState.lastJokePrintDate + "\"";
    json += "}";
    request->send(200, "application/json", json);
  });

  server.onNotFound(handle404);

  debugLog("Starting web server...");
  server.begin();
  debugLog("Web server started on port 80");

  // Wait a bit longer before printing to ensure printer is fully ready
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

  // Check scheduled printing (before manual joke check)
  if (shouldPrintScheduledJoke()) {
    debugLog("Scheduled joke print triggered at " + getCurrentTime());
    currentJoke.shouldFetchAndPrint = true;
    currentJoke.isScheduled = true;
  }

  // Check if a joke print was requested
  if (currentJoke.shouldFetchAndPrint) {
    // Step 1: Fetch joke from API and save to filesystem
    debugLog("Starting joke fetch...");
    bool fetchSuccess = fetchJokeFromAPI();

    if (fetchSuccess) {
      // HTTP connection is now closed, memory freed
      currentJoke.shouldFetchAndPrint = false;
      currentJoke.readyToPrint = true;
    } else {
      debugLog("ERROR: Failed to fetch joke");
      printDailyJoke("Error: Could not fetch joke from server");
      currentJoke.shouldFetchAndPrint = false;
      currentJoke.readyToPrint = false;
    }
  }

  // Check if joke is ready to process and print
  if (currentJoke.readyToPrint) {
    // Step 2: Process joke from file (parse HTML, decode entities, etc.)
    debugLog("Processing joke from cache...");
    String jokeText = processJokeFromFile();

    // Step 3: Print the joke
    printDailyJoke(jokeText);

    // Track scheduled prints (update date only for automatic prints)
    if (currentJoke.isScheduled) {
      String currentDate = getCurrentDate();
      updateLastPrintDate(currentDate);
      scheduleState.lastJokePrintDate = currentDate;
      debugLog("Updated lastJokePrintDate: " + currentDate);
      currentJoke.isScheduled = false;
    }

    // Reset flag
    currentJoke.readyToPrint = false;
  }

  delay(10); // Small delay to prevent excessive CPU usage
}
