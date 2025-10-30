#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <GyverButton.h>
#include <SD.h>
#include <SPI.h>
#include "CONFIG.h"
#include "interface.h"
#include "subghz.h"
#include "wifi_menu.h"
#include "bluetooth_menu.h"
#include "infrared_menu.h"
#include "gpio_menu.h"

// Function prototypes
void handleWiFiSubmenu();
void handleBluetoothSubmenu();
void handleIRSubmenu();
void handleGPIOSubmenu();

// Global display and button objects
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
GButton buttonUp(BUTTON_UP, HIGH_PULL, NORM_OPEN);
GButton buttonDown(BUTTON_DOWN, HIGH_PULL, NORM_OPEN);
GButton buttonOK(BUTTON_OK, HIGH_PULL, NORM_OPEN);
GButton buttonBack(BUTTON_BACK, HIGH_PULL, NORM_OPEN);

// Custom SPI instance for SD card
SPIClass sdSPI(HSPI); // Use HSPI for SD card

// Menu state
#define MENU_ITEM_COUNT 5
const char* menuItems[] = {"WiFi", "Bluetooth", "SubGHz", "Infrared", "GPIO"};
byte currentMenu = 0;
bool inMenu = true;

// WiFi submenu state
byte wifiMenuIndex = 0;

// Bluetooth submenu state
byte bluetoothMenuIndex = 0;
bool inBadBLE = false;
byte badBLEScriptIndex = 0;
bool scriptSelected = false;
byte selectedScript = 0;

// Infrared submenu state
byte irMenuIndex = 0;
bool inIRSelection = false;
bool inIRAttack = false;

// GPIO submenu state
byte gpioMenuIndex = 0; // Declared globally

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // Wait for Serial to initialize
  }

  // Initialize buttons
  buttonUp.setDebounce(50);
  buttonUp.setTimeout(500);
  buttonUp.setClickTimeout(300);
  buttonUp.setStepTimeout(200);
  buttonDown.setDebounce(50);
  buttonDown.setTimeout(500);
  buttonDown.setClickTimeout(300);
  buttonDown.setStepTimeout(200);
  buttonOK.setDebounce(50);
  buttonOK.setTimeout(500);
  buttonOK.setClickTimeout(300);
  buttonOK.setStepTimeout(200);
  buttonBack.setDebounce(50);
  buttonBack.setTimeout(500);
  buttonBack.setClickTimeout(300);
  buttonBack.setStepTimeout(200);
  buttonUp.setTickMode(AUTO);
  buttonDown.setTickMode(AUTO);

  // Initialize I2C with specified pins
  Wire.begin(OLED_SDA, OLED_SCL);
  
  // Initialize display
  if (!display.begin(OLED_ADR)) {
    Serial.println(F("SH110X allocation failed"));
    for(;;); // Don't proceed, loop forever if display init fails
  }

  // Initialize SD card
  Serial.println(F("Initializing SD card..."));
  sdSPI.begin(SD_CLK, SD_MISO, SD_MOSI); // Initialize custom SPI (CLK: 2, MISO: 0, MOSI: 15)
  sdSPI.setFrequency(4000000); // Set SPI clock to 4 MHz for compatibility
  if (!SD.begin(-1, sdSPI)) { // Pass -1 for CS (ignored, as CS is tied to GND)
    Serial.println(F("SD card initialization failed! Check the following:"));
    Serial.println(F("- Is the SD card inserted properly?"));
    Serial.println(F("- Is the SD card formatted as FAT32?"));
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("SD-Card Init Failed"));
    display.println(F(""));
    display.println(F("ERROR: 0x000"));
    display.display();
    for(;;); // Don't proceed, loop forever if SD init fails
  }
  Serial.println(F("SD card initialized successfully"));

  // Check SD card details
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println(F("No SD card detected"));
  } else if (cardType == CARD_MMC) {
    Serial.println(F("Card type: MMC"));
  } else if (cardType == CARD_SD) {
    Serial.println(F("Card type: SDSC"));
  } else if (cardType == CARD_SDHC) {
    Serial.println(F("Card type: SDHC"));
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024); // Size in MB
  Serial.print(F("SD Card Size: "));
  Serial.print(cardSize);
  Serial.println(F("MB"));

  // Create necessary directories
  if (!SD.exists("/WiFi")) {
    SD.mkdir("/WiFi");
    Serial.println(F("Created WiFi directory"));
  }
  if (!SD.exists("/WiFi/Wardriving")) {
    SD.mkdir("/WiFi/Wardriving");
    Serial.println(F("Created Wardriving directory"));
  }
  if (!SD.exists("/WiFi/Portals")) {
    SD.mkdir("/WiFi/Portals");
    Serial.println(F("Created Wardriving directory"));
  }
  if (!SD.exists("/BadKB")) {
    SD.mkdir("/BadKB");
    Serial.println(F("Created BadKB directory"));
  }
  if (!SD.exists("/SubGHz")) {
    SD.mkdir("/SubGHz");
    Serial.println(F("Created SubGHz directory"));
  }
  if (!SD.exists("/Infrared")) {
    SD.mkdir("/Infrared");
    Serial.println(F("Created Infrared directory"));
  }
  if (!SD.exists("/GPIO")) {
    SD.mkdir("/GPIO");
    Serial.println(F("Created GPIO directory"));
  }

  // Show splash screen
  OLED_printLogo(display);

  // Wait for any button press to proceed
  while (!buttonUp.isClick() && !buttonDown.isClick() && !buttonOK.isClick() && !buttonBack.isClick()) {
    buttonUp.tick();
    buttonDown.tick();
    buttonOK.tick();
    buttonBack.tick();
  }

  // Enter main menu
  OLED_printMenu(display, currentMenu);
}

