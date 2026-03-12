#pragma once
#include <FastLED.h>
#include "fl/audio/audio_processor.h"

// "Factor Demo" — uses vibe detector output directly as multipliers.
// No audio signal processing knowledge needed.
//
// Usage:
//   In setup():  factorDemoSetup(audioProcessor, leds, width, height, xyFunc);
//   In loop():   factorDemoLoop();

using XYMapFunc = uint16_t (*)(uint8_t x, uint8_t y);

void factorDemoSetup(fl::shared_ptr<fl::AudioProcessor> proc,
                     fl::CRGB* ledArray,
                     uint8_t width, uint8_t height,
                     XYMapFunc xyFunc);

void factorDemoLoop();
