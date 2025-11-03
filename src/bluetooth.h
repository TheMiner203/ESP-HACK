#ifndef BLUETOOTH_MENU_H
#define BLUETOOTH_MENU_H

#include <Adafruit_SH110X.h>
#include "CONFIG.h"

#define BLUETOOTH_MENU_ITEM_COUNT 4
static const char* bluetoothMenuItems[] = {"IOS-SPM", "ANDR-SPM", "WIN-SPM", "BadBLE"};

void displayBluetoothMenu(Adafruit_SH1106G &display, byte menuIndex);

#endif
