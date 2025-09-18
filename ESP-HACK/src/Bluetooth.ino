#include <Adafruit_SH110X.h>
#include <GyverButton.h>
#include "CONFIG.h"
#include "bluetooth_menu.h"
#include "ble_spam.h"
#include "subghz_menu.h"
#include <SD.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <NimBLEServer.h>
#include <NimBLEAdvertising.h>
#include <vector>
#include <string>
#include <algorithm>

// External objects from main sketch
extern Adafruit_SH1106G display;
extern GButton buttonUp;
extern GButton buttonDown;
extern GButton buttonOK;
extern GButton buttonBack;
extern bool inMenu;
extern byte currentMenu;
extern byte bluetoothMenuIndex;
extern bool inBadBLE;
extern byte badBLEScriptIndex;
extern bool scriptSelected;
extern byte selectedScript;

// Forward declaration for displayBadKBScriptExec
void displayBadKBScriptExec(Adafruit_SH1106G &display, const String& filename, const std::vector<String>& logs, int logTop);

// HID Report Descriptor (Keyboard, Report ID = 1)
static const uint8_t hidReportDescriptor[] = {
  0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
  0x85, 0x01,                    // Report ID (1)
  // Modifiers (8 bits)
  0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
  0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
  // Reserved
  0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
  // LEDs (5 bits) + padding (3 bits)
  0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
  0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
  // 6 keycodes
  0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
  0xC0
};

// Singleton-состояние BLE/HID
bool                  gBleInited   = false;
NimBLEServer*         gServer      = nullptr;
NimBLEHIDDevice*      gHid         = nullptr;
NimBLECharacteristic* gInput       = nullptr;
NimBLEAdvertising*    gAdv         = nullptr;
uint16_t              gConnHandle  = 0xFFFF;

// HID helpers
static inline void _bb_hidSend(const uint8_t rpt[8]) {
  if (!gInput) return;
  gInput->setValue((uint8_t*)rpt, 8);
  gInput->notify();
  delay(12);
}
static inline void _bb_hidPress(uint8_t mod, uint8_t key) {
  uint8_t rpt[8] = {0};
  rpt[0] = mod; rpt[2] = key;
  _bb_hidSend(rpt);
}
static inline void _bb_hidRelease() {
  uint8_t rpt[8] = {0};
  _bb_hidSend(rpt);
}

// Проверка подписки на уведомления (CCCD 0x2902)
static inline bool _bb_isNotifyEnabled(NimBLECharacteristic* ch) {
  if (!ch) return false;
  NimBLEDescriptor* cccd = ch->getDescriptorByUUID((uint16_t)0x2902);
  if (!cccd) return false;
  std::string v = cccd->getValue();
  if (v.size() < 2) return false;
  return (static_cast<uint8_t>(v[0]) & 0x01) != 0; // notify bit
}

