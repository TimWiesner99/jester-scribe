// FIRMWARE V2 - Without rotation (normal orientation printing)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <SoftwareSerial.h>

// === Function Declarations ===
void connectToWiFi();
void setupWebServer();
void handleRoot();
void handleSubmit();
void handleLogs();
void handle404();
String getFormattedDateTime();
String formatCustomDate(String customDate);
void initializePrinter();
void printReceipt();
void printServerInfo();
void setInverse(bool enable);
void printLine(String line);
void advancePaper(int lines);
void printWrapped(String text);

// === WiFi Configuration ===
const char* ssid = "CellSignalling";
const char* password = "PieterPostverdientdekost";

// === Time Configuration ===
const long utcOffsetInSeconds = 0; // UTC offset in seconds (0 for UTC, 3600 for UTC+1, etc.)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, 60000);

// === Web Server ===
ESP8266WebServer server(80);

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

void setup() {
  Serial.begin(115200);
  debugLog("\n=== Thermal Printer Server Starting ===");

  // Initialize printer
  initializePrinter();

  // Connect to WiFi
  connectToWiFi();

  // Initialize time client
  timeClient.begin();
  debugLog("Time client initialized");

  // Setup web server routes
  setupWebServer();

  // Start the server
  server.begin();
  debugLog("Web server started");

  // Wait a bit longer before printing to ensure printer is fully ready
  debugLog("Waiting for printer to stabilize before printing startup info...");
  delay(2000); // Additional 2 second delay before first print

  // Print server info
  printServerInfo();

  debugLog("=== Setup Complete ===");
}

void loop() {
  // Handle web server requests
  server.handleClient();

  // Update time client
  timeClient.update();

  // Check if we have a new receipt to print
  if (currentReceipt.hasData) {
    printReceipt();
    currentReceipt.hasData = false; // Reset flag
  }

  delay(10); // Small delay to prevent excessive CPU usage
}

// === WiFi Connection ===
void connectToWiFi() {
  debugLog("Connecting to WiFi: " + String(ssid));

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    debugLog(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    debugLog("WiFi connected successfully!");
    debugLog("IP address: " + WiFi.localIP().toString());
  } else {
    debugLog("Failed to connect to WiFi");
  }
}

// === Web Server Setup ===
void setupWebServer() {
  // Serve the main page
  server.on("/", HTTP_GET, handleRoot);

  // Handle form submission
  server.on("/submit", HTTP_POST, handleSubmit);

  // Handle submission via URL
  server.on("/submit", HTTP_GET, handleSubmit);

  // Serve debug logs
  server.on("/logs", HTTP_GET, handleLogs);

  // Handle 404
  server.onNotFound(handle404);
}

// === Web Server Handlers ===
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en" class="bg-gray-50 text-gray-900">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Life Receipt</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <script src="https://cdn.jsdelivr.net/npm/canvas-confetti@1.6.0/dist/confetti.browser.min.js"></script>
  <script defer>
    function handleInput(el) {
      const counter = document.getElementById('char-counter');
      const remaining = 200 - el.value.length;
      counter.textContent = `${remaining} characters left`;
      counter.classList.toggle('text-red-500', remaining <= 20);
    }
    function handleSubmit(e) {
      e.preventDefault();
      const formData = new FormData(e.target);
      fetch('/submit', {
        method: 'POST',
        body: formData
      }).then(() => {
        const form = document.getElementById('receipt-form');
        const message = document.getElementById('thank-you');
        form.classList.add('hidden');
        message.classList.remove('hidden');
        confetti({
          particleCount: 100,
          spread: 70,
          origin: { y: 0.6 },
        });
      });
    }
    function handleKeyPress(e) {
      if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        document.getElementById('receipt-form').dispatchEvent(new Event('submit'));
      }
    }
    function toggleDebug() {
      const console = document.getElementById('debug-console');
      const btn = document.getElementById('debug-toggle');
      console.classList.toggle('hidden');
      btn.textContent = console.classList.contains('hidden') ? '🔧 Debug' : '🔧 Hide';
    }
    async function updateLogs() {
      try {
        const response = await fetch('/logs');
        const logs = await response.text();
        const console = document.getElementById('debug-output');
        console.textContent = logs || 'No logs yet...';
        console.scrollTop = console.scrollHeight;
      } catch (e) {
        console.error('Failed to fetch logs:', e);
      }
    }
    setInterval(updateLogs, 1000);
    setTimeout(updateLogs, 100);
  </script>
