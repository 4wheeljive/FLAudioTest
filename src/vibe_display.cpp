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
}

// ---------------------------------------------------------

struct avLeveler {
    float ceiling;  // P90 tracker
    float floor;    // P10 tracker
    float value = 0.0f;

    avLeveler(float initFloor = 0.5f, float initCeiling = 3.0f)
        : ceiling(initCeiling), floor(initFloor) {}

    // Call once per frame with a vibe value (getBass/getMid/getTreb)
    float update(float v) {
        constexpr float kRate = 0.015f;      // ~4s convergence at 30fps
        constexpr float kMinRange = 0.3f;

        // Robbins-Monro quantile estimation
        // P90: 90% of values below ceiling, 10% above (beat peaks)
        // P10: 10% of values below floor (quiet moments)
        ceiling += kRate * ((v > ceiling) ? 0.9f : -0.1f);
        floor   += kRate * ((v > floor)   ? 0.1f : -0.9f);

        // Enforce minimum dynamic range
        if (ceiling - floor < kMinRange) {
            float mid = (ceiling + floor) * 0.5f;
            ceiling = mid + kMinRange * 0.5f;
            floor   = mid - kMinRange * 0.5f;
        }

        value = (v - floor) / (ceiling - floor);
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;
        return value;
    }
};

avLeveler bassViz, midViz, trebViz;

// ---------------------------------------------------------

void vibeLoop()
{
    if (!sProc || !sLeds || !sXY) return;

    // Read self-normalizing vibe levels (~1.0 = average, can spike to 10+)
    // Then normalize to 0.0–1.0 via adaptive P10/P90 percentile tracking
    float bassN = bassViz.update(sProc->getVibeBass());
    float midN  = midViz.update(sProc->getVibeMid());
    float trebN = trebViz.update(sProc->getVibeTreb());

    // --- Zone layout: bass=bottom, mid=middle, treble=top ---
    float trebleRows = sH / 4.0f; //- bassRows - midRows;
    float midRows    = sH / 4.0f; // 3.0f
    float bassRows   = sH / 4.0f; // 3.0f

    // treble starts at row 1 (not 0)
    float midStart    = trebleRows + 2.0f;        // middle
    float bassStart   = midStart + midRows + 2.0f; // sH - bassRows;   // bottom

    // Half-width of lit region (float, symmetric from center)
    float center   = (sW - 1) / 2.0f;
    float trebHalf = trebN * sW / 2.0f;
    float midHalf  = midN  * sW / 2.0f;
    float bassHalf = bassN * sW / 2.0f;

    // Slow hue drift
    EVERY_N_MILLISECONDS(50) { sBaseHue++; }

    // --- Clear and draw ---
    FastLED.clear();

    float bassHue   = sBaseHue;
    float midHue    = sBaseHue + 85.0f;
    float trebleHue = sBaseHue + 170.0f;

    // Helper: draw a band with float precision.
    // Pixels at the edge of the fill get fractional brightness for smooth edges.
    // Brightness tapers from center toward edges.
    auto drawBand = [&](float yStart, float rows, float halfWidth,
                        float hue, float sat, float level) {
        float peakBright = 140.0f + 115.0f * level;
        float yEnd = yStart + rows;
        for (uint8_t y = 0; y < sH; y++) {
            // Vertical edge blending
            float yFrac = 1.0f;
            if ((float)y < yStart) {
                yFrac = 1.0f - (yStart - (float)y);
                if (yFrac <= 0.0f) continue;
            } else if ((float)(y + 1) > yEnd) {
                yFrac = yEnd - (float)y;
                if (yFrac <= 0.0f) continue;
            }

            for (uint8_t x = 0; x < sW; x++) {
                float dist = fabsf((float)x - center);
                if (dist >= halfWidth) continue;

                // Horizontal edge blending: fractional pixel at boundary
                float xFrac = 1.0f;
                if (dist > halfWidth - 1.0f) {
                    xFrac = halfWidth - dist;
                }

                // Brightness tapers from center
                float taper = 1.0f - (dist / (sW / 2.0f));
                float bright = peakBright * taper * xFrac * yFrac;
                if (bright > 255.0f) bright = 255.0f;

                // Convert to 8-bit at the last moment
                uint8_t h8 = (uint8_t)((int)hue & 0xFF);
                uint8_t s8 = (uint8_t)sat;
                uint8_t v8 = (uint8_t)bright;
                sLeds[sXY(x, y)] = CHSV(h8, s8, v8);
            }
        }
    };

    drawBand(1.0f,       trebleRows, trebHalf, trebleHue, 200.0f, trebN);
    drawBand(midStart,   midRows,    midHalf,  midHue,    220.0f, midN);
    drawBand(bassStart,  bassRows,   bassHalf, bassHue,   240.0f, bassN);

    // Debug: raw vibe values + normalized 0-1 output
    EVERY_N_MILLISECONDS(500) {
        Serial.printf("vB=%.2f vM=%.2f vT=%.2f  nB=%.2f nM=%.2f nT=%.2f  hue=%d\n",
            sProc->getVibeBass(), sProc->getVibeMid(), sProc->getVibeTreb(),
            bassN, midN, trebN, sBaseHue);
    }
}
