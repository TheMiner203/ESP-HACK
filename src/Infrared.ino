#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <GyverButton.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <SD.h>
#include "CONFIG.h"
#include "infrared_menu.h"
#include "subghz_menu.h"

extern Adafruit_SH1106G display;
extern GButton buttonUp;
extern GButton buttonDown;
extern GButton buttonOK;
extern GButton buttonBack;
extern byte currentMenu;
extern bool inMenu;
extern byte irMenuIndex;

bool inIRMenu = false; // Flag to track if in IR submenu

// Define states
enum AppState { MENU, IR_SELECTION, SENDING_IR, IR_FILE_EXPLORER, IR_READING, IR_DELETE_CONFIRM, IR_SIGNAL_SUBMENU };
AppState state = MENU;

// IR signals
const uint32_t tvOffSignals[] = {
  0x400d1e70, 0xE0E09966, 0xE0E0F00F, 0xE0E019E6, 0xE0E0D02F,
  0x400d1e70, 0x95, 0x55EC5DB7, 0x4006F989, 0x2E90,
  0x22C1, 0x5D21, 0xED12BF40, 0x20DF10EF, 0xE0E040BF,
  0x100C, 0xA90, 0xFD02FE01, 0x400401FB, 0x20DF23DC,
  0x20DF906F, 0xE0E0807F, 0x1A90, 0xFD22DE01, 0x400405F5,
  0x20DFC03F
};
const uint32_t pjOffSignals[] = {
  0xA90, 0x4B74, 0xB04F, 0xA59A, 0xB90,
  0xC90, 0xD90, 0x4BB4, 0x30CF, 0xA55A
};
const uint32_t acOffSignals[] = {
  0xB2F, 0xB2E, 0xB2D, 0xB2C, 0xA1F0,
  0xE0E09966, 0xC2A4, 0xB4C8, 0xB2A, 0xA1E0,
  0xE0E019E6, 0xC2A5, 0xB4C9
};

const int numTVSignals = sizeof(tvOffSignals) / sizeof(tvOffSignals[0]);
const int numPJSignals = sizeof(pjOffSignals) / sizeof(pjOffSignals[0]);
const int numACSignals = sizeof(acOffSignals) / sizeof(acOffSignals[0]);
int signalIndex = 0;
unsigned long lastSendTime = 0;
int currentSignalCount = 0;
const uint32_t* currentSignals = nullptr;

// File explorer state
#define MAX_FILES 50
struct IRFileEntry { // Renamed to avoid conflict with SubGHz.ino
  String name;
  bool isDir;
};
IRFileEntry irFileList[MAX_FILES];
int irFileCount = 0;
int irFileIndex = 0;
bool irInFileExplorer = false;
bool irInDeleteConfirm = false;
String irSelectedFile = "";
String irCurrentDir = "/Infrared"; // Tracks current directory

// Signal submenu state
#define MAX_SIGNALS 20
String irSignalList[MAX_SIGNALS];
int irSignalCount = 0;
int irSignalIndex = 0;
String irSelectedSignal = "";

// IR hardware
IRsend irsend(IR_TRANSMITTER);
IRrecv irrecv(IR_RECIVER, 1024, 100); // Increased buffer to 1024, timeout 100ms
decode_results results;

// IR read variables
bool readSignal = false;
String strDeviceContent = "";
int signalsRead = 0;
uint16_t* rawcode = nullptr;
uint16_t raw_data_len = 0;
#define IR_FREQUENCY 38000
#define DUTY_CYCLE 0.330000

// Function to initialize IR
void initIR() {
  irsend.begin();
  pinMode(IR_RECIVER, INPUT);
  Serial.println(F("IR initialized: TX on pin 4, RX on pin 14"));
}

// Function to convert uint32 to string for IR file format
String uint32ToString(uint32_t value) {
  char buffer[12] = {0}; // 8 hex digits + 3 spaces + 1 null terminator
  snprintf(
    buffer,
    sizeof(buffer),
    "%02X %02X %02X %02X",
    value & 0xFF,
    (value >> 8) & 0xFF,
    (value >> 16) & 0xFF,
    (value >> 24) & 0xFF
  );
  return String(buffer);
}

