#ifndef DISPLAY_H
#define DISPLAY_H
void OLED_printLogo(Adafruit_SH1106G &display);

void OLED_printMenu(Adafruit_SH1106G &display, byte menuIndex);
#endif