#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiManager.h>
#include <Preferences.h>

// LED Pin Definitions
#define SUN_PIN       2  // GPIO 2 (A2)
#define NUM_SUN_LEDS  4
Adafruit_NeoPixel sunStrip(NUM_SUN_LEDS, SUN_PIN, NEO_GRB + NEO_KHZ800);

#define CLOUD_PIN     4  // GPIO 4 (A4)
#define NUM_CLOUD_LEDS 5
Adafruit_NeoPixel cloudStrip(NUM_CLOUD_LEDS, CLOUD_PIN, NEO_GRB + NEO_KHZ800);

#define TOUCH_PIN     3  // GPIO 3 (A3)

// State & Control Variables
bool ledsEnabled = true;
bool prevLedsEnabledState = true; // To detect changes in ledsEnabled
bool lastTouchState = LOW;
unsigned long touchStartTime = 0;
bool touchTimingStarted = false;
unsigned long startupTime = 0;

unsigned long lastUpdate = 0;
unsigned long lastThunder = 0;
String currentState = "";
String lastKnownState = "clear"; // Fallback state
bool isManualOverride = false;
bool isInitialized = false; // Becomes true after first successful weather update or manual set
const unsigned long updateInterval = 10 * 60 * 1000; // 10 minutes
bool lightningStyle = false; // Toggle between all-flash and sequential-flash

// WiFiManager Parameters
WiFiManagerParameter custom_api_key("api", "Weather API Key", "YOUR_API_KEY", 40); // Replace with your actual default or leave empty
WiFiManagerParameter custom_city("city", "City", "Seville", 20);
WiFiManagerParameter custom_country("country", "Country Code", "ES", 3);
String weatherApiKey;
String city;
String countryCode;

Preferences preferences;

// --- Crossfade Variables ---
uint32_t sunCurrentColors[NUM_SUN_LEDS];
uint32_t sunTargetColors[NUM_SUN_LEDS];
uint32_t cloudCurrentColors[NUM_CLOUD_LEDS];
uint32_t cloudTargetColors[NUM_CLOUD_LEDS];

bool sunFadingInProgress = false;
bool cloudFadingInProgress = false;
unsigned long sunFadeStartTime = 0;
unsigned long cloudFadeStartTime = 0;
const unsigned long fadeDuration = 1500; // 1.5 seconds for a fade

// Function Prototypes
void saveConfigCallback();
void updateWeather();
void applyCurrentLedState(bool forceUpdate = false); // New function to set target colors
void ensureWiFiConnected();
void indicateMode();
void indicateWiFiStatus(bool connected);
void showLightningCloud(); // Will remain mostly abrupt for the flashes

// --- Helper functions for color manipulation and fading ---
void splitColor(uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = (uint8_t)(color >> 16);
    g = (uint8_t)(color >> 8);
    b = (uint8_t)color;
}

uint8_t interpolateComponent(uint8_t start, uint8_t end, float progress) {
    return (uint8_t)((float)start + ((float)end - (float)start) * progress);
}

uint32_t interpolateColor(uint32_t startColor, uint32_t endColor, float progress, Adafruit_NeoPixel& strip) {
    if (progress >= 1.0f) return endColor;
    if (progress <= 0.0f) return startColor;

    uint8_t r1, g1, b1;
    uint8_t r2, g2, b2;

    splitColor(startColor, r1, g1, b1);
    splitColor(endColor, r2, g2, b2);

    uint8_t r = interpolateComponent(r1, r2, progress);
    uint8_t g = interpolateComponent(g1, g2, progress);
    uint8_t b = interpolateComponent(b1, b2, progress);

    return strip.Color(r, g, b);
}

void startSunFade() {
    sunFadeStartTime = millis();
    sunFadingInProgress = true;
}

void startCloudFade() {
    cloudFadeStartTime = millis();
    cloudFadingInProgress = true;
}

void setSunTargetColor(uint32_t color) {
    for (int i = 0; i < NUM_SUN_LEDS; i++) {
        sunTargetColors[i] = color;
    }
    startSunFade();
}

