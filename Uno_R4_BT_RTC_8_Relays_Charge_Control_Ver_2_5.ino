#include <RTC.h>
#include <EEPROM.h>
#include <ArduinoBLE.h>

// Pin Definitions
#define RELAY_COUNT 8
const byte relayPins[RELAY_COUNT] = {2, 3, 4, 5, 6, 7, 8, 9};
const byte EVSE_Present = 10;
const byte CP_Present = 11;
const byte Override = 12;

// EEPROM addresses
#define EEPROM_START_HOUR 0
#define EEPROM_START_MIN 1
#define EEPROM_END_HOUR 2
#define EEPROM_END_MIN 3

// Time settings (24-hour format)
byte chargeStartHour = 2;    // 2 AM
byte chargeStartMinute = 0;
byte chargeEndHour = 8;      // 8 AM
byte chargeEndMinute = 0;

// Timing constants
const unsigned int RELAY_DELAY_MS = 300;
const unsigned long CP_TIMEOUT_MS = 8000;
const unsigned long CP_CHECK_INTERVAL_MS = 250;
const unsigned long STATUS_PRINT_INTERVAL_MS = 5000;
const unsigned long WINDOW_CHECK_INTERVAL_MS = 60000;
const unsigned long TIME_CHECK_INTERVAL_MS = 5000;
const unsigned long DEBOUNCE_DELAY_MS = 100;
const unsigned long BT_CHECK_INTERVAL = 100;

// System state variables
bool isCharging = false;
bool isWaitingForCP = false;
bool overrideActive = false;
bool debugTimeChecks = false;
bool chargeAttempted = false;
unsigned long cpWaitStartTime = 0;
unsigned long lastWindowCheck = 0;
unsigned long lastCpCheck = 0;
unsigned long lastPrintTime = 0;
unsigned long lastTimeCheck = 0;
unsigned long lastBtCheckTime = 0;

