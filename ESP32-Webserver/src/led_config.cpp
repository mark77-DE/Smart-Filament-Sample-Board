#include "led_config.h"
#include "config.h"

// ============================================================================
// NeoPixel-Typ aus Konfiguration erzeugen
// ============================================================================

neoPixelType getNeoPixelType(LedType type, LedOrder order)
{
    // RGBW
    if (type == LED_TYPE_SK6812_RGBW)
    {
        return NEO_GRBW | NEO_KHZ800;
    }

    // RGB
    switch (order)
    {
        case LED_ORDER_RGB:
            return NEO_RGB | NEO_KHZ800;

        case LED_ORDER_GRB:
        default:
            return NEO_GRB | NEO_KHZ800;
    }
}

// ============================================================================
// Gemeinsame Farbfunktion
// ============================================================================

uint32_t ledColor(
    Adafruit_NeoPixel* strip,
    uint8_t r,
    uint8_t g,
    uint8_t b
)
{
    if (!strip)
        return 0;

    if (CONFIGV2.ledHardware.type == LED_TYPE_SK6812_RGBW)
    {
        return strip->Color(r, g, b, 0);
    }

    return strip->Color(r, g, b);
}