// Function to draw IR selection screen (for TV-OFF, PJ-OFF, AC-OFF, IR-Send signals)
void displayIRSelection(byte menuIndex, String signalName = "") {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  display.setTextSize(1);
  display.setCursor(38, 54);
  display.print(F("Press OK."));
  
  if (menuIndex == 0 || menuIndex == 2 || menuIndex == 3 || menuIndex == 4) {
    display.drawBitmap(52, 16, image_Power_bits, 25, 27, SH110X_WHITE);
  }
  if (menuIndex == 0 && signalName != "") {
    display.setCursor(0, 0);
    display.print(F("Signal: "));
    display.print(signalName.length() > 16 ? signalName.substring(0, 16) : signalName);
  }
  display.display();
}

// Function to draw IR sending progress
void drawSendingScreen(int progress) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  
  display.setTextSize(1);
  display.setCursor(56, 6);
  if (irMenuIndex == 2 || irMenuIndex == 3 || irMenuIndex == 4) {
    display.print(progress);
    display.print(F("%"));
  }
  
  if (irMenuIndex == 0 || irMenuIndex == 2 || irMenuIndex == 3 || irMenuIndex == 4) {
    display.drawBitmap(52, 16, image_Power_hvr_bits, 25, 27, SH110X_WHITE);
    display.setCursor(32, 54);
    display.print(irMenuIndex == 0 ? F(" Sending...") : F("IR-Attack..."));
  }
  
  display.display();
}

// Function to draw IR reading screen
void drawReadingScreen() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  display.setTextSize(1);
  
  if (!readSignal) {
    display.setCursor(5, 53);
    display.print(F("Waiting signal..."));
    display.drawBitmap(0, 12, image_InfraredLearnShort_bits, 128, 31, SH110X_WHITE);
  } else {
    // Display signal information in a structured layout
    display.setCursor(0, 0);
    display.print(F("Signal:"));

    // Display code
    display.setCursor(0, 12);
    display.print(F("Code: "));
    String code;
    if (results.decode_type == UNKNOWN) {
      rawcode = resultToRawArray(&results);
      raw_data_len = getCorrectedRawLength(&results);
      code = "";
      for (uint16_t i = 0; i < raw_data_len && i < 4; i++) { // Limit to first 4 values for display
        code += String(rawcode[i]);
        if (i < raw_data_len - 1 && i < 3) code += " ";
      }
      if (raw_data_len > 4) code += "...";
      delete[] rawcode;
      rawcode = nullptr;
    } else {
      // For parsed signals like Samsung32, show only address
      code = uint32ToString(results.address);
    }
    display.print(code.length() > 16 ? code.substring(0, 16) : code);

    // Display type
    display.setCursor(0, 24);
    display.print(F("Type: "));
    String signalType = (results.decode_type == UNKNOWN ? "RAW" : typeToString(results.decode_type, results.repeat));
    display.print(signalType.length() > 16 ? signalType.substring(0, 16) : signalType);

    // Display bits
    display.setCursor(0, 36);
    display.print(F("Bits: "));
    display.print(results.bits);

    // Display "Hold OK to save"
    display.setCursor(0, 51);
    display.print(F("Hold OK to save"));
  }
  
  display.display();
}

// Function to draw save confirmation
void drawSaveConfirm() {
  display.clearDisplay();
  display.drawBitmap(16, 6, image_DolphinSaved_bits, 92, 58, SH110X_WHITE);
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  display.setCursor(6, 16);
  display.print(F("Saved"));
  display.display();
}

// Function to draw file explorer
void drawFileExplorer() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setTextColor(SH110X_WHITE);

  // Display current directory
  display.setCursor(0, 0);
  display.print(irCurrentDir);

  // Separator
  display.setCursor(0, 12);
  display.println(F("---------------------"));

  // If empty
  if (irFileCount == 0) {
    display.setCursor(0, 24);
    display.println(F("No files."));
    display.display();
    return;
  }

  // Scrolling window
  const int perPage = 4;
  int maxStart = (irFileCount > perPage) ? (irFileCount - perPage) : 0;
  int startIndex = irFileIndex;
  if (startIndex > maxStart) startIndex = maxStart;
  for (int i = 0; i < perPage; i++) {
    int idx = startIndex + i;
    if (idx >= irFileCount) break;
    display.setCursor(0, 24 + i * 10);
    if (idx == irFileIndex) {
      display.fillRect(0, 24 + i * 10 - 1, display.width(), 10, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    } else {
      display.setTextColor(SH110X_WHITE);
    }
    String name = irFileList[idx].name;
    if (irFileList[idx].isDir) name = "[" + name + "]";
    display.print(name);
  }
  display.display();
}