// Bluetooth Service and Characteristics
BLEService evseService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLEStringCharacteristic txCharacteristic("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify, 512);
BLEStringCharacteristic rxCharacteristic("19B10002-E8F2-537E-4F6C-D104768A1214", BLEWrite, 512);

void loadChargeWindow() {
  chargeStartHour = EEPROM.read(EEPROM_START_HOUR);
  chargeStartMinute = EEPROM.read(EEPROM_START_MIN);
  chargeEndHour = EEPROM.read(EEPROM_END_HOUR);
  chargeEndMinute = EEPROM.read(EEPROM_END_MIN);
}

void saveChargeWindow() {
  EEPROM.update(EEPROM_START_HOUR, chargeStartHour);
  EEPROM.update(EEPROM_START_MIN, chargeStartMinute);
  EEPROM.update(EEPROM_END_HOUR, chargeEndHour);
  EEPROM.update(EEPROM_END_MIN, chargeEndMinute);
}

void printToAll(String message) {
  Serial.println(message);
  if (BLE.connected()) {
    String btMessage = "\n" + message;
    int chunkSize = 128;
    for (int i = 0; i < message.length(); i += chunkSize) {
      String chunk = message.substring(i, min(i + chunkSize, message.length()));
      txCharacteristic.writeValue(chunk);
      delay(10);
    }
  }
}

void printHelp() {
  String help = "\nEVSE Charge Controller Commands:";
  help += "\nhelp - Show this help";
  help += "\nstatus - Show current status";
  help += "\nsettime YYYY-MM-DD HH:MM:SS - Set RTC time";
  help += "\nsetwindow HH:MM HH:MM - Set charge window";
  help += "\nemergency - Immediate shutdown";
  printToAll(help);
}

void printStatus() {
  RTCTime currentTime;
  RTC.getTime(currentTime);
  
  struct tm tmTime = currentTime.getTmTime();
  
  String status = "\n\nCurrent Time: ";
  // Correct year handling (tm_year is years since 1900)
  status += 1900 + tmTime.tm_year;
  status += '-';
  status += (tmTime.tm_mon + 1) < 10 ? "0" : "";
  status += (tmTime.tm_mon + 1);
  status += '-';
  status += tmTime.tm_mday < 10 ? "0" : "";
  status += tmTime.tm_mday;
  status += ' ';
  status += tmTime.tm_hour < 10 ? "0" : "";
  status += tmTime.tm_hour;
  status += ':';
  status += tmTime.tm_min < 10 ? "0" : "";
  status += tmTime.tm_min;
  status += ':';
  status += tmTime.tm_sec < 10 ? "0" : "";
  status += tmTime.tm_sec;
  
  status += "\nCharge Window: ";
  status += chargeStartHour < 10 ? "0" : "";
  status += chargeStartHour;
  status += ':';
  status += chargeStartMinute < 10 ? "0" : "";
  status += chargeStartMinute;
  status += " to ";
  status += chargeEndHour < 10 ? "0" : "";
  status += chargeEndHour;
  status += ':';
  status += chargeEndMinute < 10 ? "0" : "";
  status += chargeEndMinute;
  
  status += "\nEVSE Present: ";
  status += digitalRead(EVSE_Present) == LOW ? "Yes" : "No";
  status += "\nCP Present: ";
  status += digitalRead(CP_Present) == LOW ? "Yes" : "No";
  status += "\nOverride Active: ";
  status += overrideActive ? "Yes" : "No";
  status += "\nCharging State: ";
  status += isCharging ? "Charging" : isWaitingForCP ? "Waiting for CP" : "Not Charging";
  status += "\n";
  printToAll(status);
}

void handleCommand(String command) {
  command.trim();
  
  if (command.equals("help")) {
    printHelp();
  } 
  else if (command.equals("status")) {
    printStatus();
  } 
  else if (command.startsWith("settime ")) {
    setTime(command.substring(8));
  } 
  else if (command.startsWith("setwindow ")) {
    setChargeWindow(command.substring(10));
  } 
  else if (command.equals("emergency")) {
    emergencyStop();
  } 
  else if (command.length() > 0) {
    printToAll("\nUnknown command. Type 'help' for available commands.");
  }
}

void setTime(String command) {
  int year = command.substring(0, 4).toInt();
  int month = command.substring(5, 7).toInt();
  int day = command.substring(8, 10).toInt();
  int hour = command.substring(11, 13).toInt();
  int minute = command.substring(14, 16).toInt();
  int second = command.substring(17, 19).toInt();

  RTCTime newTime;
  struct tm tm;
  tm.tm_year = year - 1900;  // Years since 1900
  tm.tm_mon = month - 1;     // 0-11
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;
  newTime = RTCTime(tm);
  
  RTC.setTime(newTime);
  printToAll("\nRTC time set successfully");
}

void setChargeWindow(String command) {
  chargeStartHour = command.substring(0, 2).toInt();
  chargeStartMinute = command.substring(3, 5).toInt();
  chargeEndHour = command.substring(6, 8).toInt();
  chargeEndMinute = command.substring(9, 11).toInt();

  saveChargeWindow();
  printToAll("\nCharge window updated and saved");
}

bool shouldCharge() {
  RTCTime currentTime;
  RTC.getTime(currentTime);
  
  struct tm tmTime = currentTime.getTmTime();
  
  unsigned long currentMinutes = tmTime.tm_hour * 60 + tmTime.tm_min;
  unsigned long startMinutes = chargeStartHour * 60 + chargeStartMinute;
  unsigned long endMinutes = chargeEndHour * 60 + chargeEndMinute;

  if (startMinutes < endMinutes) {
    return (currentMinutes >= startMinutes && currentMinutes < endMinutes);
  } 
  else if (startMinutes > endMinutes) {
    return (currentMinutes >= startMinutes || currentMinutes < endMinutes);
  }
  return false;
}

void startCharging() {
  if (isCharging || isWaitingForCP) return;

// Turn on relays in original order (2-9)
  for (byte i = 0; i < RELAY_COUNT; i++) {
    digitalWrite(relayPins[i], LOW);
    delay(RELAY_DELAY_MS);
  }

  isWaitingForCP = true;
  cpWaitStartTime = millis();
  printToAll("\nRelays activated - Waiting for CP signal");
}

void stopCharging() {
  if (!isCharging && !isWaitingForCP) return;

 // Turn off relays in REVERSE order (9-2)
 // for (byte i = 0; i < RELAY_COUNT; i++) {
  for (byte i = RELAY_COUNT; i > 0; i--) {    
    digitalWrite(relayPins[i-1], HIGH);
    delay(RELAY_DELAY_MS);
  }

  isCharging = false;
  isWaitingForCP = false;
  overrideActive = false;
  printToAll("\nRelays deactivated - Charging stopped");
}

void emergencyStop() {
  // Turn off relays in REVERSE order (9-2)
  for (byte i = RELAY_COUNT; i > 0; i--) {
    digitalWrite(relayPins[i-1], HIGH);
  }


  isCharging = false;
  isWaitingForCP = false;
  overrideActive = false;
  printToAll("\nEMERGENCY STOP ACTIVATED");
}

void setupBluetooth() {
  if (!BLE.begin()) {
    Serial.println("\nFailed to initialize BLE!");
    while (1);
  }

  BLE.setLocalName("EVSEController");
  BLE.setAdvertisedService(evseService);

  evseService.addCharacteristic(txCharacteristic);
  evseService.addCharacteristic(rxCharacteristic);

  BLE.addService(evseService);

  txCharacteristic.writeValue("\nEVSE Controller Ready");
  BLE.advertise();
  Serial.println("\nBluetooth LE service started");
}

void checkBluetooth() {
  if (millis() - lastBtCheckTime > BT_CHECK_INTERVAL) {
    lastBtCheckTime = millis();
    
    BLEDevice central = BLE.central();
    
    if (central) {
      if (!central.connected()) {
        Serial.print("\nDisconnected from central: ");
        Serial.println(central.address());
        BLE.advertise();
      }
      else if (rxCharacteristic.written()) {
        String command = rxCharacteristic.value();
        handleCommand(command);
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  for (byte i = 0; i < RELAY_COUNT; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }
  
  pinMode(EVSE_Present, INPUT_PULLUP);
  pinMode(CP_Present, INPUT_PULLUP);
  pinMode(Override, INPUT_PULLUP);
  
  lastWindowCheck = millis();
  lastCpCheck = millis();
  lastPrintTime = millis();
  lastTimeCheck = millis();
  lastBtCheckTime = millis();
  
  loadChargeWindow();
  
  RTC.begin();
  
  if (!RTC.isRunning()) {
    Serial.println("\nRTC not running - setting default time");
    struct tm tm;
    tm.tm_year = 123;  // 2023 (2023-1900)
    tm.tm_mon = 0;     // January
    tm.tm_mday = 1;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    RTCTime defaultTime(tm);
    RTC.setTime(defaultTime);
  }
  
  setupBluetooth();
  
  printHelp();
  printStatus();
}

void loop() {
  static byte lastEVSEState = HIGH;
  static byte lastOverrideState = HIGH;
  
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    handleCommand(command);
  }
  
  checkBluetooth();
  
  byte currentEVSEState = digitalRead(EVSE_Present);
  byte currentOverrideState = digitalRead(Override);
  byte currentCPState = digitalRead(CP_Present);

  if (millis() - lastPrintTime > STATUS_PRINT_INTERVAL_MS) {
    printStatus();
    lastPrintTime = millis();
  }

  if (currentEVSEState != lastEVSEState) {
    if (currentEVSEState == LOW) {
      printToAll("\nEVSE Connected \nWaiting for charge window or override\n");
      if (!shouldCharge()) chargeAttempted = false;
    } else {
      printToAll("\nEVSE Disconnected");
      stopCharging();
      overrideActive = false;
    }
    lastEVSEState = currentEVSEState;
    delay(DEBOUNCE_DELAY_MS);
  }

  if (lastOverrideState == HIGH && currentOverrideState == LOW) {
    printToAll("\nOverride button pressed");
    if (currentEVSEState == LOW && !isCharging && !isWaitingForCP) {
      overrideActive = true;
      chargeAttempted = true;
      startCharging();
    }
    delay(DEBOUNCE_DELAY_MS);
  }
  lastOverrideState = currentOverrideState;

  if (!shouldCharge()) chargeAttempted = false;

  if (isWaitingForCP) {
    if (currentCPState == LOW) {
      isWaitingForCP = false;
      isCharging = true;
      lastCpCheck = millis();
      printToAll("\nCP detected - Charging started");
    } 
    else if (millis() - cpWaitStartTime >= CP_TIMEOUT_MS) {
      printToAll("\nCP timeout - Stopping charging");
      stopCharging();
      chargeAttempted = true;
    }
  }

  if (isCharging) {
    if (currentCPState == HIGH) {
      printToAll("\nCP lost during charging - Stopping charging");
      stopCharging();
      chargeAttempted = true;
    }
    
    if (!overrideActive && millis() - lastWindowCheck > WINDOW_CHECK_INTERVAL_MS) {
      lastWindowCheck = millis();
      if (!shouldCharge()) {
        printToAll("\nCharge window ended - Stopping charging");
        stopCharging();
      }
    }
  }

  if (!overrideActive && !isCharging && !isWaitingForCP && 
      currentEVSEState == LOW && !chargeAttempted) {
    if (millis() - lastTimeCheck > TIME_CHECK_INTERVAL_MS) {
      if (shouldCharge()) {
        printToAll("\nWithin charge window - Starting charging");
        chargeAttempted = true;
        startCharging();
      }
      lastTimeCheck = millis();
    }
  }

  delay(10);
}