</head>
<body class="flex flex-col min-h-screen justify-between items-center py-12 px-4 font-sans">
  <main class="w-full max-w-md text-center">
    <h1 class="text-3xl font-semibold mb-10 text-gray-900 tracking-tight">Life Receipt:</h1>
    <form id="receipt-form" onsubmit="handleSubmit(event)" action="/submit" method="post" class="bg-white shadow-2xl rounded-3xl p-8 space-y-6 border border-gray-100">
      <textarea
        name="message"
        maxlength="200"
        oninput="handleInput(this)"
        onkeypress="handleKeyPress(event)"
        placeholder="Type your receipt…"
        class="w-full p-4 rounded-xl border border-gray-200 focus:outline-none focus:ring-2 focus:ring-gray-400 focus:border-transparent resize-none text-gray-800 placeholder-gray-400"
        rows="4"
        required
        autofocus
      ></textarea>
      <div id="char-counter" class="text-sm text-gray-500 text-right">200 characters left</div>
      <button type="submit" class="w-full bg-gray-900 hover:bg-gray-800 text-white py-3 rounded-xl font-medium transition-all duration-200 hover:scale-[1.02] hover:shadow-lg">
        Send
      </button>
    </form>
    <div id="thank-you" class="hidden text-gray-700 font-semibold text-xl mt-8 animate-fade-in">
      🎉 Receipt submitted. You did it!
    </div>

    <!-- Debug Console -->
    <div class="w-full mt-8">
      <button id="debug-toggle" onclick="toggleDebug()" class="w-full bg-gray-100 hover:bg-gray-200 text-gray-600 py-2 px-4 rounded-lg font-medium text-sm transition-all duration-200">
        🔧 Debug
      </button>
      <div id="debug-console" class="hidden mt-4 bg-black text-green-400 p-4 rounded-lg font-mono text-xs max-h-96 overflow-y-auto">
        <div class="mb-2 text-gray-400 border-b border-gray-700 pb-2">System Logs (auto-refresh)</div>
        <pre id="debug-output" class="whitespace-pre-wrap">Loading...</pre>
      </div>
    </div>
  </main>
  <footer class="text-sm text-gray-400 mt-16">
    Designed with love by <a href="https://urbancircles.club" target="_blank" class="text-gray-500 hover:text-gray-700 transition-colors duration-200 underline decoration-gray-300 hover:decoration-gray-500 underline-offset-2">Peter / Urban Circles</a>
  </footer>
  <style>
    @keyframes fade-in {
      from { opacity: 0; transform: translateY(8px); }
      to { opacity: 1; transform: translateY(0); }
    }
    .animate-fade-in {
      animation: fade-in 0.6s ease-out forwards;
    }
  </style>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleSubmit() {
  if (server.hasArg("message")) {
    currentReceipt.message = server.arg("message");

    // Check if a custom date was provided
    if (server.hasArg("date")) {
      String customDate = server.arg("date");
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

    server.send(200, "text/plain", "Receipt received and will be printed!");
  } else {
    server.send(400, "text/plain", "Missing message parameter");
  }
}

void handle404() {
  server.send(404, "text/plain", "Page not found");
}

void handleLogs() {
  String logs = "";

  // Build logs string from circular buffer
  int startIdx = (logCount < MAX_LOG_LINES) ? 0 : logIndex;
  for (int i = 0; i < logCount; i++) {
    int idx = (startIdx + i) % MAX_LOG_LINES;
    logs += logBuffer[idx] + "\n";
  }

  server.send(200, "text/plain", logs);
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
