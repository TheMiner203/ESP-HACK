#ifndef SUBGHZ_H
#define SUBGHZ_H

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <GyverButton.h>
#include <EEPROM.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>
#include "CONFIG.h"


#define SUBGHZ_MENU_ITEM_COUNT 4
static const char* subghzMenuItems[] = {"SubRead", "SubSend", "Analyzer", "Jammer"};


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
void setupCC1101();
void configureCC1101();
void restoreReceiveMode();
void read_rcswitch(tpKeyData* kd);
void read_raw(tpKeyData* kd);
void OLED_printWaitingSignal();
void OLED_printKey(tpKeyData* kd, String fileName = "", bool isSending = false);
void OLED_printError(String st, bool err = true);
bool saveKeyToSD(tpKeyData* kd);
bool loadKeyFromSD(String fileName, tpKeyData* kd);
void sendSynthKey(tpKeyData* kd);
void RCSwitch_send(uint64_t data, unsigned int bits, int pulse, int protocol, int repeat);
void RCSwitch_RAW_send(int *ptrtransmittimings);
void myDelayMcs(unsigned long dl);
void OLED_printAnalyzer(bool signalReceived = false, float detectedFreq = 0.0);
void OLED_printJammer();
void OLED_printFileExplorer();
void OLED_printDeleteConfirm();
void startJamming();
void stopJamming();
String getTypeName(emKeys tp);
void loadFileList();
bool initRfModule(String mode, float freq);
void deinitRfModule();
void OLED_printSubGHzMenu(Adafruit_SH1106G &display, byte menuIndex);
#endif