// Function to draw signal submenu
void drawSignalSubmenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setTextColor(SH110X_WHITE);

  // Display current file
  display.setCursor(0, 0);
  String name = irSelectedFile;
  if (name.length() > 16) name = name.substring(0, 16);
  display.print(F("File: "));
  display.print(name);

  // Separator
  display.setCursor(0, 12);
  display.println(F("---------------------"));

  // If empty
  if (irSignalCount == 0) {
    display.setCursor(0, 24);
    display.println(F("No signals."));
    display.display();
    return;
  }

  // Scrolling window
  const int perPage = 4;
  int maxStart = (irSignalCount > perPage) ? (irSignalCount - perPage) : 0;
  int startIndex = irSignalIndex;
  if (startIndex > maxStart) startIndex = maxStart;
  for (int i = 0; i < perPage; i++) {
    int idx = startIndex + i;
    if (idx >= irSignalCount) break;
    display.setCursor(0, 24 + i * 10);
    if (idx == irSignalIndex) {
      display.fillRect(0, 24 + i * 10 - 1, display.width(), 10, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    } else {
      display.setTextColor(SH110X_WHITE);
    }
    String signalName = irSignalList[idx];
    if (signalName.length() > 16) signalName = signalName.substring(0, 16);
    display.print(signalName);
  }
  display.display();
}

// Function to draw delete confirmation
void drawDeleteConfirm() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  display.setCursor(0, 0);
  String name = irFileList[irFileIndex].name;
  if (name.length() > 16) name = name.substring(0, 16);
  display.print(F("File: "));
  display.print(name);
  display.setCursor(0, 24);
  display.print(F("Press OK to DELETE."));
  display.display();
}

// Load IR signals from file
void loadIRSignals(String fileName) {
  irSignalCount = 0;
  File file = SD.open(irCurrentDir + "/" + fileName, FILE_READ);
  if (!file) {
    Serial.print(F("Failed to open file for signals: "));
    Serial.println(fileName);
    return;
  }

  String signalName = "";
  while (file.available() && irSignalCount < MAX_SIGNALS) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.startsWith("name:")) {
      signalName = line.substring(5);
      signalName.trim();
      irSignalList[irSignalCount] = signalName;
      irSignalCount++;
    }
  }
  file.close();
  Serial.print(F("Found "));
  Serial.print(irSignalCount);
  Serial.print(F(" signals in "));
  Serial.println(fileName);
}

// Send IR signal from file by index
bool sendIRSignal(String fileName, int signalIdx) {
  File file = SD.open(irCurrentDir + "/" + fileName, FILE_READ);
  if (!file) {
    Serial.print(F("Failed to open file: "));
    Serial.println(fileName);
    return false;
  }

  String protocol = "";
  String address = "";
  String command = "";
  String rawData = "";
  uint16_t frequency = 38000;
  uint8_t bits = 32;
  bool parsedMode = false;
  int currentSignal = -1;

  String line;
  while (file.available()) {
    line = file.readStringUntil('\n');
    line.trim();
    if (line.startsWith("name:")) {
      currentSignal++;
    }
    if (currentSignal == signalIdx) {
      if (line.startsWith("type:")) {
        String type = line.substring(5);
        type.trim();
        parsedMode = (type == "parsed");
      } else if (line.startsWith("protocol:")) {
        protocol = line.substring(9);
        protocol.trim();
      } else if (line.startsWith("address:")) {
        address = line.substring(8);
        address.trim();
      } else if (line.startsWith("command:")) {
        command = line.substring(8);
        command.trim();
      }
    }
  }
  file.close();

  if (currentSignal < signalIdx) {
    Serial.println(F("Signal index not found in file"));
    return false;
  }

  if (parsedMode && protocol != "" && address != "" && command != "") {
    if (protocol.equalsIgnoreCase("Samsung32")) {
      uint8_t addressValue = strtoul(address.substring(0, 2).c_str(), nullptr, 16);
      uint8_t commandValue = strtoul(command.substring(0, 2).c_str(), nullptr, 16);
      uint64_t data = irsend.encodeSAMSUNG(addressValue, commandValue);
      irsend.sendSAMSUNG(data, 32);
      Serial.println(F("Sent Samsung32 IR signal"));
      return true;
    } else if (protocol.equalsIgnoreCase("SONY")) {
      uint8_t commandValue = strtoul(command.substring(0, 2).c_str(), nullptr, 16);
      irsend.sendSony(commandValue, 12); // SONY protocol typically uses 12 bits for command
      Serial.println(F("Sent SONY IR signal"));
      return true;
    }
    Serial.print(F("Unsupported protocol: "));
    Serial.println(protocol);
    return false;
  } else if (!parsedMode && rawData != "" && frequency != 0) {
    uint16_t dataBufferSize = 1;
    for (int i = 0; i < rawData.length(); i++) {
      if (rawData[i] == ' ') dataBufferSize++;
    }
    uint16_t* dataBuffer = (uint16_t*)malloc(dataBufferSize * sizeof(uint16_t));
    if (!dataBuffer) {
      Serial.println(F("Failed to allocate memory for IR data"));
      return false;
    }
    uint16_t count = 0;
    String data = rawData;
    while (data.length() > 0 && count < dataBufferSize) {
      int delimiterIndex = data.indexOf(' ');
      if (delimiterIndex == -1) delimiterIndex = data.length();
      String dataChunk = data.substring(0, delimiterIndex);
      data.remove(0, delimiterIndex + 1);
      dataBuffer[count++] = dataChunk.toInt();
    }
    irsend.sendRaw(dataBuffer, count, frequency);
    free(dataBuffer);
    Serial.println(F("Sent RAW IR signal"));
    return true;
  }
  Serial.println(F("Invalid IR signal format"));
  return false;
}