void loop() {
  // Update buttons
  buttonUp.tick();
  buttonDown.tick();
  buttonOK.tick();
  buttonBack.tick();

  if (inMenu) {
    // Navigate main menu
    if (buttonUp.isClick()) {
      currentMenu = (currentMenu - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
      OLED_printMenu(display, currentMenu);
      Serial.println(F("Main menu Up"));
    }
    if (buttonDown.isClick()) {
      currentMenu = (currentMenu + 1) % MENU_ITEM_COUNT;
      OLED_printMenu(display, currentMenu);
      Serial.println(F("Main menu Down"));
    }
    if (buttonOK.isClick()) {
      Serial.println(F("Main menu OK"));
      inMenu = false; // Exit main menu
      if (currentMenu == 0) {
        // Enter WiFi submenu (Index 0)
        wifiMenuIndex = 0;
        displayWiFiMenu(display, wifiMenuIndex);
        Serial.println(F("Entered WiFi submenu"));
      } else if (currentMenu == 1) {
        // Enter Bluetooth submenu (Index 1)
        bluetoothMenuIndex = 0;
        displayBluetoothMenu(display, bluetoothMenuIndex);
        Serial.println(F("Entered Bluetooth submenu"));
      } else if (currentMenu == 2) {
        // Enter SubGHz submenu (Index 2)
        Serial.println(F("Entering SubGHz menu"));
        runSubGHz();
        // After SubGHz returns, restore main menu
        inMenu = true;
        OLED_printMenu(display, currentMenu);
      } else if (currentMenu == 3) {
        // Enter Infrared submenu (Index 3)
        irMenuIndex = 0;
        displayIRMenu(display, irMenuIndex);
        Serial.println(F("Entered Infrared submenu"));
      } else if (currentMenu == 4) {
        // Enter GPIO submenu (Index 4)
        gpioMenuIndex = 0;
        displayGPIOMenu(display, gpioMenuIndex);
        Serial.println(F("Entered GPIO submenu"));
      }
    }
  } else {
    // Handle submenus
    if (currentMenu == 0) {
      // Handle WiFi submenu
      handleWiFiSubmenu();
    } else if (currentMenu == 1) {
      // Handle Bluetooth submenu
      handleBluetoothSubmenu();
    } else if (currentMenu == 3) {
      // Handle Infrared submenu
      handleIRSubmenu();
    } else if (currentMenu == 4) {
      // Handle GPIO submenu
      handleGPIOSubmenu();
    } else {
      // Handle other submenus (SubGHz)
      if (buttonBack.isClick()) {
        inMenu = true;
        OLED_printMenu(display, currentMenu);
        Serial.println(F("Back to main menu from other submenu"));
      }
    }
  }
}