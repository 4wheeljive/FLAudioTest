#pragma once
#include <FastLED.h>
#include "fl/audio/audio_processor.h"

// "Vibe Reactor" - Audio-reactive LED display
//
// Shows three horizontal frequency bands (bass/mid/treble) that fill
// symmetrically from center based on smoothed levels, each with its
// own shifting color. On beat detection, a bright ring ripples outward
// from center.
//
// Usage:
//   In setup():  vibeSetup(audioProcessor, leds, width, height, xyFunc);
//   In loop():   vibeLoop();

// XY mapping function type: takes (x, y) returns LED index
using XYMapFunc = uint16_t (*)(uint8_t x, uint8_t y);

void vibeSetup(fl::shared_ptr<fl::AudioProcessor> proc,
               fl::CRGB* ledArray,
               uint8_t width, uint8_t height,
               XYMapFunc xyFunc);

void vibeLoop();