// Load IR file list
void loadIRFileList() {
  irFileCount = 0;
  File dir = SD.open(irCurrentDir);
  if (!dir) {
    Serial.print(F("Failed to open directory: "));
    Serial.println(irCurrentDir);
    return;
  }
  // First collect directories
  while (irFileCount < MAX_FILES) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (entry.isDirectory()) {
      irFileList[irFileCount].name = entry.name();
      irFileList[irFileCount].isDir = true;
      irFileCount++;
    }
    entry.close();
  }
  // Then collect .ir files
  dir.rewindDirectory();
  while (irFileCount < MAX_FILES) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory() && String(entry.name()).endsWith(".ir")) {
      irFileList[irFileCount].name = entry.name();
      irFileList[irFileCount].isDir = false;
      irFileCount++;
    }
    entry.close();
  }
  dir.close();
  Serial.print(F("Found "));
  Serial.print(irFileCount);
  Serial.println(F(" entries"));
}

// Parse raw signal for display and file
String parseRawSignal() {
  rawcode = resultToRawArray(&results);
  raw_data_len = getCorrectedRawLength(&results);
  String signal_code = "";
  for (uint16_t i = 0; i < raw_data_len; i++) {
    signal_code += String(rawcode[i]) + " ";
  }
  delete[] rawcode;
  rawcode = nullptr;
  signal_code.trim();
  return signal_code;
}

// Append signal to device content string
void appendToDeviceContent(String btn_name) {
  strDeviceContent += "name: " + btn_name + "\n";
  strDeviceContent += "type: parsed\n";
  switch (results.decode_type) {
    case decode_type_t::SAMSUNG: {
      strDeviceContent += "protocol: Samsung32\n";
      break;
    }
    case decode_type_t::SONY: {
      strDeviceContent += "protocol: SONY\n";
      break;
    }
    case decode_type_t::NEC: {
      strDeviceContent += "protocol: NEC\n";
      break;
    }
    case decode_type_t::UNKNOWN: {
      strDeviceContent += "type: raw\n";
      strDeviceContent += "frequency: " + String(IR_FREQUENCY) + "\n";
      strDeviceContent += "duty_cycle: " + String(DUTY_CYCLE) + "\n";
      strDeviceContent += "data: " + parseRawSignal() + "\n";
      return;
    }
    default: {
      strDeviceContent += "protocol: " + typeToString(results.decode_type, results.repeat) + "\n";
      break;
    }
  }
  strDeviceContent += "address: " + uint32ToString(results.address) + "\n";
  strDeviceContent += "command: " + uint32ToString(results.command) + "\n";
  strDeviceContent += "#\n";
}

