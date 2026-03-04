#include "vibe_display.h"
#include <math.h>

// --- Module state ---
static fl::shared_ptr<fl::AudioProcessor> sProc;
static fl::CRGB* sLeds = nullptr;
static XYMapFunc sXY = nullptr;
static uint8_t sW = 0;
static uint8_t sH = 0;
static uint16_t sNumLeds = 0;

static uint8_t sBaseHue = 0;

// Per-band ripple state
static float   sBassRippleRadius = 999.0f;
static uint8_t sBassRippleBright = 0;
static float   sMidRippleRadius = 999.0f;
static uint8_t sMidRippleBright = 0;

static uint32_t sBassSpikes = 0;
static uint32_t sMidSpikes = 0;

static float sCX = 0;
static float sCY = 0;
static float sMaxDist = 0;

// ---------------------------------------------------------

void vibeSetup(fl::shared_ptr<fl::AudioProcessor> proc,
               fl::CRGB* ledArray,
               uint8_t width, uint8_t height,
               XYMapFunc xyFunc)
{
    sProc = proc;
    sLeds = ledArray;
    sXY   = xyFunc;
    sW    = width;
    sH    = height;
    sNumLeds = (uint16_t)width * height;

    sCX = (width - 1) / 2.0f;
    sCY = (height - 1) / 2.0f;
    sMaxDist = sqrtf(sCX * sCX + sCY * sCY);

    // Bass spike: big wide ripple + hue jump
    proc->onVibeBassSpike([&]() {
        sBassSpikes++;
        sBassRippleRadius = 0.0f;
        sBassRippleBright = 255;
        sBaseHue += 32;
    });

    // Mid spike: thinner, faster ripple
    proc->onVibeMidSpike([&]() {
        sMidSpikes++;
        sMidRippleRadius = 0.0f;
        sMidRippleBright = 200;
    });
}

// ---------------------------------------------------------

void vibeLoop()
{
    if (!sProc || !sLeds || !sXY) return;

    // Read self-normalizing vibe levels (~1.0 = average)
    float bass = sProc->getVibeBass();
    float mid  = sProc->getVibeMid();
    float treb = sProc->getVibeTreb();

    // Clamp to useful display range: map ~0..2 to 0..1
    auto normLevel = [](float v) -> float {
        float n = v * 0.5f;  // 1.0 (average) -> 0.5 fill
        if (n > 1.0f) n = 1.0f;
        if (n < 0.0f) n = 0.0f;
        return n;
    };
    float bassN = normLevel(bass);
    float midN  = normLevel(mid);
    float trebN = normLevel(treb);

    // --- Zone layout: bass=bottom, mid=middle, treble=top ---
    uint8_t bassRows   = sH / 3;
    uint8_t midRows    = sH / 3;
    uint8_t trebleRows = sH - bassRows - midRows;

    uint8_t bassStart   = sH - bassRows;   // bottom
    uint8_t midStart    = trebleRows;       // middle
    // treble starts at row 0

    // Columns to light based on level (symmetric from center)
    uint8_t bassCols   = (uint8_t)(bassN * sW);
    uint8_t midCols    = (uint8_t)(midN  * sW);
    uint8_t trebleCols = (uint8_t)(trebN * sW);

    // Slow hue drift
    EVERY_N_MILLISECONDS(50) { sBaseHue++; }

    // Decay bass ripple brightness
    if (sBassRippleBright > 4) {
        sBassRippleBright = (uint8_t)(sBassRippleBright * 0.88f);
    } else {
        sBassRippleBright = 0;
    }

    // Decay mid ripple brightness (faster decay)
    if (sMidRippleBright > 4) {
        sMidRippleBright = (uint8_t)(sMidRippleBright * 0.82f);
    } else {
        sMidRippleBright = 0;
    }

    // Expand ripples quickly (~20-40ms to cross the display)
    if (sBassRippleRadius < sMaxDist + 6.0f) {
        sBassRippleRadius += 3.0f;
    }
    if (sMidRippleRadius < sMaxDist + 6.0f) {
        sMidRippleRadius += 4.5f;  // Mid ripple moves faster
    }

    // --- Clear and draw ---
    FastLED.clear();

    uint8_t bassHue   = sBaseHue;
    uint8_t midHue    = sBaseHue + 85;
    uint8_t trebleHue = sBaseHue + 170;

    // Helper: draw a band of rows with symmetric column fill
    auto drawBand = [&](uint8_t yStart, uint8_t rows, uint8_t cols,
                        uint8_t hue, uint8_t sat, float level) {
        uint8_t rowBright = 140 + (uint8_t)(115.0f * level);
        int halfCols = (int)(cols / 2);
        int center = (int)(sW / 2);
        for (uint8_t y = yStart; y < yStart + rows; y++) {
            for (uint8_t x = 0; x < sW; x++) {
                int d = abs((int)x - center);
                if (d < halfCols) {
                    uint8_t b = (d * 4 < rowBright) ? rowBright - d * 4 : 0;
                    sLeds[sXY(x, y)] = CHSV(hue, sat, b);
                }
            }
        }
    };

    drawBand(bassStart,  bassRows,   bassCols,   bassHue,   240, bassN);
    drawBand(midStart,   midRows,    midCols,    midHue,    220, midN);
    drawBand(0,          trebleRows, trebleCols, trebleHue, 200, trebN);

    // --- Ripples: expanding colored rings from center ---
    // One pass through the pixel grid, check both ripples per pixel
    for (uint8_t y = 0; y < sH; y++) {
        for (uint8_t x = 0; x < sW; x++) {
            float dx = (float)x - sCX;
            float dy = (float)y - sCY;
            float dist = sqrtf(dx * dx + dy * dy);

            // Bass ripple: wide, warm-colored ring
            if (sBassRippleBright > 0 && sBassRippleRadius < sMaxDist + 6.0f) {
                float diff = fabsf(dist - sBassRippleRadius);
                float ringWidth = 4.0f;
                if (diff < ringWidth) {
                    uint8_t rb = (uint8_t)((1.0f - diff / ringWidth) * sBassRippleBright);
                    // Warm tinted ring (bass hue)
                    sLeds[sXY(x, y)] += fl::CRGB(rb, rb / 3, 0);
                }
            }

            // Mid ripple: thinner, cool-colored ring
            if (sMidRippleBright > 0 && sMidRippleRadius < sMaxDist + 6.0f) {
                float diff = fabsf(dist - sMidRippleRadius);
                float ringWidth = 2.0f;
                if (diff < ringWidth) {
                    uint8_t rb = (uint8_t)((1.0f - diff / ringWidth) * sMidRippleBright);
                    // Cool tinted ring (mid/cyan)
                    sLeds[sXY(x, y)] += fl::CRGB(0, rb, rb / 2);
                }
            }
        }
    }

    // Debug
    EVERY_N_MILLISECONDS(500) {
        Serial.printf("vB=%.2f vM=%.2f vT=%.2f bassSpk=%lu midSpk=%lu hue=%d\n",
            bass, mid, treb, (unsigned long)sBassSpikes, (unsigned long)sMidSpikes, sBaseHue);
    }
}