// ASCII -> HID (US раскладка)
static bool _bb_asciiToHID(char c, uint8_t &key, uint8_t &mod) {
  mod = 0x00;

  // letters
  if (c >= 'a' && c <= 'z') { key = 0x04 + (c - 'a'); return true; }
  if (c >= 'A' && c <= 'Z') { key = 0x04 + (c - 'A'); mod = 0x02; return true; } // Shift

  // digits
  if (c >= '1' && c <= '9') { key = 0x1E + (c - '1'); return true; }
  if (c == '0') { key = 0x27; return true; }

  // whitespace
  if (c == ' ') { key = 0x2C; return true; }
  if (c == '\n' || c == '\r') { key = 0x28; return true; }
  if (c == '\t') { key = 0x2B; return true; }

  // number row shifted symbols
  if (c == '!') { key = 0x1E; mod = 0x02; return true; } // Shift+1
  if (c == '@') { key = 0x1F; mod = 0x02; return true; } // Shift+2
  if (c == '#') { key = 0x20; mod = 0x02; return true; } // Shift+3
  if (c == '$') { key = 0x21; mod = 0x02; return true; } // Shift+4
  if (c == '%') { key = 0x22; mod = 0x02; return true; } // Shift+5
  if (c == '^') { key = 0x23; mod = 0x02; return true; } // Shift+6
  if (c == '&') { key = 0x24; mod = 0x02; return true; } // Shift+7
  if (c == '*') { key = 0x25; mod = 0x02; return true; } // Shift+8
  if (c == '(') { key = 0x26; mod = 0x02; return true; } // Shift+9
  if (c == ')') { key = 0x27; mod = 0x02; return true; } // Shift+0

  // punctuation keys and their shifts
  if (c == '-') { key = 0x2D; return true; }
  if (c == '_') { key = 0x2D; mod = 0x02; return true; }

  if (c == '=') { key = 0x2E; return true; }
  if (c == '+') { key = 0x2E; mod = 0x02; return true; }

  if (c == '[') { key = 0x2F; return true; }
  if (c == '{') { key = 0x2F; mod = 0x02; return true; }

  if (c == ']') { key = 0x30; return true; }
  if (c == '}') { key = 0x30; mod = 0x02; return true; }

  if (c == '\\') { key = 0x31; return true; }
  if (c == '|') { key = 0x31; mod = 0x02; return true; }

  if (c == ';') { key = 0x33; return true; }
  if (c == ':') { key = 0x33; mod = 0x02; return true; }

  if (c == '\'') { key = 0x34; return true; }
  if (c == '\"') { key = 0x34; mod = 0x02; return true; }

  if (c == '`') { key = 0x35; return true; }
  if (c == '~') { key = 0x35; mod = 0x02; return true; }

  if (c == ',') { key = 0x36; return true; }
  if (c == '<') { key = 0x36; mod = 0x02; return true; }

  if (c == '.') { key = 0x37; return true; }
  if (c == '>') { key = 0x37; mod = 0x02; return true; }

  if (c == '/') { key = 0x38; return true; }
  if (c == '?') { key = 0x38; mod = 0x02; return true; }

  return false;
}

static void _bb_sendText(const String& s) {
  for (size_t i = 0; i < s.length(); ++i) {
    uint8_t key, mod;
    if (!_bb_asciiToHID(s[i], key, mod)) continue;
    _bb_hidPress(mod, key);
    delay(8);
    _bb_hidRelease();
    delay(8);
  }
}

// DuckyScript parser
static int _bb_parseDelay(const String& s) {
  long v = s.toInt();
  if (v < 0) v = 0;
  if (v > 60000) v = 60000;
  return (int)v;
}

static bool _bb_runDuckyScript(const char* filename, std::vector<String>& logs, Adafruit_SH1106G &display) {
  if (!gServer || !gInput) {
    logs.push_back("BLE not initialized");
    Serial.println("[BadKB] BLE not initialized");
    displayBadKBScriptExec(display, filename, logs, logs.size() > 4 ? logs.size() - 4 : 0);
    return false;
  }
  if (gServer->getConnectedCount() == 0) {
    logs.push_back("No BLE connection");
    Serial.println("[BadKB] No BLE connection");
    displayBadKBScriptExec(display, filename, logs, logs.size() > 4 ? logs.size() - 4 : 0);
    return false;
  }

  File file = SD.open(filename, FILE_READ);
  if (!file) {
    logs.push_back("File not found");
    Serial.printf("[BadKB] Failed to open file: %s\n", filename);
    displayBadKBScriptExec(display, filename, logs, logs.size() > 4 ? logs.size() - 4 : 0);
    return false;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    logs.push_back(line); // Log the command
    Serial.printf("[BadKB] Executing: %s\n", line.c_str());
    displayBadKBScriptExec(display, filename, logs, logs.size() > 4 ? logs.size() - 4 : 0);

    if (line.startsWith("REM")) { continue; }

    if (line.startsWith("DELAY ")) {
      int ms = _bb_parseDelay(line.substring(6));
      Serial.printf("[BadKB] Delaying %d ms\n", ms);
      delay(ms);
      continue;
    }

    if (line.equalsIgnoreCase("ENTER")) {
      Serial.println("[BadKB] Sending ENTER");
      _bb_hidPress(0x00, 0x28); delay(25); _bb_hidRelease();
      delay(6);
      continue;
    }

    if (line.equalsIgnoreCase("GUI R") || line.equalsIgnoreCase("GUI r") || line.equalsIgnoreCase("WINR")) {
      Serial.println("[BadKB] Sending GUI+R");
      _bb_hidPress(0x08, 0x15); delay(30); _bb_hidRelease();
      delay(60);
      continue;
    }

    if (line.startsWith("STRING ")) {
      String text = line.substring(7);
      Serial.printf("[BadKB] Sending STRING: %s\n", text.c_str());
      _bb_sendText(text);
      continue;
    }

    Serial.printf("[BadKB] Skipping unknown command: %s\n", line.c_str());
  }

  file.close();
  Serial.println("[BadKB] Script execution completed");
  logs.push_back("Done");
  displayBadKBScriptExec(display, filename, logs, logs.size() > 4 ? logs.size() - 4 : 0);
  return true;
}

