#ifndef SUBGHZ_H
#define SUBGHZ_H

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <GyverButton.h>
#include <EEPROM.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>
#include "CONFIG.h"

// Shared global variables
extern Adafruit_SH1106G display;
extern GButton buttonUp;
extern GButton buttonDown;
extern GButton buttonOK;
extern GButton buttonBack;

// SubGHz enums and structures
enum emKeys { kUnknown, kP12bt, k12bt, k24bt, k64bt, kKeeLoq, kANmotors64, kPrinceton, kRcSwitch, kStarLine, kCAME, kNICE, kHOLTEK };
enum emMenuState { menuMain, menuReceive, menuTransmit, menuAnalyzer, menuJammer };

struct tpKeyData {
  byte keyID[9];
  int zero[2];
  int one[2];
  int prePulse[2];
  int startPause[2];
  int midlePause[2];
  byte prePulseLenth;
  byte codeLenth;
  byte firstDataIdx;
  emKeys type;
  float frequency;
  int te;
  char rawData[16];
  int bitLength;
  char preset[8];
};

// Function prototype for SubGHz entry point
void runSubGHz();

#endif