void setSunTargetColorsRandomBrightness(uint8_t r_base, uint8_t g_base, uint8_t b_base) {
    for (int i = 0; i < NUM_SUN_LEDS; i++) {
        float brightness = random(85, 101) / 100.0; // Random brightness flicker
        sunTargetColors[i] = sunStrip.Color((uint8_t)(r_base * brightness), 
                                            (uint8_t)(g_base * brightness), 
                                            (uint8_t)(b_base * brightness));
    }
    startSunFade();
}

void setCloudTargetColor(uint32_t color) {
    for (int i = 0; i < NUM_CLOUD_LEDS; i++) {
        cloudTargetColors[i] = color;
    }
    startCloudFade();
}


void applyCrossfade() {
    bool sunNeedsShow = false;
    bool cloudNeedsShow = false;

    if (sunFadingInProgress) {
        unsigned long elapsed = millis() - sunFadeStartTime;
        float progress = (float)elapsed / fadeDuration;
        if (progress >= 1.0f) {
            progress = 1.0f;
            sunFadingInProgress = false;
        }
        for (int i = 0; i < NUM_SUN_LEDS; i++) {
            uint32_t newColor = interpolateColor(sunCurrentColors[i], sunTargetColors[i], progress, sunStrip);
            if (sunCurrentColors[i] != newColor) {
                sunStrip.setPixelColor(i, newColor);
                sunCurrentColors[i] = newColor;
                sunNeedsShow = true;
            }
        }
    }

    if (cloudFadingInProgress) {
        unsigned long elapsed = millis() - cloudFadeStartTime;
        float progress = (float)elapsed / fadeDuration;
        if (progress >= 1.0f) {
            progress = 1.0f;
            cloudFadingInProgress = false;
        }
        for (int i = 0; i < NUM_CLOUD_LEDS; i++) {
            uint32_t newColor = interpolateColor(cloudCurrentColors[i], cloudTargetColors[i], progress, cloudStrip);
             if (cloudCurrentColors[i] != newColor) {
                cloudStrip.setPixelColor(i, newColor);
                cloudCurrentColors[i] = newColor;
                cloudNeedsShow = true;
            }
        }
    }

    if (sunNeedsShow) sunStrip.show();
    if (cloudNeedsShow) cloudStrip.show();
}
// --- End of Fading Helpers ---

void saveConfigCallback() {
  preferences.begin("weatherled", false);
  preferences.putString("city", custom_city.getValue());
  preferences.putString("countryCode", custom_country.getValue());
  preferences.putString("apiKey", custom_api_key.getValue());
  preferences.end();
}

void setup() {
  pinMode(TOUCH_PIN, INPUT);

  sunStrip.begin();
  cloudStrip.begin();
  sunStrip.setBrightness(255); // Set max brightness, actual color values will control intensity
  cloudStrip.setBrightness(255);

  // Initialize current colors to black
  for (int i = 0; i < NUM_SUN_LEDS; i++) sunCurrentColors[i] = sunStrip.Color(0,0,0);
  for (int i = 0; i < NUM_CLOUD_LEDS; i++) cloudCurrentColors[i] = cloudStrip.Color(0,0,0);
  
  setSunTargetColor(sunStrip.Color(0,0,0)); // Set target to black
  setCloudTargetColor(cloudStrip.Color(0,0,0)); // Set target to black
  // Apply immediately for startup
  for (int i = 0; i < NUM_SUN_LEDS; i++) sunStrip.setPixelColor(i, sunCurrentColors[i]);
  for (int i = 0; i < NUM_CLOUD_LEDS; i++) cloudStrip.setPixelColor(i, cloudCurrentColors[i]);
  sunStrip.show();
  cloudStrip.show();


  // Test both strips immediately (orange) - this will be an abrupt change for testing
  for (int i = 0; i < NUM_SUN_LEDS; i++) sunStrip.setPixelColor(i, sunStrip.Color(255, 165, 0));
  for (int i = 0; i < NUM_CLOUD_LEDS; i++) cloudStrip.setPixelColor(i, cloudStrip.Color(255, 165, 0));
  sunStrip.show();
  cloudStrip.show();
  delay(1000);
  // After test, fade them out
  setSunTargetColor(sunStrip.Color(0,0,0));
  setCloudTargetColor(cloudStrip.Color(0,0,0));
  // Let the loop handle the fade out from orange to black.

  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.addParameter(&custom_api_key);
  wm.addParameter(&custom_city);
  wm.addParameter(&custom_country);

  preferences.begin("weatherled", true);
  String savedCity = preferences.getString("city", "Seville");
  String savedCountry = preferences.getString("countryCode", "ES");
  String savedApiKey = preferences.getString("apiKey", "YOUR_API_KEY"); // Ensure this matches default
  preferences.end();

  custom_city.setValue(savedCity.c_str(), 20);
  custom_country.setValue(savedCountry.c_str(), 3);
  custom_api_key.setValue(savedApiKey.c_str(), 40);

  wm.setSaveConfigCallback(saveConfigCallback);

  bool connected = wm.autoConnect("WeatherLED_AP", NULL);
  indicateWiFiStatus(connected); // This is an abrupt indicator

  if (!connected) {
    // Blink orange if WiFi fails critically during setup
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < NUM_SUN_LEDS; j++) sunStrip.setPixelColor(j, sunStrip.Color(255, 165, 0));
      sunStrip.show();
      delay(500);
      for (int j = 0; j < NUM_SUN_LEDS; j++) sunStrip.setPixelColor(j, sunStrip.Color(0,0,0));
      sunStrip.show();
      delay(500);
    }
    // ESP.restart(); // Or while(true)
    while (true) { delay(1000); } 
  }

  weatherApiKey = custom_api_key.getValue();
  city = custom_city.getValue();
  countryCode = custom_country.getValue();

  updateWeather(); // Initial weather update
  // applyCurrentLedState will be called within updateWeather if successful
  
  lastTouchState = digitalRead(TOUCH_PIN);
  startupTime = millis();
  prevLedsEnabledState = ledsEnabled;
}