// Callbacks сервера
class PairServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    gConnHandle = connInfo.getConnHandle();
    Serial.printf("[BLE Pair] Connected (handle=%u)\n", gConnHandle);
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    Serial.printf("[BLE Pair] Disconnected, reason=%d\n", reason);
    gConnHandle = 0xFFFF;
    // Разрешаем быстрые переподключения
    if (gAdv) {
      gAdv->start();
    }
  }
};

// Разовая инициализация BLE/HID (singleton)
static void ensureBleHidInited() {
  if (gBleInited) return;

  Serial.println("[BLE Pair] Initializing BLE (ESP-BLE)...");
  NimBLEDevice::init("ESP-BLE");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  // Безэкранная клавиатура: бондинг + Secure Connections, без MITM
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityAuth(true, false, true); // bond, !mitm, sc

  // Сервер
  gServer = NimBLEDevice::createServer();
  static PairServerCallbacks sCallbacks; // статический объект, живёт всё время
  gServer->setCallbacks(&sCallbacks);

  // HID
  gHid = new NimBLEHIDDevice(gServer);
  gInput = gHid->getInputReport(1); // Report ID = 1

  gHid->setManufacturer("xAI");
  gHid->setPnp(0x02, 0x1234, 0xABCD, 0x0110);
  gHid->setHidInfo(0x00, 0x01);
  gHid->setReportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
  gHid->setBatteryLevel(100);
  gHid->startServices();

  // Реклама
  gAdv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advData;
  NimBLEAdvertisementData scanData;

  advData.setFlags(0x06); // LE General Discoverable + BR/EDR Not Supported
  advData.setAppearance(0x03C1); // HID Keyboard
  advData.addServiceUUID(gHid->getHidService()->getUUID());
  scanData.setName("ESP-BLE");

  gAdv->setAdvertisementData(advData);
  gAdv->setScanResponseData(scanData);
  gAdv->setMinInterval(32); // ~20 ms
  gAdv->setMaxInterval(64); // ~40 ms

  gBleInited = true;
  Serial.println("[BLE Pair] BLE/HID initialized (one-time)");
}

// Function to stop BLE
static void stopBLE() {
  if (gAdv && gAdv->isAdvertising()) {
    gAdv->stop();
    Serial.println("[BLE Pair] Advertising stopped");
  }
  if (gBleInited) {
    NimBLEDevice::deinit();
    gBleInited = false;
    gServer = nullptr;
    gHid = nullptr;
    gInput = nullptr;
    gAdv = nullptr;
    gConnHandle = 0xFFFF;
    Serial.println("[BLE Pair] BLE deinitialized");
  }
}

// File explorer state
#define MAX_FILES 50
struct BadKBFileEntry {
  String name;
  bool isDir;
};
BadKBFileEntry badKBFileList[MAX_FILES];
int badKBFileCount = 0;
int badKBFileIndex = 0;
String badKBCurrentDir = "/BadKB";

// Function to load file list
static void loadBadKBFileList() {
  badKBFileCount = 0;
  File root = SD.open(badKBCurrentDir);
  if (!root) {
    Serial.println(F("Failed to open directory"));
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(F("Not a directory"));
    root.close();
    return;
  }

  // First pass: count files and dirs
  int dirCount = 0, fileCount = 0;
  File file = root.openNextFile();
  while (file) {
    String fname = String(file.name());
    if (fname[0] != '.') {
      if (file.isDirectory()) dirCount++;
      else if (fname.endsWith(".txt")) fileCount++;
    }
    file.close();
    file = root.openNextFile();
  }

  // Second pass: load dirs then files, sorted
  // Load dirs
  root.rewindDirectory();
  file = root.openNextFile();
  std::vector<String> dirs;
  while (file) {
    String fname = String(file.name());
    if (file.isDirectory() && fname[0] != '.') {
      dirs.push_back(fname);
    }
    file.close();
    file = root.openNextFile();
  }
  std::sort(dirs.begin(), dirs.end());
  for (auto& d : dirs) {
    badKBFileList[badKBFileCount].name = d;
    badKBFileList[badKBFileCount].isDir = true;
    badKBFileCount++;
  }

  // Load files
  root.rewindDirectory();
  file = root.openNextFile();
  std::vector<String> files;
  while (file) {
    String fname = String(file.name());
    if (!file.isDirectory() && fname[0] != '.' && fname.endsWith(".txt")) {
      files.push_back(fname);
    }
    file.close();
    file = root.openNextFile();
  }
  std::sort(files.begin(), files.end());
  for (auto& f : files) {
    badKBFileList[badKBFileCount].name = f;
    badKBFileList[badKBFileCount].isDir = false;
    badKBFileCount++;
  }

  root.close();
}

