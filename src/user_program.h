#ifndef USER_PROGRAM_H
#define USER_PROGRAM_H

#include <Arduino.h>

// Initialize your main program
void userProgramSetup();

// Main program loop
void userProgramLoop();

// Thermal printer functions
void initializePrinter();
void printReceipt();
void printServerInfo();
void setInverse(bool enable);
void printLine(String line);
void advancePaper(int lines);
void printWrapped(String text);

// Time utilities
String getFormattedDateTime();
String formatCustomDate(String customDate);

// Debug logging
void debugLog(String message);

#endif
