#include <FastLED.h>
#include "fl/audio_input.h"
#include "fl/audio.h"
#include "fl/audio/audio_processor.h"
#include "fl/audio/audio_detector.h"
#include "fl/time_alpha.h"

bool debug = true;

//#define BIG_BOARD
#undef BIG_BOARD

#define PIN0 2

//*********************************************

#ifdef BIG_BOARD 
	
	#include "reference/matrixMap_32x48_3pin.h" 
	#define PIN1 3
    #define PIN2 4
    #define HEIGHT 32 
    #define WIDTH 48
    #define NUM_STRIPS 3
    #define NUM_LEDS_PER_STRIP 512
				
#else 
	
	#include "reference/matrixMap_22x22.h"
	#define HEIGHT 22 
    #define WIDTH 22
    #define NUM_STRIPS 1
    #define NUM_LEDS_PER_STRIP 484
	
#endif

#define NUM_LEDS ( WIDTH * HEIGHT )

fl::CRGB leds[NUM_LEDS];
uint16_t ledNum = 0;

extern const uint16_t progTopDown[NUM_LEDS] PROGMEM;

uint16_t myXY(uint8_t x, uint8_t y) {
	if (x >= WIDTH || y >= HEIGHT) return 0;
	uint16_t i = ( y * WIDTH ) + x;
	ledNum = progTopDown[i];
	return ledNum;
}

uint8_t hue = 0;
uint8_t beatBrightness = 0;  // Decaying brightness for beat pulse

using namespace fl;

// Audio setup ***************************************************

#define I2S_CLK_PIN 7 // Serial Clock (SCK) (BLUE)
#define I2S_WS_PIN 8  // Word Select (WS) (GREEN)
#define I2S_SD_PIN 9  // Serial Data (SD) (YELLOW)
#define I2S_CHANNEL fl::Left 

fl::AudioConfigI2S i2sConfig(I2S_WS_PIN, I2S_SD_PIN, I2S_CLK_PIN, 0, I2S_CHANNEL, 44100, 16, fl::Philips);
fl::AudioConfig config(i2sConfig);
fl::shared_ptr<fl::IAudioInput> audioInput;

fl::AudioProcessor audioProcessor;

// Beat detection state
float currentBPM = 0.0f;
uint32_t lastBeatTime = 0;
uint32_t beatCount = 0;
uint32_t onsetCount = 0;
bool vocalsActive = false;
float vocalConfidence = 0.0f;
uint8_t vocalConfidenceEMA = 0;
uint8_t smoothedVocalConfidence = 0;
uint8_t scaledVocalConfidence = 0;

// **************************************************************

void setup() {
		
	Serial.begin(115200);
	delay(1000);
	
    FastLED.setExclusiveDriver("RMT");

	FastLED.addLeds<WS2812B, PIN0, GRB>(leds, 0, NUM_LEDS_PER_STRIP)
		.setCorrection(TypicalLEDStrip);

	#ifdef PIN1
		FastLED.addLeds<WS2812B, PIN1, GRB>(leds, NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP)
			.setCorrection(TypicalLEDStrip);
	#endif
	
	#ifdef PIN2
		FastLED.addLeds<WS2812B, PIN2, GRB>(leds, NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP)
			.setCorrection(TypicalLEDStrip);
	#endif

	FastLED.setBrightness(75);

	FastLED.clear();
	FastLED.show();

	fl::string errorMsg;
	audioInput = fl::IAudioInput::create(config, &errorMsg);
	audioInput->start();
    audioProcessor.setAutoGainEnabled(true);

    audioProcessor.onBeat([]() {
        beatCount++;
        lastBeatTime = fl::millis();
        Serial.print("BEAT #");
        Serial.println(beatCount);
        beatBrightness = 255;   // Reset brightness on each beat
        //hue += 32;              // Shift color on each beat
    });

    audioProcessor.onVocalStart([]() {
        vocalsActive = true;
    });

    audioProcessor.onVocalEnd([]() {
        vocalsActive = false;
    });

}

// **************************************************************

void beatPulse() {

    FastLED.clear();
    CHSV color = CHSV(hue,255,beatBrightness);
    fill_solid(leds, NUM_LEDS, color);  

    // Decay the brightness
    if (beatBrightness > 10) {
        beatBrightness = beatBrightness * 0.85f;  // Exponential decay
    } else {
        beatBrightness = 0;
    }

}

/*uint8_t smoothVocalConfidence(uint8_t level) {
    constexpr float attack  = 0.35f;  // 0.35 fast rise on spikes
    constexpr float release = 0.20f;  // 0.04f = slow decay
    float alpha  = (level > vocalConfidenceEMA) ? attack : release;
    vocalConfidenceEMA += alpha * (level - vocalConfidenceEMA);
    return vocalConfidenceEMA;
 }*/

void vocalResponse() {
    FastLED.clear();
    vocalConfidence = audioProcessor.getVocalConfidence();
    //smoothedVocalConfidence = smoothVocalConfidence(vocalConfidence);
    scaledVocalConfidence = fl::map_range_clamped<float, uint8_t>(vocalConfidence, 0.0f, 0.7f, 0, 255);
    CHSV color = CHSV(0,255,scaledVocalConfidence);
    fill_solid(leds, NUM_LEDS, color);  
    //hue++;
}

// **************************************************************

void loop(){
    
    EVERY_N_MILLISECONDS(250) {
        float bass = audioProcessor.getBassLevel();
        float mid = audioProcessor.getMidLevel();
        float treble = audioProcessor.getTrebleLevel();

        FASTLED_DBG("Bass: " << bass
                    << " Mid: " << mid
                    << " Treb: " << treble
        );
        FASTLED_DBG("Vox active: " << vocalsActive
                    << " Vox conf: " << vocalConfidence
                    << " Smoothed vox conf: " << scaledVocalConfidence
        );
	}

	while (fl::AudioSample sample = audioInput->read()) {
        audioProcessor.update(sample);
	}

    if (scaledVocalConfidence > 0.05f) {
        vocalResponse();
    } else {
        beatPulse();
    }
    FastLED.show();

}