#ifndef WIFI_H
#define WIFI_H

#include <Adafruit_SH110X.h>
#include "CONFIG.h"

#define WIFI_MENU_ITEM_COUNT 4
static const char* wifiMenuItems[] = {"Deauther", "Beacon", "Portal", "Wardrvng"};

void displayWiFiMenu(Adafruit_SH1106G &display, byte menuIndex);

void saveCapturedDataToCSV(String csvLine);

#endif