void loop() {
  ensureWiFiConnected(); // Checks and attempts reconnect, may indicate and restart

  if (millis() - lastUpdate >= updateInterval) {
    if (!isManualOverride) { // Only auto-update if not in manual mode
        updateWeather();
    }
    lastUpdate = millis();
  }

  // Handle touch input
  if (millis() - startupTime > 3000) { // Debounce touch for a few seconds after startup
    bool touchState = digitalRead(TOUCH_PIN);
    if (touchState == HIGH && lastTouchState == LOW && !touchTimingStarted) {
      touchStartTime = millis();
      touchTimingStarted = true;
    }
    if (touchState == LOW && lastTouchState == HIGH && touchTimingStarted) { // Touch released
      unsigned long touchDuration = millis() - touchStartTime;
      touchTimingStarted = false;

      if (touchDuration < 1000) { // Short press: Toggle LEDs
        ledsEnabled = !ledsEnabled;
      } else if (touchDuration < 5000) { // Medium press: Cycle manual states
        isManualOverride = true;
        if (currentState == "clear") currentState = "clear+cloud";
        else if (currentState == "clear+cloud") currentState = "cloud";
        else if (currentState == "cloud") currentState = "thunder";
        else currentState = "clear"; // Cycle back
        lastKnownState = currentState; // Manual override sets lastKnownState
        isInitialized = true; // Manual set means it's initialized
        indicateMode(); // Abrupt indicator
        applyCurrentLedState(true); // Force update to new manual state
      } else if (touchDuration < 10000) { // Long press: Revert to auto mode & fetch weather
        isManualOverride = false;
        indicateMode(); // Abrupt indicator
        updateWeather(); // Fetch fresh weather for auto mode
        lastUpdate = millis();
      } else { // Very long press: Reset WiFi
        WiFiManager wm;
        wm.resetSettings();
        // Indicate reset - e.g. flash red
        for(int k=0; k<3; k++){
            for (int i = 0; i < NUM_SUN_LEDS; i++) sunStrip.setPixelColor(i, sunStrip.Color(255,0,0));
            for (int i = 0; i < NUM_CLOUD_LEDS; i++) cloudStrip.setPixelColor(i, cloudStrip.Color(255,0,0));
            sunStrip.show(); cloudStrip.show();
            delay(300);
            for (int i = 0; i < NUM_SUN_LEDS; i++) sunStrip.setPixelColor(i, 0);
            for (int i = 0; i < NUM_CLOUD_LEDS; i++) cloudStrip.setPixelColor(i, 0);
            sunStrip.show(); cloudStrip.show();
            delay(300);
        }
        ESP.restart();
      }
    }
    lastTouchState = touchState;
  }

  // Apply LED state based on ledsEnabled
  if (ledsEnabled != prevLedsEnabledState) {
      if (ledsEnabled) {
          // Turning ON
          sunStrip.setBrightness(255); // Ensure brightness is up
          cloudStrip.setBrightness(255);
          isInitialized = true; // Allow display
          applyCurrentLedState(true); // Fade in to current state
      } else {
          // Turning OFF
          setSunTargetColor(sunStrip.Color(0,0,0));
          setCloudTargetColor(cloudStrip.Color(0,0,0));
          // Fading out handled by applyCrossfade.
          // Optional: after fadeDuration, could explicitly call strip.setBrightness(0)
          // but fading to black should suffice.
      }
      prevLedsEnabledState = ledsEnabled;
  }

  if (ledsEnabled && isInitialized) {
    // Regular LED updates (fading handled by applyCrossfade)
    // applyCurrentLedState() is called when state *changes*.
    // Lightning effect needs special handling if it's an active animation
    if (currentState == "thunder" && (millis() - lastThunder >= 30000)) {
        // Don't interrupt an ongoing fade for clouds to base "cloudy" for thunder
        // If cloud is not already fading or has finished fading to its base cloudy color for thunder
        if(!cloudFadingInProgress || cloudTargetColors[0] == cloudStrip.Color(150,150,150)) {
          showLightningCloud(); // This is an abrupt animation overlay
          lastThunder = millis();
          // After lightning, ensure cloud target is back to standard cloudy
          // showLightningCloud() already calls setCloudTargetColor(cloudStrip.Color(150, 150, 150));
        }
    }
  }

  applyCrossfade(); // Continuously update fades

  delay(20); // Loop delay for responsiveness and smooth fades
}

