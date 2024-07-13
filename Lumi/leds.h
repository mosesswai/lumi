#include <Adafruit_NeoPixel.h>

#define NUM_PIXELS         4
#define PIXEL_BRIGHTNESS  30
#define PIXEL_PIN         12

// light colors
typedef enum {
  OFF = 0x000000,
  RED =  0x00FF00,
  ORANGE = 0xA5FF00,
  YELLOW = 0xFFFF00,
  GREEN =  0xFF0000,
  LAVENDER = 0xE6E6FA
}Colors;

// current ON pixel
uint16_t active_pixel = 0;

/***** Neopixels *****/
Adafruit_NeoPixel pixels(NUM_PIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// initialize the Neopixel LED
void initLED(){
  // If this board has a power control pin, we must set it to output and high
  // in order to enable the NeoPixels. We put this in an #if defined so it can
  // be reused for other boards without compilation errors
  // //PIN_NEOPIXEL
  // #if defined(NEOPIXEL_POWER)
  //   pinMode(NEOPIXEL_POWER, OUTPUT);
  //   digitalWrite(NEOPIXEL_POWER, HIGH);
  // #endif

  pixels.begin(); 
  pixels.setBrightness(PIXEL_BRIGHTNESS); 
}

// set the Neopixel color
void setIndicator(Colors color){
    pixels.fill(color);
    pixels.show();
}

// get Neopixel brightness
int getIndicatorBrightness() {
  return pixels.getBrightness();
}

// set Neopixel brightness
void setIndicatorBrightness(int brightness) {
  pixels.setBrightness(brightness);
  pixels.show(); 
}

// advance the ON LED to the next in line
void advanceIndicator() {
  uint32_t col = pixels.getPixelColor(active_pixel);
  active_pixel +=1;
  if (active_pixel == NUM_PIXELS) active_pixel = 0; 
  pixels.fill();
  pixels.setPixelColor(active_pixel, col);
  pixels.setBrightness(PIXEL_BRIGHTNESS);
  pixels.show();
}

// reset indicator LED brightness
void resetIndicator() {
  pixels.setBrightness(PIXEL_BRIGHTNESS);
  pixels.show();
}

