#if DISPLAY_TYPE == DISPLAY_TYPE_GC9A01

#include "display/display.h"
#include <Adafruit_GC9A01A.h>
#include <Adafruit_GFX.h>
#include <SPI.h>

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

void displayInit()
{
    tft.begin();
    tft.fillScreen(GC9A01A_BLACK);
}

void displayLoop()
{
}

void displayShowIP(const String &ip)
{
    tft.setCursor(0,0);
    tft.setTextColor(GC9A01A_WHITE);
    tft.setTextSize(2);
    tft.println(ip);
}

void displayMessage(const String &msg)
{
    tft.fillScreen(GC9A01A_BLACK);
    tft.setCursor(0,0);
    tft.println(msg);
}

#endif