// Save IR signal to file
bool saveIRSignal() {
  String filename = "Infrared";
  int index = 1;
  File dir = SD.open(irCurrentDir);
  if (!dir) {
    Serial.println(F("Failed to open /Infrared directory"));
    return false;
  }
  dir.close();
  SD.mkdir(irCurrentDir);
  while (SD.exists(irCurrentDir + "/" + filename + "_" + String(index) + ".ir")) {
    index++;
  }
  filename = irCurrentDir + "/" + filename + "_" + String(index) + ".ir";
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.print(F("Failed to create file: "));
    Serial.println(filename);
    return false;
  }
  file.println("Filetype: IR signals file");
  file.println("Version: 1");
  file.println("#");
  file.print(strDeviceContent);
  file.close();
  Serial.print(F("File saved: "));
  Serial.println(filename);
  return true;
}

// Handle IR submenu
void handleIRSubmenu() {
  static bool irInitialized = false;
  static unsigned long sendStartTime = 0; // Track start time of IR sending

  if (!irInitialized) {
    initIR();
    irInitialized = true;
    buttonUp.setDebounce(50); // Debounce 50ms
    buttonDown.setDebounce(50);
    buttonOK.setDebounce(50);
    buttonBack.setDebounce(50);
    buttonUp.setTimeout(150); // Reduced to 150ms for responsiveness
    buttonDown.setTimeout(150);
    buttonOK.setTimeout(500); // Set hold timeout to 500ms for save
    buttonBack.setTimeout(500); // Set hold timeout to 500ms for delete confirmation
    Serial.println(F("IR submenu initialized"));
  }

  // Enable/disable IR receiver as needed
  static bool irReceiverEnabled = false;
  if (state == IR_READING && !irReceiverEnabled) {
    irrecv.enableIRIn();
    irReceiverEnabled = true;
    Serial.println(F("IR receiver enabled"));
  } else if (state != IR_READING && irReceiverEnabled) {
    irrecv.disableIRIn();
    irReceiverEnabled = false;
    Serial.println(F("IR receiver disabled"));
  }

  buttonUp.tick();
  buttonDown.tick();
  buttonOK.tick();
  buttonBack.tick();

  // Ensure irMenuIndex stays within bounds
  if (irMenuIndex >= IR_MENU_ITEM_COUNT) {
    irMenuIndex = 0;
    Serial.println(F("Reset irMenuIndex to 0"));
  }

  if (state == IR_SELECTION) {
    static byte lastMenuIndex = 255;
    if (irMenuIndex != lastMenuIndex) {
      displayIRSelection(irMenuIndex, irMenuIndex == 0 ? irSelectedSignal : "");
      lastMenuIndex = irMenuIndex;
      Serial.print(F("IR selection displayed, index: "));
      Serial.println(irMenuIndex);
    }
    if (buttonOK.isClick()) {
      if (irMenuIndex == 2 || irMenuIndex == 3 || irMenuIndex == 4) {
        switch (irMenuIndex) {
          case 2: // PJ-OFF
            currentSignals = pjOffSignals;
            currentSignalCount = numPJSignals;
            state = SENDING_IR;
            signalIndex = 0;
            lastSendTime = 0;
            Serial.println(F("Starting PJ-OFF IR attack (NEC)"));
            drawSendingScreen(0);
            break;
          case 3: // AC-OFF
            currentSignals = acOffSignals;
            currentSignalCount = numACSignals;
            state = SENDING_IR;
            signalIndex = 0;
            lastSendTime = 0;
            Serial.println(F("Starting AC-OFF IR attack (NEC)"));
            drawSendingScreen(0);
            break;
          case 4: // TV-OFF
            currentSignals = tvOffSignals;
            currentSignalCount = numTVSignals;
            state = SENDING_IR;
            signalIndex = 0;
            lastSendTime = 0;
            Serial.println(F("Starting TV-OFF IR attack (NEC)"));
            drawSendingScreen(0);
            break;
        }
      } else if (irMenuIndex == 0) {
        state = SENDING_IR;
        lastSendTime = 0;
        sendStartTime = millis(); // Start 3-second timer
        Serial.print(F("Starting IR-Send for signal: "));
        Serial.println(irSelectedSignal);
        drawSendingScreen(0);
      }
    }
    if (buttonBack.isClick()) {
      if (irMenuIndex == 0) {
        state = IR_SIGNAL_SUBMENU;
        display.clearDisplay();
        drawSignalSubmenu();
        Serial.println(F("Back to signal submenu from IR selection"));
      } else {
        inIRMenu = true;
        state = MENU;
        display.clearDisplay();
        displayIRMenu(display, irMenuIndex);
        display.display(); // Ensure display is updated
        delay(10); // Small delay to ensure display refresh
        Serial.print(F("Back to IR submenu from IR selection, irMenuIndex: "));
        Serial.println(irMenuIndex);
      }
    }
  } else if (state == SENDING_IR) {
    unsigned long currentMillis = millis();
    if (irMenuIndex == 2 || irMenuIndex == 3 || irMenuIndex == 4) {
      if (signalIndex >= currentSignalCount) {
        state = IR_SELECTION;
        signalIndex = 0;
        displayIRSelection(irMenuIndex);
        Serial.println(F("IR sending completed"));
        return;
      }
      if (currentMillis - lastSendTime >= 15) {
        lastSendTime = currentMillis;
        uint32_t signal = currentSignals[signalIndex];
        irsend.sendNEC(signal, 32);
        Serial.print(F("IR signal sent: 0x"));
        Serial.println(signal, HEX);
        signalIndex++;
        int progress = map(signalIndex, 0, currentSignalCount, 0, 100);
        static int lastProgress = -1;
        if (progress != lastProgress) {
          drawSendingScreen(progress);
          lastProgress = progress;
        }
      }
      if (buttonOK.isClick() || buttonBack.isClick()) {
        state = IR_SELECTION;
        signalIndex = 0;
        displayIRSelection(irMenuIndex);
        Serial.println(F("IR sending stopped by user"));
      }
    } else if (irMenuIndex == 0) {
      if (buttonBack.isClick()) {
        state = IR_SIGNAL_SUBMENU;
        display.clearDisplay();
        drawSignalSubmenu();
        Serial.println(F("Back to signal submenu from IR sending"));
        return;
      }
      if (buttonOK.isClick()) {
        sendStartTime = millis(); // Restart 3-second timer on OK
        Serial.println(F("Restarting 3-second IR-Send on OK"));
        drawSendingScreen(0);
      }
      if (currentMillis - sendStartTime >= 3000) { // Stop after 3 seconds
        state = IR_SIGNAL_SUBMENU;
        display.clearDisplay();
        drawSignalSubmenu();
        Serial.println(F("IR sending stopped after 3 seconds"));
        return;
      }
      if (currentMillis - lastSendTime >= 15) {
        lastSendTime = currentMillis;
        if (sendIRSignal(irSelectedFile, irSignalIndex)) {
          Serial.print(F("IR signal sent: "));
          Serial.println(irSelectedSignal);
          drawSendingScreen(0); // Update screen to show sending
        } else {
          Serial.println(F("Failed to send IR signal"));
          state = IR_SIGNAL_SUBMENU;
          display.clearDisplay();
          drawSignalSubmenu();
          return;
        }
      }
    }
  } else if (state == IR_SIGNAL_SUBMENU) {
    static int lastSignalIndex = -1;
    if (irSignalIndex != lastSignalIndex) {
      drawSignalSubmenu();
      lastSignalIndex = irSignalIndex;
    }
    if (buttonUp.isClick()) {
      irSignalIndex = (irSignalIndex == 0) ? (irSignalCount - 1) : irSignalIndex - 1;
      Serial.println(F("Signal submenu Up"));
    }
    if (buttonDown.isClick()) {
      irSignalIndex = (irSignalIndex == irSignalCount - 1) ? 0 : irSignalIndex + 1;
      Serial.println(F("Signal submenu Down"));
    }
    if (buttonOK.isClick() && irSignalCount > 0) {
      irSelectedSignal = irSignalList[irSignalIndex];
      state = IR_SELECTION;
      displayIRSelection(irMenuIndex, irSelectedSignal);
      Serial.print(F("Selected signal: "));
      Serial.println(irSelectedSignal);
    }
    if (buttonBack.isClick()) {
      state = IR_FILE_EXPLORER;
      irInFileExplorer = true;
      irSignalCount = 0;
      irSignalIndex = 0;
      irSelectedSignal = "";
      display.clearDisplay();
      drawFileExplorer();
      Serial.println(F("Back to file explorer from signal submenu"));
    }
    yield(); // Allow other tasks to run
  } else if (state == IR_FILE_EXPLORER && irInFileExplorer && !irInDeleteConfirm) {
    static int lastFileIndex = -1;
    if (irFileIndex != lastFileIndex) {
      drawFileExplorer();
      lastFileIndex = irFileIndex;
    }
    if (buttonUp.isClick()) {
      irFileIndex = (irFileIndex == 0) ? (irFileCount - 1) : irFileIndex - 1;
      Serial.println(F("File explorer Up"));
    }
    if (buttonDown.isClick()) {
      irFileIndex = (irFileIndex == irFileCount - 1) ? 0 : irFileIndex + 1;
      Serial.println(F("File explorer Down"));
    }
    if (buttonOK.isClick() && irFileCount > 0) {
      if (irFileList[irFileIndex].isDir) {
        irCurrentDir += "/" + irFileList[irFileIndex].name;
        irFileIndex = 0;
        irFileCount = 0;
        loadIRFileList();
        display.clearDisplay();
        drawFileExplorer();
        Serial.print(F("Entered directory: "));
        Serial.println(irCurrentDir);
      } else {
        irInFileExplorer = false;
        irInDeleteConfirm = false;
        irSelectedFile = irFileList[irFileIndex].name;
        irSignalIndex = 0;
        irSignalCount = 0;
        loadIRSignals(irSelectedFile);
        state = IR_SIGNAL_SUBMENU;
        display.clearDisplay();
        drawSignalSubmenu();
        Serial.print(F("Selected IR file: "));
        Serial.println(irSelectedFile);
      }
    }
    if (buttonBack.isHold()) {
      irInDeleteConfirm = true;
      display.clearDisplay();
      drawDeleteConfirm();
      Serial.println(F("Entered delete confirmation"));
    }
    if (buttonBack.isClick()) {
      if (irCurrentDir != "/Infrared") {
        int lastSlash = irCurrentDir.lastIndexOf('/');
        irCurrentDir = irCurrentDir.substring(0, lastSlash);
        if (irCurrentDir == "") irCurrentDir = "/Infrared";
        irFileIndex = 0;
        irFileCount = 0;
        loadIRFileList();
        display.clearDisplay();
        drawFileExplorer();
        Serial.print(F("Navigated up to directory: "));
        Serial.println(irCurrentDir);
      } else {
        inIRMenu = false;
        state = MENU;
        inMenu = true;
        currentMenu = 3; // index of Infrared tile
        irInFileExplorer = false;
        irInDeleteConfirm = false;
        irSelectedFile = "";
        display.clearDisplay();
        OLED_printMenu(display, currentMenu);
        display.display(); // Ensure display is updated
        delay(10); // Small delay to ensure display refresh
        Serial.println(F("Back to MAIN MENU from IR-Send file explorer"));
      }
    }
    yield(); // Allow other tasks to run
  } else if (state == IR_FILE_EXPLORER && irInFileExplorer && irInDeleteConfirm) {
    if (buttonOK.isClick()) {
      String filePath = irCurrentDir + "/" + irFileList[irFileIndex].name;
      if (SD.remove(filePath)) {
        display.clearDisplay();
        display.drawBitmap(5, 2, image_DolphinMafia_bits, 119, 62, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setTextWrap(false);
        display.setCursor(84, 15);
        display.print(F("Deleted"));
        display.display();
        Serial.print(F("File deleted: "));
        Serial.println(filePath);
        delay(1000);
        irFileCount = 0;
        irFileIndex = 0;
        loadIRFileList();
        irInDeleteConfirm = false;
        display.clearDisplay();
        drawFileExplorer();
      } else {
        display.clearDisplay();
        display.setTextColor(SH110X_WHITE);
        display.setTextWrap(false);
        display.setCursor(0, 0);
        display.print(F("Failed to delete"));
        display.display();
        Serial.print(F("Failed to delete file: "));
        Serial.println(filePath);
        delay(1000);
        irInDeleteConfirm = false;
        display.clearDisplay();
        drawFileExplorer();
      }
    }
    if (buttonBack.isClick()) {
      irInDeleteConfirm = false;
      display.clearDisplay();
      drawFileExplorer();
      Serial.println(F("Cancelled file deletion"));
    }
  } else if (state == IR_READING) {
    static int lastSignalRead = -1;
    if (irrecv.decode(&results)) {
      readSignal = true;
      signalsRead++;
      if (signalsRead <= 20) { // Limit to 20 signals
        appendToDeviceContent("Signal " + String(signalsRead));
        Serial.println(F("IR signal captured and stored"));
      } else {
        Serial.println(F("IR signal captured but not stored (limit of 20 signals reached)"));
      }
      irrecv.resume();
    }
    if (signalsRead != lastSignalRead) {
      drawReadingScreen();
      lastSignalRead = signalsRead;
      readSignal = false; // Reset to allow capturing the next signal
    }
    if (buttonOK.isHold()) {
      if (signalsRead > 0) {
        if (saveIRSignal()) {
          drawSaveConfirm();
          delay(1000);
          signalsRead = 0;
          strDeviceContent = "";
          readSignal = false;
          irrecv.resume();
          drawReadingScreen();
          Serial.println(F("IR signal saved"));
        } else {
          display.clearDisplay();
          display.setTextColor(SH110X_WHITE);
          display.setTextWrap(false);
          display.setCursor(0, 0);
          display.print(F("Failed to save"));
          display.display();
          delay(1000);
          readSignal = false;
          irrecv.resume();
          drawReadingScreen();
          Serial.println(F("Failed to save IR signal"));
        }
      }
    }
    if (buttonBack.isClick()) {
      inIRMenu = true;
      state = MENU;
      readSignal = false;
      signalsRead = 0;
      strDeviceContent = "";
      irrecv.disableIRIn();
      irReceiverEnabled = false;
      display.clearDisplay();
      displayIRMenu(display, irMenuIndex);
      display.display(); // Ensure display is updated
      delay(10); // Small delay to ensure display refresh
      Serial.println(F("Back to IR submenu from IR-Read"));
    }
    yield(); // Allow other tasks to run
  } else {
    if (!inIRMenu) {
      inIRMenu = true;
      display.clearDisplay();
      displayIRMenu(display, irMenuIndex);
      display.display(); // Ensure display is updated
      delay(10); // Small delay to ensure display refresh
      Serial.println(F("Entered IR submenu"));
    }
    static byte lastMenuIndex = 255;
    if (irMenuIndex != lastMenuIndex) {
      display.clearDisplay();
      displayIRMenu(display, irMenuIndex);
      display.display(); // Ensure display is updated
      delay(10); // Small delay to ensure display refresh
      lastMenuIndex = irMenuIndex;
      Serial.print(F("IR main menu displayed, index: "));
      Serial.println(irMenuIndex);
    }
    if (buttonUp.isClick()) {
      irMenuIndex = (irMenuIndex - 1 + IR_MENU_ITEM_COUNT) % IR_MENU_ITEM_COUNT;
      Serial.println(F("IR menu Up"));
    }
    if (buttonDown.isClick()) {
      irMenuIndex = (irMenuIndex + 1) % IR_MENU_ITEM_COUNT;
      Serial.println(F("IR menu Down"));
    }
    if (buttonOK.isClick()) {
      switch (irMenuIndex) {
        case 0: // IR-Send
          state = IR_FILE_EXPLORER;
          irInFileExplorer = true;
          irFileIndex = 0;
          irFileCount = 0;
          irCurrentDir = "/Infrared"; // Reset to root directory
          loadIRFileList();
          display.clearDisplay();
          drawFileExplorer();
          Serial.println(F("Entered IR-Send file explorer"));
          break;
        case 1: // IR-Read
          state = IR_READING;
          readSignal = false;
          signalsRead = 0;
          strDeviceContent = "";
          display.clearDisplay();
          drawReadingScreen();
          Serial.println(F("Entered IR-Read mode"));
          break;
        case 2: // PJ-OFF
        case 3: // AC-OFF
        case 4: // TV-OFF
          state = IR_SELECTION;
          Serial.println(F("Entered IR selection"));
          display.clearDisplay();
          displayIRSelection(irMenuIndex);
          break;
        default:
          inIRMenu = true;
          state = MENU;
          display.clearDisplay();
          displayIRMenu(display, irMenuIndex);
          display.display(); // Ensure display is updated
          delay(10); // Small delay to ensure display refresh
          Serial.println(F("Invalid IR menu option, staying in IR submenu"));
          break;
      }
    }
    if (buttonBack.isClick()) {
      inIRMenu = false;
      state = MENU;
      inMenu = true;
      currentMenu = 3; // Return to main menu with IR selected
      display.clearDisplay();
      OLED_printMenu(display, currentMenu);
      display.display(); // Ensure display is updated
      delay(10); // Small delay to ensure display refresh
      Serial.println(F("Back to main menu with IR selected"));
    }
  }
}