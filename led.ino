#include "FastLED.h"  
#include <SoftwareSerial.h>
#include <stdlib.h>
#define LED_DT_1 5
#define COLOR_ORDER GRB                                      
#define LED_TYPE WS2812                                
#define NUM_LEDS 9

uint8_t max_bright = 255;
CRGB leds[NUM_LEDS];
unsigned int state = 1;
static uint8_t starthue = 0;

SoftwareSerial atmega(10,11); //RX, TX

uint8_t get_val() {
  while(!atmega.available());
  return atmega.read();
}
 
void setup() {
  delay(1000);                                            
 
  LEDS.addLeds<LED_TYPE, LED_DT_1, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(50);

  pinMode(10, INPUT);
  Serial.begin(57600);

  while (!Serial);
  atmega.begin(2400);
  Serial.println("Hello");
}


 
void loop() {
  uint8_t val = get_val();
  Serial.print("Received: ");
  Serial.println(val);
  state = val;
  switch(state) {
    case(0) :
      fill_solid(leds, 9, CRGB::Red);
      FastLED.show();
      FastLED.delay(15);
      break;
    case(1) :
      fill_solid(leds, 9, CRGB::Blue);
      FastLED.show();
      FastLED.delay(15);
      break;
    case(2) :
      fill_solid(leds, 9, CRGB::Green);
      FastLED.show();
      FastLED.delay(15);
      break;
     case(3) :
      fill_rainbow( leds, 9, --starthue, 20);
      FastLED.show();
      FastLED.delay(15);
      break;
     case(4) :
      fill_solid( leds, 9, CRGB::Black);
      FastLED.show();
      FastLED.delay(15);
      break;
  }
   
  //static uint8_t starthue = 0;
  //fill_rainbow( leds, NUM_LEDS, --starthue, 20);

 delay(50);
 
}