// Display BadKB explorer
void drawBadKBExplorer(Adafruit_SH1106G &display) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setTextColor(SH110X_WHITE);

  // Display current directory
  display.setCursor(0, 0);
  display.print(badKBCurrentDir);

  // Separator
  display.setCursor(0, 12);
  display.println(F("---------------------"));

  // If empty
  if (badKBFileCount == 0) {
    display.setCursor(0, 24);
    display.println(F("No files."));
    display.display();
    return;
  }

  // Scrolling window
  const int perPage = 4;
  int maxStart = (badKBFileCount > perPage) ? (badKBFileCount - perPage) : 0;
  int startIndex = badKBFileIndex;
  if (startIndex > maxStart) startIndex = maxStart;
  for (int i = 0; i < perPage; i++) {
    int idx = startIndex + i;
    if (idx >= badKBFileCount) break;
    display.setCursor(0, 24 + i * 10);
    if (idx == badKBFileIndex) {
      display.fillRect(0, 24 + i * 10 - 1, display.width(), 10, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    } else {
      display.setTextColor(SH110X_WHITE);
    }
    String name = badKBFileList[idx].name;
    if (badKBFileList[idx].isDir) name = "[" + name + "]";
    display.print(name);
  }
  display.display();
}

// Display deletion confirmation
void drawDeleteConfirm(Adafruit_SH1106G &display, const String& filename) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  display.setCursor(0, 0);
  display.print(F("File: "));
  display.print(filename);
  display.setCursor(0, 24);
  display.print(F("Press OK to DELETE."));
  display.display();
}

// Display script execution screen
void displayBadKBScriptExec(Adafruit_SH1106G &display, const String& filename, const std::vector<String>& logs, int logTop) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  display.setCursor(0, 0);
  display.println(filename);
  display.println(F("---------------------"));

  int maxLogs = (display.height() - 20) / 10;
  for (int i = 0; i < maxLogs; i++) {
    int idx = logTop + i;
    if (idx >= (int)logs.size()) break;
    display.setCursor(0, 20 + i * 10);
    display.print(logs[idx]);
  }
  display.display();
}

// BLE Spam states
enum BLESpamState { IDLE, READY, RUNNING };
BLESpamState bleSpamState = IDLE;
EBLEPayloadType currentSpamType;

// Function to generate random name for Microsoft spam
String generateRandomName() {
  const char* charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  int len = random(1, 11); // Random length between 1 and 10
  String randomName = "";
  for (int i = 0; i < len; ++i) {
    randomName += charset[random(0, strlen(charset))];
  }
  return randomName;
}

void displayBLESpamHeader(const char* spamType, bool running) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  display.setCursor(0, 0);
  if (running) {
    display.println(String(spamType) + "...");
    display.println(F("---------------------"));
  } else {
    display.println(F("Press OK."));
    display.println(F("---------------------"));
  }
}

void displayBLESpamDevice(const char* deviceName) {
  // Clear only the device name area (line 16 to 26)
  display.fillRect(0, 16, display.width(), 10, SH110X_BLACK);
  display.setCursor(0, 16);
  display.print(deviceName);
  display.display();
}

void displayFullBLESpamScreen(const char* spamType, bool running, const char* deviceName = "") {
  displayBLESpamHeader(spamType, running);
  if (running) {
    display.setCursor(0, 16);
    display.print(deviceName);
  }
  display.display();
}

