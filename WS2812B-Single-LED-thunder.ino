#include <Adafruit_NeoPixel.h>

#define LED_PIN   5     // Use pin 5 for the NeoPixel strip
#define NUM_LEDS  5     // One strip with 5 NeoPixels

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastEffect = 0;
unsigned long effectInterval;

void setup() {
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  randomSeed(analogRead(0));
  effectInterval = random(2000, 10000); // Set first strike interval
}

void loop() {
  unsigned long now = millis();
  if (now - lastEffect >= effectInterval) {
    // Randomly choose one of the two lightning effects
    if (random(0, 2) == 0) {
      lightningEffect1();
    } else {
      lightningEffect2();
    }
    lastEffect = now;
    effectInterval = random(2000, 10000); // Set next strike interval
  }
}

// Lightning Effect 1: All pixels flash together in quick bursts
void lightningEffect1() {
  int flashes = random(2, 4); // 2 to 3 flashes per strike
  for (int i = 0; i < flashes; i++) {
    // Turn all LEDs to bright white
    for (int j = 0; j < NUM_LEDS; j++) {
      strip.setPixelColor(j, strip.Color(255, 255, 255));
    }
    strip.show();
    delay(random(20, 100)); // Flash duration

    // Turn off all LEDs
    for (int j = 0; j < NUM_LEDS; j++) {
      strip.setPixelColor(j, strip.Color(0, 0, 0));
    }
    strip.show();
    delay(random(50, 200)); // Pause between flashes
  }
}

// Lightning Effect 2: Cascading flash that lights each LED in sequence
void lightningEffect2() {
  // Light up LEDs one by one
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(255, 255, 255));
    strip.show();
    delay(random(30, 70)); // Delay between lighting each LED
  }
  delay(50);
  // Turn off all LEDs simultaneously
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
  delay(random(100, 200));
}
