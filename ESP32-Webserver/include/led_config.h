#pragma once

#include <Adafruit_NeoPixel.h>

// ============================================================================
// LED-Typen
// ============================================================================

/**
 * @brief Supported LED chip families.
 */
enum LedType {
    LED_TYPE_WS2812B,
    LED_TYPE_SK6812_RGB,
    LED_TYPE_SK6812_RGBW
};

// ============================================================================
// LED-Farbreihenfolge
//
// Currently supported orders:
//
//   GRB  -> WS2812B / SK6812 RGB
//   RGB  -> RGB-LEDs mit RGB-Reihenfolge
//   GRBW -> SK6812 RGBW
// ============================================================================

/**
 * @brief Color order used by the addressable LEDs.
 */
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
// Type checking
// ============================================================================

/**
 * @brief Checks whether the configured LED type uses an RGBW output format.
 * @return true for RGBW-capable strip types, otherwise false.
 */
constexpr bool ledTypeIsRgbw()
{
    return LED_TYPE == LED_TYPE_SK6812_RGBW;
}


/**
 * @brief Checks whether the configured LED order is an RGBW variant.
 * @return true if the order includes a white channel, otherwise false.
 */
constexpr bool ledOrderIsRgbw()
{
    return LED_ORDER == LED_ORDER_GRBW;
}

static_assert(
    ledTypeIsRgbw() == ledOrderIsRgbw(),
    "LED_TYPE and LED_ORDER are incompatible"
);

// ============================================================================
// Runtime: create the NeoPixel type from the configuration
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