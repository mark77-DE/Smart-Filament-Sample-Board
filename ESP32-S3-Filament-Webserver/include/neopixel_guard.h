#pragma once
#include <Adafruit_NeoPixel.h>

// Optional: genau EINE definieren (oder keine -> Blocking Default)
// #define NEOPIXEL_SHOW_DIRECT      // kein Mutex, keine waits -> max Speed, unsicher bei Multi-Strip
// #define NEOPIXEL_SHOW_NONBLOCK    // non-blocking: sendet nur, wenn sofort möglich

#ifdef __cplusplus
extern "C" {
#endif

// Blocking & threadsafe: global serialisiert show() für ALLE Streifen
void neopixelShowSafe(Adafruit_NeoPixel* strip);

// Non-blocking: nur wenn sofort möglich; sonst false
bool neopixelTryShow(Adafruit_NeoPixel* strip);

// Atomisch: sendet A dann B unter EINEM globalen Lock (für „ein Frame“ auf zwei Stripes)
void neopixelShowPairSafe(Adafruit_NeoPixel* a, Adafruit_NeoPixel* b);

#ifdef __cplusplus
}
#endif
