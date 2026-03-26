#include "display/display_config.h"



#if DISPLAY_TYPE == DISPLAY_TYPE_ST7789

#include "display/st7789/display_st7789.h"  // LGFX bekannt
#include "display/display.h"
#include "display/display_anim.h"

#include <Arduino.h>
#include <math.h>
#include <LovyanGFX.hpp>
#include <cstdint>  // für uint8_t, uint16_t, uint32_t
#include "display/st7789/logoBitmap.h"

#include "update_manager.h"



namespace DisplayAnim {

struct IdleState {
    bool active = false;
    bool textFirst = false;
    String idleText = IDLE_TEXT_STRING;  // Default
    bool imageDrawn = false;       // NEU: Bild nur einmal zeichnen
    bool textDrawn = false;
    size_t highlightIndex = 0;
    unsigned long lastAnimTime = 0;
};



static IdleState state;

// -------------------------
// HSV -> RGB565
// -------------------------
uint16_t hsvTo565(float h, float s=1.0, float v=1.0) {
    float r, g, b;
    int i = int(h*6);
    float f = h*6 - i;
    float p = v*(1 - s);
    float q = v*(1 - f*s);
    float t = v*(1 - (1-f)*s);
    switch(i%6){
        case 0: r=v; g=t; b=p; break;
        case 1: r=q; g=v; b=p; break;
        case 2: r=p; g=v; b=t; break;
        case 3: r=p; g=q; b=v; break;
        case 4: r=t; g=p; b=v; break;
        case 5: r=v; g=p; b=q; break;
    }
    return ((int)(r*31)<<11) | ((int)(g*63)<<5) | ((int)(b*31));
}

// -------------------------
// Start / Stop
// -------------------------
void startIdle(unsigned long now) {
    state.active = true;
    state.textFirst = false;
    state.imageDrawn = false; 
}

void startIdleTextFirst(unsigned long now) {
    state.active = true;
    state.textFirst = true;
    state.imageDrawn = false; 
}

void stop() {
    state.active = false;
}

// -------------------------
// Tick Idle
// -------------------------
static uint32_t lastFrame = 0;

// -------------------------
// Tick Idle + Update Hinweis
// -------------------------
void tickIdle(LGFX &display, unsigned long now) {
    if (!state.active) return;

    // -------------------
    // Bild einmalig zeichnen
    // -------------------
    if (!state.imageDrawn) {
        int logoWidth = 320;
        int logoHeight = 125;
        int x = (display.width() - logoWidth) / 2;
        int y = 0;

        display.fillScreen(TFT_WHITE);
        display.pushImage(x, y, logoWidth, logoHeight, logoBitmap);

        state.imageDrawn = true;
        state.highlightIndex = 0;
        state.lastAnimTime = now;
    }

    // -------------------
    // Haupt-Text-Animation
    // -------------------
    const int logoHeight = 125;
    const int textY = logoHeight + 15;

    display.setTextSize(3);
    display.setTextDatum(TL_DATUM); // Top-Left

    int totalWidth = display.textWidth(state.idleText);
    int startX = (display.width() - totalWidth) / 2;

    const uint32_t animDelay = 150;
    if (now - state.lastAnimTime >= animDelay) {
        state.lastAnimTime = now;

        uint16_t color;
        switch (state.highlightIndex % 3) {
            case 0: color = TFT_RED; break;
            case 1: color = TFT_GREEN; break;
            case 2: color = TFT_BLUE; break;
        }

        String prefix = state.idleText.substring(0, state.highlightIndex);
        String currentChar = state.idleText.substring(state.highlightIndex, state.highlightIndex + 1);
        String suffix = state.idleText.substring(state.highlightIndex + 1);

        int prefixWidth = display.textWidth(prefix);
        int charWidth   = display.textWidth(currentChar);

        display.setTextColor(TFT_BLACK, TFT_WHITE);
        display.drawString(prefix, startX, textY);

        display.setTextColor(color, TFT_WHITE);
        display.drawString(currentChar, startX + prefixWidth, textY);

        display.setTextColor(TFT_BLACK, TFT_WHITE);
        display.drawString(suffix, startX + prefixWidth + charWidth, textY);

        state.highlightIndex++;
        if (state.highlightIndex >= state.idleText.length()) {
            state.highlightIndex = 0;
        }
    }

    // -------------------
    // Update-Hinweis unten rechts
    // -------------------
    const UpdateInfo &info = getUpdateInfo();  // globale Struktur abfragen
    if (info.updateAvailable) {
        const String updateText = "UPDATE";
        display.setTextSize(2);
        display.setTextColor(TFT_RED, TFT_YELLOW); // rot auf gelb

        int x = display.width() - display.textWidth(updateText) - 5; // 5px Padding rechts
        int y = display.height() - 26;                                // 10px vom unteren Rand

        display.drawString(updateText, x, y);
    }
}

// Typewriter Animation
void playThreeLineTypewriter(
    LGFX& display,
    const String& line1,
    const String& line2,
    const String& line3,
    uint32_t charDelayMs,
    uint32_t linePauseMs,
    uint32_t endHoldMs,
    bool eraseBackwards,
    uint32_t eraseCharDelayMs,
    uint32_t eraseLinePauseMs
) {
    const String lines[3] = {line1, line2, line3};
    const int totalLines = 3;
    const int lineHeight = 26;
    const int centerY = (display.height() - totalLines * lineHeight) / 2;

    display.setTextColor(TFT_WHITE, TFT_BLACK); // Textfarbe und Hintergrund
    display.setTextDatum(TL_DATUM);            // Links oben als Referenz

    int lineX[totalLines]; // X-Positionen merken

    // =====================
    // Typwriter-Effekt einblenden
    // =====================
    for (int i = 0; i < totalLines; i++) {
        int y = centerY + i * lineHeight;

        for (uint16_t c = 0; c < lines[i].length(); c++) {
            String displayText = lines[i].substring(0, c + 1);
            int textWidth = display.textWidth(displayText.c_str());
            int x = (display.width() - textWidth) / 2;

            lineX[i] = x;

            display.fillRect(0, y, display.width(), lineHeight, TFT_BLACK);
            display.setCursor(x, y);
            display.print(displayText);

            delay(charDelayMs);
            yield();
        }
        delay(linePauseMs);
    }

    delay(endHoldMs);

    // =====================
    // Scroll nach oben bis komplett weg
    // =====================
    if (eraseBackwards) {
    for (int i = totalLines - 1; i >= 0; i--) {
        int y = centerY + i * lineHeight;

        // von voller Länge runterzählen
        for (int c = lines[i].length(); c >= 0; c--) {
            String displayText = lines[i].substring(0, c);

            int textWidth = display.textWidth(displayText.c_str());
            int x = (display.width() - textWidth) / 2;

            // Zeile sauber löschen
            display.fillRect(0, y, display.width(), lineHeight, TFT_BLACK);

            // gekürzten Text neu zeichnen
            display.setCursor(x, y);
            display.print(displayText);

            delay(eraseCharDelayMs);
            yield();
        }

        delay(eraseLinePauseMs);
    }

    // final sicher schwarz
    display.fillRect(0, 0, display.width(), display.height(), TFT_BLACK);
}
}

// PROGMEM-Overload
void playThreeLineTypewriter(
    DisplayType& display,
    const __FlashStringHelper* line1,
    const __FlashStringHelper* line2,
    const __FlashStringHelper* line3,
    uint32_t charDelayMs,
    uint32_t linePauseMs,
    uint32_t endHoldMs,
    bool eraseBackwards,
    uint32_t eraseCharDelayMs,
    uint32_t eraseLinePauseMs
) {
    playThreeLineTypewriter(
        display,
        String(line1),
        String(line2),
        String(line3),
        charDelayMs,
        linePauseMs,
        endHoldMs,
        eraseBackwards,
        eraseCharDelayMs,
        eraseLinePauseMs
    );
}

} // namespace DisplayAnim

#endif