void indicateWiFiStatus(bool connected) {
  // This is an abrupt indicator, temporarily overrides fades
  uint32_t color = connected ? sunStrip.Color(0, 255, 0) : sunStrip.Color(255, 0, 0); // Green for success, Red for failure
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < NUM_SUN_LEDS; j++) sunStrip.setPixelColor(j, color);
    sunStrip.show();
    delay(300); // Shorter delay for quicker indication
    for (int j = 0; j < NUM_SUN_LEDS; j++) sunStrip.setPixelColor(j, 0); // Off
    sunStrip.show();
    delay(300);
  }
  // After indication, the main loop's applyCrossfade will restore/continue the stateful fade.
  // Force an update to the current state's target colors to ensure a clean transition from the indicator
  if(ledsEnabled && isInitialized) applyCurrentLedState(true); 
  else { // if LEDs are off or not initialized, ensure they fade to black
      setSunTargetColor(sunStrip.Color(0,0,0));
      setCloudTargetColor(cloudStrip.Color(0,0,0));
  }
}

void indicateMode() {
  // This is an abrupt indicator, temporarily overrides fades
  // Manual: Light Blue (e.g., 135, 206, 235)
  // Auto: Light Orange (e.g., 255, 200, 100)
  uint32_t color = isManualOverride ? cloudStrip.Color(135, 206, 235) : cloudStrip.Color(255, 200, 100);
  
  for (int k=0; k<2; k++){ // Blink twice
      for (int i = 0; i < NUM_CLOUD_LEDS; i++) {
        cloudStrip.setPixelColor(i, color);
      }
      cloudStrip.show();
      delay(250); // Quicker blinks
      for (int i = 0; i < NUM_CLOUD_LEDS; i++) {
        cloudStrip.setPixelColor(i, 0); // Off
      }
      cloudStrip.show();
      delay(250);
  }
  // After indication, the main loop's applyCrossfade will restore/continue the stateful fade.
  // Force an update to the current state's target colors
  if(ledsEnabled && isInitialized) applyCurrentLedState(true);
  else {
      setSunTargetColor(sunStrip.Color(0,0,0));
      setCloudTargetColor(cloudStrip.Color(0,0,0));
  }
}

void ensureWiFiConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    // WiFi was connected, now it's not. Try to reconnect.
    WiFi.disconnect(); // Explicitly disconnect before reconnecting
    WiFi.reconnect();
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) { // 15 sec timeout
      delay(500);
    }

    if (WiFi.status() != WL_CONNECTED) {
      indicateWiFiStatus(false); // Indicate failure (abrupt)
      ESP.restart(); // Restart if reconnect fails
    } else {
      indicateWiFiStatus(true); // Indicate success (abrupt)
    }
  }
}

void updateWeather() {
  if (isManualOverride) { // Don't fetch weather if in manual mode
      applyCurrentLedState(); // Ensure manual state is displayed
      return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" 
                 + city + "," + countryCode 
                 + "&appid=" + weatherApiKey 
                 + "&units=metric";

    int retries = 3;
    int httpCode = 0; // Initialize httpCode

    while (retries > 0) {
      http.begin(url);
      httpCode = http.GET();

      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(2048); // Increased size slightly for safety
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
          // JSON parsing failed
          http.end();
          retries--;
          delay(1000 * (4 - retries)); // Exponential backoff for retries
          continue; // Try again
        }

        // Robust JSON field checking
        if (!doc.containsKey("weather") || !doc["weather"].is<JsonArray>() || doc["weather"].size() == 0 ||
            !doc["weather"][0].is<JsonObject>() || !doc["weather"][0].containsKey("description") ||
            !doc["weather"][0]["description"].is<const char*>()) {
          // Invalid weather data structure
          http.end();
          retries--;
          delay(1000 * (4 - retries));
          continue; // Try again
        }
        
        const char* descCond = doc["weather"][0]["description"];
        String descStr = String(descCond);
        descStr.toLowerCase();
        http.end(); // End HTTP connection as soon as data is retrieved and parsed

        String newState = "";
        if (descStr == "clear sky") newState = "clear";
        else if (descStr == "few clouds" || descStr == "scattered clouds") newState = "clear+cloud";
        else if (descStr == "broken clouds" || descStr == "overcast clouds" ||
                 descStr == "mist" || descStr == "haze" || descStr == "fog" || descStr == "squalls" ||
                 descStr == "smoke" || descStr == "sand" || descStr == "dust" || descStr == "ash" || descStr == "tornado") newState = "cloud";
        else if (descStr.indexOf("rain") != -1 || descStr.indexOf("drizzle") != -1) newState = "cloud"; // Simple rain = cloud for now
        else if (descStr.indexOf("thunderstorm") != -1) newState = "thunder";
        else if (descStr.indexOf("snow") != -1 || descStr.indexOf("sleet") != -1) newState = "cloud"; // Snow/sleet = cloud
        else newState = lastKnownState; // Default to last known if unrecognized

        if (newState != currentState || !isInitialized) {
          currentState = newState;
          lastKnownState = newState; // Update lastKnownState with new valid weather
          isInitialized = true;     // Now we have a valid state
          applyCurrentLedState(true); // Update LEDs with fade
          if (currentState == "thunder") lastThunder = millis(); // Reset thunder timer
        }
        return; // Successfully updated weather

      } else { // HTTP GET failed
        http.end();
        retries--;
        if (retries > 0) {
            delay(1000 * (4-retries)); // Wait longer before retrying
        }
      }
    } // End of retry loop

    // If all retries failed or other non-OK HTTP code
    if (currentState == "" && lastKnownState != "") { // If never got a good reading but have a fallback
        currentState = lastKnownState;
    } else if (currentState == "") { // Absolute first run, no fallback, default to clear
        currentState = "clear"; 
        lastKnownState = "clear";
    }
    // No change to 'currentState' if retries fail, effectively using the last good 'currentState' or 'lastKnownState'
    isInitialized = true; // Consider initialized even if update failed, to show last known state
    applyCurrentLedState(); // Apply the current (possibly old) state

  } else { // WiFi not connected
    if (currentState == "" && lastKnownState != "") currentState = lastKnownState;
    else if (currentState == "") currentState = "clear"; // Default if no state ever known
    isInitialized = true;
    applyCurrentLedState();
  }
}


