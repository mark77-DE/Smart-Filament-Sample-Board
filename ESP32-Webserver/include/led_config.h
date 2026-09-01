#pragma once

#include <Adafruit_NeoPixel.h>

// ============================================================================
// LED-Typen
// ============================================================================

enum LedType {
    LED_TYPE_WS2812B,
    LED_TYPE_SK6812_RGB,
    LED_TYPE_SK6812_RGBW
};

// ============================================================================
// LED-Farbreihenfolge
//
// Aktuell unterstützte Reihenfolgen:
//
//   GRB  -> WS2812B / SK6812 RGB
//   RGB  -> RGB-LEDs mit RGB-Reihenfolge
//   GRBW -> SK6812 RGBW
// ============================================================================

enum LedOrder {
    LED_ORDER_GRB,
    LED_ORDER_GBR,
    LED_ORDER_RGB,
    LED_ORDER_RBG,
    LED_ORDER_BRG,
    LED_ORDER_BGR,

    LED_ORDER_GRBW,
    LED_ORDER_GBRW,
    LED_ORDER_RGBW,
    LED_ORDER_RBGW,
    LED_ORDER_BRGW,
    LED_ORDER_BGRW
};

// ============================================================================
// Defaults
// ============================================================================

#ifndef LED_TYPE
#define LED_TYPE LED_TYPE_WS2812B
#endif

#ifndef LED_ORDER
#define LED_ORDER LED_ORDER_GRB
#endif

// ============================================================================
// Typprüfung
// ============================================================================

constexpr bool ledTypeIsRgbw()
{
    return LED_TYPE == LED_TYPE_SK6812_RGBW;
}

constexpr bool ledOrderIsRgbw()
{
    return LED_ORDER == LED_ORDER_GRBW;
}

static_assert(
    ledTypeIsRgbw() == ledOrderIsRgbw(),
    "LED_TYPE and LED_ORDER are incompatible"
);

// ============================================================================
// Runtime: NeoPixel-Typ aus Konfiguration erzeugen
// ============================================================================

neoPixelType getNeoPixelType(LedType type, LedOrder order);

// ============================================================================
// Gemeinsame Farbfunktion
// ============================================================================

uint32_t ledColor(
    Adafruit_NeoPixel* strip,
    uint8_t r,
    uint8_t g,
    uint8_t b
);