void handleBluetoothSubmenu() {
  // Update button states
  buttonUp.tick();
  buttonDown.tick();
  buttonOK.tick();
  buttonBack.tick();

  // Display appropriate menu
  static bool explorerLoaded = false;
  static std::vector<String> execLogs;
  static int execLogTop = 0;
  static bool scriptRunning = false;
  static bool scriptDone = false;
  static String selectedFile = "";
  static bool waitingForConnection = false;
  static bool deleteConfirm = false;
  static String deleteFile = "";
  static bool justEnteredBadBLE = false;

  if (inBadBLE) {
    if (deleteConfirm) {
      drawDeleteConfirm(display, deleteFile);
      if (buttonOK.isClick()) {
        String fullPath = badKBCurrentDir + "/" + deleteFile;
        bool success;
        if (badKBFileList[badKBFileIndex].isDir) {
          success = SD.rmdir(fullPath);
        } else {
          success = SD.remove(fullPath);
        }
        display.clearDisplay();
        display.setTextColor(SH110X_WHITE);
        display.setTextWrap(false);
        if (success) {
          display.drawBitmap(5, 2, image_DolphinMafia_bits, 119, 62, SH110X_WHITE);
          display.setCursor(84, 15);
          display.print(F("Deleted"));
          display.display();
          Serial.printf("[BadKB] File deleted: %s\n", fullPath.c_str());
        } else {
          display.setCursor(0, 0);
          display.print(F("Failed to delete"));
          display.display();
          Serial.printf("[BadKB] Failed to delete file: %s\n", fullPath.c_str());
        }
        delay(1000);
        deleteConfirm = false;
        explorerLoaded = false;
        badKBFileCount = 0;
        badKBFileIndex = 0;
        loadBadKBFileList();
        drawBadKBExplorer(display);
      }
      if (buttonBack.isClick()) {
        deleteConfirm = false;
        drawBadKBExplorer(display);
        Serial.println(F("[BadKB] Cancelled file deletion"));
      }
      return;
    }
    if (scriptSelected) {
      if (scriptRunning) {
        // Initialize BLE and start advertising
        ensureBleHidInited();
        if (!(gAdv && gAdv->isAdvertising())) {
          gAdv->start();
          Serial.println("[BadKB] Started BLE advertising");
        }

        // Wait for BLE connection with timeout
        if (!waitingForConnection) {
          waitingForConnection = true;
          execLogs.clear();
          execLogs.push_back("Waiting for BLE...");
          displayBadKBScriptExec(display, selectedFile, execLogs, 0);
        }

        const uint32_t connectionTimeout = 30000; // 30 seconds
        static uint32_t waitStartTime = 0;
        if (waitStartTime == 0) {
          waitStartTime = millis();
        }

        if (gServer && gServer->getConnectedCount() > 0) {
          // Connection established, run the script
          waitingForConnection = false;
          waitStartTime = 0;
          execLogs.clear();
          bool ok = _bb_runDuckyScript(selectedFile.c_str(), execLogs, display);
          scriptRunning = false;
          scriptDone = true;
          if (!ok) {
            execLogs.push_back("Failed");
          }
          displayBadKBScriptExec(display, selectedFile, execLogs, execLogs.size() > 4 ? execLogs.size() - 4 : 0);
        } else if (millis() - waitStartTime >= connectionTimeout) {
          // Timeout waiting for connection
          waitingForConnection = false;
          waitStartTime = 0;
          scriptRunning = false;
          scriptDone = true;
          execLogs.clear();
          execLogs.push_back("Connection timeout");
          displayBadKBScriptExec(display, selectedFile, execLogs, 0);
          Serial.println("[BadKB] Connection timeout");
        }
      } else {
        displayBadKBScriptExec(display, selectedFile, execLogs, execLogTop);
      }
    } else {
      if (!explorerLoaded) {
        loadBadKBFileList();
        explorerLoaded = true;
        justEnteredBadBLE = false; // Reset on loading explorer
      }
      drawBadKBExplorer(display);
    }
  } else {
    if (bleSpamState == IDLE) {
      displayBluetoothMenu(display, bluetoothMenuIndex);
    } else {
      // For READY/RUNNING states, show spam screen
      static String currentDeviceName = "";
      if (bleSpamState == READY) {
        displayFullBLESpamScreen(bluetoothMenuItems[bluetoothMenuIndex], false);
      } else {
        // In RUNNING state, update only when device changes
        static bool firstRun = true;
        if (firstRun) {
          displayFullBLESpamScreen(
            bluetoothMenuItems[bluetoothMenuIndex],
            true,
            currentDeviceName.c_str()
          );
          firstRun = false;
        }
      }
    }
  }

  // Handle navigation
  if (inBadBLE) {
    if (deleteConfirm) {
      // Handled in the display block
    } else if (scriptSelected) {
      // Handle scrolling logs
      if (buttonUp.isClick() && execLogTop > 0) {
        execLogTop--;
        displayBadKBScriptExec(display, selectedFile, execLogs, execLogTop);
      }
      if (buttonDown.isClick() && execLogTop + ((display.height() - 20) / 10) < (int)execLogs.size()) {
        execLogTop++;
        displayBadKBScriptExec(display, selectedFile, execLogs, execLogTop);
      }
      // Handle back from selected script
      if (buttonBack.isClick()) {
        scriptSelected = false;
        scriptRunning = false;
        scriptDone = false;
        waitingForConnection = false;
        execLogs.clear();
        execLogTop = 0;
        if (gAdv && gAdv->isAdvertising()) {
          gAdv->stop();
        }
        explorerLoaded = false;
        drawBadKBExplorer(display);
        Serial.println(F("[BadKB] Back to BadKB script selection"));
      }
      if (buttonOK.isClick() && scriptDone) {
        // Rerun script
        scriptRunning = true;
        scriptDone = false;
        waitingForConnection = false;
      }
    } else {
      // Navigate BadKB explorer
      if (buttonUp.isClick()) {
        if (badKBFileIndex > 0) {
          badKBFileIndex--;
          drawBadKBExplorer(display);
        }
        Serial.println(F("[BadKB] menu Up"));
      }
      if (buttonDown.isClick()) {
        if (badKBFileIndex < badKBFileCount - 1) {
          badKBFileIndex++;
          drawBadKBExplorer(display);
        }
        Serial.println(F("[BadKB] menu Down"));
      }
      if (buttonOK.isClick()) {
        if (badKBFileCount == 0) return;
        String selName = badKBFileList[badKBFileIndex].name;
        if (badKBFileList[badKBFileIndex].isDir) {
          badKBCurrentDir += "/" + selName;
          badKBFileIndex = 0;
          explorerLoaded = false;
          drawBadKBExplorer(display);
          Serial.println(F("[BadKB] Entered BadKB directory"));
        } else if (selName.endsWith(".txt")) {
          selectedFile = badKBCurrentDir + "/" + selName;
          scriptSelected = true;
          scriptRunning = true;
          scriptDone = false;
          waitingForConnection = false;
          execLogs.clear();
          execLogTop = 0;
          ensureBleHidInited(); // Initialize BLE when selecting a script
          displayBadKBScriptExec(display, selectedFile, execLogs, execLogTop);
          Serial.println(F("[BadKB] Selected BadKB script"));
        }
      }
      if (buttonBack.isHold()) {
        if (badKBFileCount == 0) return;
        deleteFile = badKBFileList[badKBFileIndex].name;
        deleteConfirm = true;
        drawDeleteConfirm(display, deleteFile);
        Serial.println(F("[BadKB] Initiated delete confirmation"));
      }
      if (buttonBack.isClick() && !justEnteredBadBLE) {
        if (badKBCurrentDir != "/BadKB") {
          int lastSlash = badKBCurrentDir.lastIndexOf('/');
          badKBCurrentDir = badKBCurrentDir.substring(0, lastSlash);
          badKBFileIndex = 0;
          explorerLoaded = false;
          drawBadKBExplorer(display);
          Serial.println(F("[BadKB] Back to parent BadKB directory"));
        } else {
          inBadBLE = false;
          explorerLoaded = false;
          justEnteredBadBLE = false;
          stopBLE(); // Disable BLE when exiting BadKB
          inMenu = true;
          bluetoothMenuIndex = 0;
          display.clearDisplay();
          OLED_printMenu(display, currentMenu);
          display.display();
          delay(10); // Small delay to ensure display refresh
          Serial.println(F("[BadKB] Back to main menu from BadKB"));
        }
      }
    }
  } else {
    // Handle Bluetooth menu and BLE spam states
    if (bleSpamState == IDLE) {
      // Navigate Bluetooth menu
      if (buttonUp.isClick()) {
        bluetoothMenuIndex = (bluetoothMenuIndex - 1 + BLUETOOTH_MENU_ITEM_COUNT) % BLUETOOTH_MENU_ITEM_COUNT;
        displayBluetoothMenu(display, bluetoothMenuIndex);
        Serial.println(F("[Bluetooth] menu Up"));
      }
      if (buttonDown.isClick()) {
        bluetoothMenuIndex = (bluetoothMenuIndex + 1) % BLUETOOTH_MENU_ITEM_COUNT;
        displayBluetoothMenu(display, bluetoothMenuIndex);
        Serial.println(F("[Bluetooth] menu Down"));
      }
      if (buttonOK.isClick()) {
        Serial.println(F("[Bluetooth] menu OK"));
        if (bluetoothMenuIndex == 3) {
          // Enter BadKB submenu
          inBadBLE = true;
          badKBCurrentDir = "/BadKB";
          badKBFileIndex = 0;
          explorerLoaded = false;
          justEnteredBadBLE = true; // Flag to prevent immediate exit
          ensureBleHidInited(); // Initialize BLE when entering BadKB
          Serial.println(F("[BadKB] Entered BadKB submenu"));
        } else {
          // Handle BLE Spam options
          bleSpamState = READY;
          switch (bluetoothMenuIndex) {
            case 0: currentSpamType = AppleJuice; break;
            case 1: currentSpamType = Google; break;
            case 2: currentSpamType = Microsoft; break;
          }
          displayFullBLESpamScreen(bluetoothMenuItems[bluetoothMenuIndex], false);
          Serial.println(F("[Bluetooth] Ready for BLE spam"));
        }
      }
      if (buttonBack.isClick()) {
        inMenu = true;
        bluetoothMenuIndex = 0;
        display.clearDisplay();
        OLED_printMenu(display, currentMenu);
        display.display();
        delay(10); // Small delay to ensure display refresh
        Serial.println(F("[Bluetooth] Back to main menu from Bluetooth"));
      }
    } else if (bleSpamState == READY || bleSpamState == RUNNING) {
      static unsigned long lastSpamTime = 0;
      static int deviceIndex = 0;
      static String currentDeviceName = "";
      static bool firstRun = true;

      // Handle button presses
      if (buttonOK.isClick()) {
        if (bleSpamState == READY) {
          // Start spamming
          bleSpamState = RUNNING;
          BLEDevice::init("ESP-HACK");
          lastSpamTime = 0;
          deviceIndex = 0;
          firstRun = true;
          Serial.println(F("[Bluetooth] Started BLE spam"));
        } else {
          // Stop spamming
          bleSpamState = READY;
          BLEDevice::deinit();
          firstRun = true;
          Serial.println(F("[Bluetooth] Stopped BLE spam"));
          displayFullBLESpamScreen(bluetoothMenuItems[bluetoothMenuIndex], false);
        }
      }

      if (buttonBack.isClick()) {
        // Back to menu
        if (bleSpamState == RUNNING) {
          BLEDevice::deinit();
        }
        bleSpamState = IDLE;
        firstRun = true;
        displayBluetoothMenu(display, bluetoothMenuIndex);
        Serial.println(F("[Bluetooth] Back to Bluetooth menu"));
      }

      // Execute spam if running
      if (bleSpamState == RUNNING) {
        const unsigned long spamInterval = 100; // Interval

        if (millis() - lastSpamTime >= spamInterval) {
          // Generate new device name
          switch (currentSpamType) {
            case AppleJuice:
              currentDeviceName = appleDevices[deviceIndex % appleDevicesCount].name;
              break;
            case Google:
              currentDeviceName = devices[deviceIndex % devicesCount].name;
              break;
            case Microsoft:
              currentDeviceName = generateRandomName();
              break;
          }

          // Execute spam
          executeSpam(currentSpamType);

          // Update display
          if (firstRun) {
            displayFullBLESpamScreen(
              bluetoothMenuItems[bluetoothMenuIndex],
              true,
              currentDeviceName.c_str()
            );
            firstRun = false;
          } else {
            displayBLESpamDevice(currentDeviceName.c_str());
          }

          deviceIndex++;
          lastSpamTime = millis();
        }
      }
    }
  }
}