#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "display.h"
#include "interface.h"

void OLED_printLogo(Adafruit_SH1106G &display) {
  display.clearDisplay();
  display.drawBitmap(0, 0, image_ESPHACK_bits, 128, 64, 1);
  display.setTextColor(1);
  display.setTextWrap(false);
  display.setCursor(2, 2);
  display.print("v0.1");
  display.display();
}

// Main menu
void OLED_printMenu(Adafruit_SH1106G &display, byte menuIndex) {
  display.clearDisplay();

  if (menuIndex == 0) { // WiFi
    display.drawBitmap(-35, 2, image_WiFi_bits, 128, 64, 1);
    display.drawBitmap(122, 12, image_DOTsel_bits, 4, 4, 1);
    display.drawBitmap(122, 21, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 30, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 39, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 48, image_DOT_bits, 4, 4, 1);
    display.setTextColor(1);
    display.setTextSize(2);
    display.setTextWrap(false);
    display.setCursor(58, 25);
    display.print("WiFi");
  } else if (menuIndex == 1) { // Bluetooth
    display.drawBitmap(-38, 6, image_Bluetooth_bits, 128, 64, 1);
    display.drawBitmap(122, 12, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 21, image_DOTsel_bits, 4, 4, 1);
    display.drawBitmap(122, 30, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 39, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 48, image_DOT_bits, 4, 4, 1);
    display.setTextColor(1);
    display.setTextSize(2);
    display.setTextWrap(false);
    display.setCursor(58, 25);
    display.print("BLE");
  } else if (menuIndex == 2) { // SubGHz
    display.drawBitmap(5, 14, image_SubGHzsubghz_bits, 39, 39, 1);
    display.drawBitmap(122, 12, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 21, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 30, image_DOTsel_bits, 4, 4, 1);
    display.drawBitmap(122, 39, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 48, image_DOT_bits, 4, 4, 1);
    display.setTextColor(1);
    display.setTextSize(2);
    display.setTextWrap(false);
    display.setCursor(48, 25);
    display.print("SubGHz");
  } else if (menuIndex == 3) { // Infrared
    display.drawBitmap(-37, 6, image_IR_bits, 128, 64, 1);
    display.drawBitmap(122, 12, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 21, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 30, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 39, image_DOTsel_bits, 4, 4, 1);
    display.drawBitmap(122, 48, image_DOT_bits, 4, 4, 1);
    display.setTextColor(1);
    display.setTextSize(2);
    display.setTextWrap(false);
    display.setCursor(65, 25);
    display.print("IR");
  } else if (menuIndex == 4) { // GPIO
    display.drawBitmap(-34, 6, image_GPIOgpio_bits, 128, 64, 1);
    display.drawBitmap(122, 12, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 21, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 30, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 39, image_DOT_bits, 4, 4, 1);
    display.drawBitmap(122, 48, image_DOTsel_bits, 4, 4, 1);
    display.setTextColor(1);
    display.setTextSize(2);
    display.setTextWrap(false);
    display.setCursor(58, 25);
    display.print("GPIO");
  }

  display.drawBitmap(121, 58, image_ARROWdwn_bits, 6, 3, 1);
  display.drawBitmap(121, 3, image_ARROWup_bits, 6, 3, 1);

  display.display();
}