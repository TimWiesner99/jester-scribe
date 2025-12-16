#ifndef MAIN_PROGRAM_H
#define MAIN_PROGRAM_H

#include <Arduino.h>

// Initialize your main program
void mainProgramSetup();

// Main program loop
void mainProgramLoop();

// Thermal printer functions
void initializePrinter();
void printReceipt();
void printDailyJoke();
void printServerInfo();
void setInverse(bool enable);
void printLine(String line);
void advancePaper(int lines);
void printWrapped(String text);

// Time utilities
String getFormattedDateTime();
String formatCustomDate(String customDate);
String getCurrentDate();
String getCurrentTime();

// Schedule configuration
bool loadScheduleConfig(String &dailyPrintTime, String &lastJokePrintDate);
bool saveScheduleConfig(String dailyPrintTime, String lastJokePrintDate);
bool updateLastPrintDate(String date);
bool shouldPrintScheduledJoke();

// Debug logging
void debugLog(String message);

#endif
