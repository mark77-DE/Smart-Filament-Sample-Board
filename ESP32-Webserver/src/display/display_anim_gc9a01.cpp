#include "display_config.h"

#if DISPLAY_TYPE == DISPLAY_TYPE_GC9A01
#include "display_anim.h"
#include "display.h"
#include <Arduino.h>
#include <math.h>

// ---------------------------------
// Spinner Frames (alt, kompatibel)
// ---------------------------------
static const char spinnerFrames[] = {'|', '/', '-', '\\'};
static const uint8_t spinnerFrameCount = sizeof(spinnerFrames) / sizeof(spinnerFrames[0]);

// ---------------------------------
// Idle State
// ---------------------------------
namespace DisplayAnim {

struct IdleState {
    bool active = false;
    bool textFirst = false;

    // Alte Spinner-Daten
    unsigned long lastSpinnerUpdate = 0;
    uint8_t currentSpinnerFrame = 0;
    uint32_t spinnerIntervalMs = 150;
    String idleText = IDLE_TEXT_STRING;

    // Neue Spule & Farbe
    float rotation = 0;        // Drehwinkel Spule
    float textRotation = 0;    // Drehwinkel Text
    float hue = 0;             // Farbe Spule
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
    state.lastSpinnerUpdate = now;
    state.currentSpinnerFrame = 0;
    state.rotation = 0;
    state.textRotation = 0;
    state.hue = 0;
}

void startIdleTextFirst(unsigned long now) {
    state.active = true;
    state.textFirst = true;
    state.lastSpinnerUpdate = now;
    state.currentSpinnerFrame = 0;
    state.rotation = 0;
    state.textRotation = 0;
    state.hue = 0;
}

void stop() {
    state.active = false;
}

// -------------------------
// Tick Idle
// -------------------------
static uint32_t lastFrame = 0;

void tickIdle(LGFX &display, unsigned long now) {
    if (!state.active) return;

    display.fillScreen(TFT_BLACK);
    int cx = display.width()/2;
    int cy = display.height()/2;
    int radius = 60;

    // Beispiel: LovyanGFX schnelle Kreise
    for(int r=0; r<6; r++){
        float ringRadius = radius - r*8;
        for(float a=0; a<2*PI; a+=0.05){
            int x = cx + int(cos(a + state.rotation) * ringRadius);
            int y = cy + int(sin(a + state.rotation) * ringRadius);
            display.drawPixel(x, y, display.color565(255 * fabs(sin(state.hue)), 255 * fabs(cos(state.hue)), 128));
        }
    }

    state.rotation += 0.05f;
    state.hue += 0.01f;
}

// -------------------------------------------------------
// Drei-Zeilen-Typewriter Animation (blocking mit yield)
// -------------------------------------------------------
void playThreeLineTypewriter(
    DisplayType& display,
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

    for (int i = 0; i < 3; i++) {
        display.setCursor(0, i * 10);
        display.print(""); // Start leer
        
        for (uint16_t c = 0; c < lines[i].length(); c++) {
            display.setCursor(0, i * 10);
            display.print(lines[i].substring(0, c + 1));
            delay(charDelayMs);
            yield();
        }
        delay(linePauseMs);
    }
    delay(endHoldMs);

    if (eraseBackwards) {
        for (int i = 2; i >= 0; i--) {
            for (int c = lines[i].length(); c > 0; c--) {
                display.setCursor(0, i * 10);
                display.print(lines[i].substring(0, c - 1) + " ");
                delay(eraseCharDelayMs);
                yield();
            }
            delay(eraseLinePauseMs);
        }
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