void applyCurrentLedState(bool forceUpdate) {
    if (!ledsEnabled && !forceUpdate) { // If LEDs are off, target is black (handled by ledsEnabled logic)
        if (sunFadingInProgress || cloudFadingInProgress) { /* Allow fade to black to complete */ }
        else { // Ensure they are set to black if no fade is active
            bool changed = false;
            for(int i=0; i<NUM_SUN_LEDS; i++) if(sunCurrentColors[i] != 0) {sunStrip.setPixelColor(i,0); sunCurrentColors[i]=0; changed=true;}
            if(changed) sunStrip.show();
            changed = false;
            for(int i=0; i<NUM_CLOUD_LEDS; i++) if(cloudCurrentColors[i] != 0) {cloudStrip.setPixelColor(i,0); cloudCurrentColors[i]=0; changed=true;}
            if(changed) cloudStrip.show();
        }
        return;
    }
    if (!isInitialized && !forceUpdate) return; // Don't do anything if not initialized yet unless forced

    // Determine target colors based on currentState
    if (currentState == "clear") {
        setSunTargetColorsRandomBrightness(255, 165, 0); // Sunny Orange-Yellow
        setCloudTargetColor(cloudStrip.Color(0, 0, 0));   // Clouds off
    } else if (currentState == "clear+cloud") {
        setSunTargetColorsRandomBrightness(255, 165, 0); // Sunny
        setCloudTargetColor(cloudStrip.Color(150, 150, 150)); // Clouds grey
    } else if (currentState == "cloud") {
        setSunTargetColor(sunStrip.Color(70, 70, 70));    // Sun very dim/greyish
        setCloudTargetColor(cloudStrip.Color(150, 150, 150)); // Clouds grey
    } else if (currentState == "thunder") {
        setSunTargetColor(sunStrip.Color(40, 40, 60));    // Sun very dim, bluish/grey
        setCloudTargetColor(cloudStrip.Color(100, 100, 120)); // Clouds dark grey/blue base for thunder
                                                         // Lightning animation will override this temporarily
    } else { // Default or unknown state - fade to off or last known
        setSunTargetColor(sunStrip.Color(0,0,0));
        setCloudTargetColor(cloudStrip.Color(0,0,0));
    }
}


void showLightningCloud() {
  // This animation is abrupt and directly controls the strip.
  // It should set the cloud back to its base "thunder" color when done.
  const int flashes = random(2, 5); // Random number of flashes
  uint32_t baseCloudColorForThunder = cloudStrip.Color(100, 100, 120);


  for (int i = 0; i < flashes; i++) {
    if (lightningStyle) { // Sequential flash
      for (int j = 0; j < NUM_CLOUD_LEDS; j++) { // Light up sequentially
        cloudStrip.setPixelColor(j, cloudStrip.Color(255, 255, 255));
        if (j > 0) cloudStrip.setPixelColor(j - 1, baseCloudColorForThunder); // Turn previous off or to base
        cloudStrip.show();
        delay(random(30, 70));
      }
      // Hold last LED lit briefly or turn all off
      delay(random(50,100));
      for (int j = 0; j < NUM_CLOUD_LEDS; j++) cloudStrip.setPixelColor(j, baseCloudColorForThunder);
      cloudStrip.show();
      delay(random(100, 200));

    } else { // All LEDs flash at once
      for (int j = 0; j < NUM_CLOUD_LEDS; j++) cloudStrip.setPixelColor(j, cloudStrip.Color(255, 255, 255));
      cloudStrip.show();
      delay(random(50, 150));
      for (int j = 0; j < NUM_CLOUD_LEDS; j++) cloudStrip.setPixelColor(j, baseCloudColorForThunder);
      cloudStrip.show();
      delay(random(50, 150));
    }
  }
  
  // After flashes, ensure the cloud strip's target color is set back to the base thunder cloud color.
  // The applyCrossfade in the main loop will then ensure it smoothly maintains or returns to this.
  // We also update currentCloudColors to reflect the end of the lightning animation.
  for(int i=0; i < NUM_CLOUD_LEDS; i++) {
      cloudStrip.setPixelColor(i, baseCloudColorForThunder); // Ensure visually it's this color
      cloudCurrentColors[i] = baseCloudColorForThunder; // Update our record of current color
  }
  cloudStrip.show();
  setCloudTargetColor(baseCloudColorForThunder); // Re-affirm target for fading logic
  
  lightningStyle = !lightningStyle; // Toggle for next time
}
