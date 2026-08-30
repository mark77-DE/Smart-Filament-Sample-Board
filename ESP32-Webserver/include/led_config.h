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
// RGB:
//   RGB, RBG, GRB, GBR, BRG, BGR
//
// RGBW:
//   WRGB, WRBG, WGRB, WGBR, WBRG, WBGR,
//   RWGB, RWBG, RGWB, RGBW, RBWG, RBGW,
//   GWRB, GWBR, GRWB, GRBW, GBWR, GBRW,
//   BWRG, BWGR, BRWG, BRGW, BGWR, BGRW
// ============================================================================

enum LedOrder {
    // RGB
    LED_ORDER_RGB,
    LED_ORDER_RBG,
    LED_ORDER_GRB,
    LED_ORDER_GBR,
    LED_ORDER_BRG,
    LED_ORDER_BGR,

    // RGBW
    LED_ORDER_WRGB,
    LED_ORDER_WRBG,
    LED_ORDER_WGRB,
    LED_ORDER_WGBR,
    LED_ORDER_WBRG,
    LED_ORDER_WBGR,

    LED_ORDER_RWGB,
    LED_ORDER_RWBG,
    LED_ORDER_RGWB,
    LED_ORDER_RGBW,
    LED_ORDER_RBWG,
    LED_ORDER_RBGW,

    LED_ORDER_GWRB,
    LED_ORDER_GWBR,
    LED_ORDER_GRWB,
    LED_ORDER_GRBW,
    LED_ORDER_GBWR,
    LED_ORDER_GBRW,

    LED_ORDER_BWRG,
    LED_ORDER_BWGR,
    LED_ORDER_BRWG,
    LED_ORDER_BRGW,
    LED_ORDER_BGWR,
    LED_ORDER_BGRW
};

// ============================================================================
// Defaults
//
// Falls die Defines nicht in platformio.ini gesetzt sind, bleibt das bisherige
// Verhalten aktiv.
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
    switch (LED_ORDER) {
        case LED_ORDER_WRGB:
        case LED_ORDER_WRBG:
        case LED_ORDER_WGRB:
        case LED_ORDER_WGBR:
        case LED_ORDER_WBRG:
        case LED_ORDER_WBGR:
        case LED_ORDER_RWGB:
        case LED_ORDER_RWBG:
        case LED_ORDER_RGWB:
        case LED_ORDER_RGBW:
        case LED_ORDER_RBWG:
        case LED_ORDER_RBGW:
        case LED_ORDER_GWRB:
        case LED_ORDER_GWBR:
        case LED_ORDER_GRWB:
        case LED_ORDER_GRBW:
        case LED_ORDER_GBWR:
        case LED_ORDER_GBRW:
        case LED_ORDER_BWRG:
        case LED_ORDER_BWGR:
        case LED_ORDER_BRWG:
        case LED_ORDER_BRGW:
        case LED_ORDER_BGWR:
        case LED_ORDER_BGRW:
            return true;

        default:
            return false;
    }
}

// Ein RGBW-Typ benötigt zwingend eine RGBW-Reihenfolge.
// Ein RGB-Typ benötigt eine RGB-Reihenfolge.
//
// Dadurch führt eine versehentliche Kombination wie
//
//   LED_TYPE=LED_TYPE_SK6812_RGBW
//   LED_ORDER=LED_ORDER_GRB
//
// bereits beim Build zu einem klaren Fehler.
static_assert(
    ledTypeIsRgbw() == ledOrderIsRgbw(),
    "LED_TYPE and LED_ORDER are incompatible"
);

// ============================================================================
// Adafruit-NeoPixel-Typ aus LED_ORDER erzeugen
// ============================================================================

constexpr neoPixelType getNeoPixelOrder()
{
    switch (LED_ORDER) {

        // ---------------- RGB ----------------

        case LED_ORDER_RGB:
            return NEO_RGB;

        case LED_ORDER_RBG:
            return NEO_RBG;

        case LED_ORDER_GRB:
            return NEO_GRB;

        case LED_ORDER_GBR:
            return NEO_GBR;

        case LED_ORDER_BRG:
            return NEO_BRG;

        case LED_ORDER_BGR:
            return NEO_BGR;

        // ---------------- RGBW ----------------

        case LED_ORDER_WRGB:
            return NEO_WRGB;

        case LED_ORDER_WRBG:
            return NEO_WRBG;

        case LED_ORDER_WGRB:
            return NEO_WGRB;

        case LED_ORDER_WGBR:
            return NEO_WGBR;

        case LED_ORDER_WBRG:
            return NEO_WBRG;

        case LED_ORDER_WBGR:
            return NEO_WBGR;

        case LED_ORDER_RWGB:
            return NEO_RWGB;

        case LED_ORDER_RWBG:
            return NEO_RWBG;

        case LED_ORDER_RGWB:
            return NEO_RGWB;

        case LED_ORDER_RGBW:
            return NEO_RGBW;

        case LED_ORDER_RBWG:
            return NEO_RBWG;

        case LED_ORDER_RBGW:
            return NEO_RBGW;

        case LED_ORDER_GWRB:
            return NEO_GWRB;

        case LED_ORDER_GWBR:
            return NEO_GWBR;

        case LED_ORDER_GRWB:
            return NEO_GRWB;

        case LED_ORDER_GRBW:
            return NEO_GRBW;

        case LED_ORDER_GBWR:
            return NEO_GBWR;

        case LED_ORDER_GBRW:
            return NEO_GBRW;

        case LED_ORDER_BWRG:
            return NEO_BWRG;

        case LED_ORDER_BWGR:
            return NEO_BWGR;

        case LED_ORDER_BRWG:
            return NEO_BRWG;

        case LED_ORDER_BRGW:
            return NEO_BRGW;

        case LED_ORDER_BGWR:
            return NEO_BGWR;

        case LED_ORDER_BGRW:
            return NEO_BGRW;
    }

    // Sollte wegen LED_ORDER eigentlich nie erreicht werden.
    return NEO_GRB;
}

// ============================================================================
// Der von beiden LED-Controllern verwendete NeoPixel-Typ
//
// Wir verwenden zunächst ausschließlich 800 kHz.
// ============================================================================

constexpr neoPixelType LED_NEO_PIXEL_TYPE =
    getNeoPixelOrder() + NEO_KHZ800;
    