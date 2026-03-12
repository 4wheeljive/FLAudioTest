#pragma once
#include <FastLED.h>
#include "fl/audio/audio_processor.h"

// XY mapping function type: takes (x, y) returns LED index
using XYMapFunc = uint16_t (*)(uint8_t x, uint8_t y);

void vibeSetup(fl::shared_ptr<fl::AudioProcessor> proc,
               fl::CRGB* ledArray,
               uint8_t width, uint8_t height,
               XYMapFunc xyFunc);

void vibeLoop();