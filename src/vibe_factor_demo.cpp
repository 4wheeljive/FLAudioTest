#include "vibe_factor_demo.h"
#include <math.h>

// --- Module state ---
static fl::shared_ptr<fl::AudioProcessor> sProc;
static fl::CRGB* sLeds = nullptr;
static XYMapFunc sXY = nullptr;
static uint8_t sW = 0, sH = 0;

void factorDemoSetup(fl::shared_ptr<fl::AudioProcessor> proc,
                     fl::CRGB* ledArray,
                     uint8_t width, uint8_t height,
                     XYMapFunc xyFunc)
{
    sProc = proc;
    sLeds = ledArray;
    sXY = xyFunc;
    sW = width;
    sH = height;
}

void factorDemoLoop()
{
    if (!sProc || !sLeds || !sXY) return;

    // Get vibe levels — just numbers, bigger = more energy in that band
    float bass = sProc->getVibeBass();
    float mid  = sProc->getVibeMid();
    float treb = sProc->getVibeTreb();

    // Use them directly as MULTIPLIERS on visual parameters.
    // No normalization, no log compression, no audio knowledge needed.
    // Values hover around 1.0, so they naturally scale things:
    //   1.0 = normal, >1 = more intense, <1 = calmer

    // Bass controls brightness: base of 140, bass multiplies it
    float bright = 140.0f * bass;
    if (bright > 255.0f) bright = 255.0f;
    if (bright < 0.0f) bright = 0.0f;
    uint8_t v8 = (uint8_t)bright;

    // Mid controls how fast the pattern animates
    static float time = 0.0f;
    time += 0.3f * mid;

    // Treble controls how fast colors cycle
    static float hueBase = 0.0f;
    hueBase += 0.5f * treb;

    // Simple animated plasma — three overlapping sine waves
    for (uint8_t y = 0; y < sH; y++) {
        for (uint8_t x = 0; x < sW; x++) {
            float v = sinf(x * 0.3f + time)
                    + sinf(y * 0.4f - time * 0.7f)
                    + sinf((x + y) * 0.2f + time * 0.5f);
            // v ranges roughly -3 to +3, use as hue variation
            float hue = hueBase + v * 30.0f;

            sLeds[sXY(x, y)] = CHSV(
                (uint8_t)((int)hue & 0xFF),
                220,
                v8
            );
        }
    }

    EVERY_N_MILLISECONDS(500) {
        Serial.printf("bass=%.2f mid=%.2f treb=%.2f bright=%d\n",
            bass, mid, treb, (int)v8);
    }
}
