#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiManager.h>
#include <Preferences.h>

#define SUN_PIN      2  // GPIO 2 (A2)
#define NUM_SUN_LEDS 4
Adafruit_NeoPixel sunStrip(NUM_SUN_LEDS, SUN_PIN, NEO_GRB + NEO_KHZ800);

#define CLOUD_PIN      4  // GPIO 4 (A4)
#define NUM_CLOUD_LEDS 5
Adafruit_NeoPixel cloudStrip(NUM_CLOUD_LEDS, CLOUD_PIN, NEO_GRB + NEO_KHZ800);

#define TOUCH_PIN    3  // GPIO 3 (A3)

bool ledsEnabled = true;
bool lastTouchState = LOW;
unsigned long touchStartTime = 0;
bool touchTimingStarted = false;
unsigned long startupTime = 0;

unsigned long lastUpdate = 0;
unsigned long lastThunder = 0;
String currentState = "";
String lastKnownState = "clear";
bool isManualOverride = false;
bool isInitialized = false;
const unsigned long updateInterval = 10 * 60 * 1000;
bool lightningStyle = false; // Toggle between all-flash and sequential-flash

WiFiManagerParameter custom_api_key("api", "Weather API Key", "2daec8d0525b0e600598bb69cfdec452", 40);
WiFiManagerParameter custom_city("city", "City", "Seville", 20);
WiFiManagerParameter custom_country("country", "Country Code", "ES", 3);
String weatherApiKey;
String city;
String countryCode;

Preferences preferences;

void saveConfigCallback() {
  preferences.begin("weatherled", false);
  preferences.putString("city", custom_city.getValue());
  preferences.putString("countryCode", custom_country.getValue());
  preferences.putString("apiKey", custom_api_key.getValue());
  preferences.end();
}

void updateWeather();
void updateLEDs();
void ensureWiFiConnected();
void clearLEDs();
void showSunny();
void showCloudy();
void showLightningCloud();
void indicateMode();
void indicateWiFiStatus(bool connected);

void setup() {
  pinMode(TOUCH_PIN, INPUT);

  sunStrip.begin();
  cloudStrip.begin();

  // Test both strips immediately
  for (int i = 0; i < NUM_SUN_LEDS; i++) {
    sunStrip.setPixelColor(i, sunStrip.Color(255, 165, 0)); // Orange
  }
  for (int i = 0; i < NUM_CLOUD_LEDS; i++) {
    cloudStrip.setPixelColor(i, sunStrip.Color(255, 165, 0)); // Orange
  }
  sunStrip.show();
  cloudStrip.show();
  delay(1000);
  clearLEDs();

  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.addParameter(&custom_api_key);
  wm.addParameter(&custom_city);
  wm.addParameter(&custom_country);

  // Load saved preferences
  preferences.begin("weatherled", true);
  String savedCity = preferences.getString("city", "Seville");
  String savedCountry = preferences.getString("countryCode", "ES");
  String savedApiKey = preferences.getString("apiKey", "2daec8d0525b0e600598bb69cfdec452");
  preferences.end();

  custom_city.setValue(savedCity.c_str(), 20);
  custom_country.setValue(savedCountry.c_str(), 3);
  custom_api_key.setValue(savedApiKey.c_str(), 40);

  wm.setSaveConfigCallback(saveConfigCallback);

  bool connected = wm.autoConnect("WeatherLED_AP", NULL);
  indicateWiFiStatus(connected);

  if (!connected) {
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < NUM_SUN_LEDS; j++) {
        sunStrip.setPixelColor(j, sunStrip.Color(255, 165, 0)); // Orange
      }
      sunStrip.show();
      delay(500);
      clearLEDs();
      delay(500);
    }
    while (true) {
      delay(1000);
    }
  }

  weatherApiKey = custom_api_key.getValue();
  city = custom_city.getValue();
  countryCode = custom_country.getValue();

  isInitialized = true;
  updateWeather();
  delay(100);
  lastTouchState = digitalRead(TOUCH_PIN);
  startupTime = millis();
}

void loop() {
  ensureWiFiConnected();

  if (millis() - lastUpdate >= updateInterval) {
    updateWeather();
    lastUpdate = millis();
  }

  if (millis() - startupTime > 5000) {
    bool touchState = digitalRead(TOUCH_PIN);
    if (touchState == HIGH && lastTouchState == LOW && !touchTimingStarted) {
      touchStartTime = millis();
      touchTimingStarted = true;
    }
    if (touchState == LOW && lastTouchState == HIGH) {
      unsigned long touchDuration = millis() - touchStartTime;
      if (touchDuration < 1000) {
        ledsEnabled = !ledsEnabled;
        if (!ledsEnabled) clearLEDs();
      } else if (touchDuration < 5000) {
        isManualOverride = true;
        isInitialized = true;
        if (currentState == "clear") currentState = "clear+cloud";
        else if (currentState == "clear+cloud") currentState = "cloud";
        else if (currentState == "cloud") currentState = "thunder";
        else currentState = "clear";
        lastKnownState = currentState;
        clearLEDs();
        indicateMode();
      } else if (touchDuration < 10000) {
        isManualOverride = false;
        isInitialized = true;
        updateWeather();
        lastUpdate = millis();
      } else {
        WiFiManager wm;
        wm.resetSettings();
        ESP.restart();
      }
      touchTimingStarted = false;
    }
    lastTouchState = touchState;
  }

  // Enforce LED off state when disabled
  if (!ledsEnabled) {
    sunStrip.setBrightness(0); // Disable NeoPixel output
    cloudStrip.setBrightness(0);
    clearLEDs(); // Force LEDs off
    delay(10); // Short delay to ensure stability
  } else if (isInitialized) {
    sunStrip.setBrightness(255); // Restore brightness when enabled
    cloudStrip.setBrightness(255);
    updateLEDs();
  }
  delay(100);
}

void indicateWiFiStatus(bool connected) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < NUM_SUN_LEDS; j++) {
      if (connected) {
        sunStrip.setPixelColor(j, sunStrip.Color(0, 255, 0)); // Green for success
      } else {
        sunStrip.setPixelColor(j, sunStrip.Color(255, 0, 0)); // Red for failure
      }
    }
    sunStrip.show();
    delay(500);
    clearLEDs();
    delay(500);
  }
}

void updateLEDs() {
  if (currentState == "clear") {
    showSunny();
  }
  else if (currentState == "clear+cloud") {
    showSunny();
    showCloudy();
  }
  else if (currentState == "cloud") {
    showCloudy();
  }
  else if (currentState == "thunder") {
    if (millis() - lastThunder >= 30000) {
      showLightningCloud();
      lastThunder = millis();
    }
    else {
      showCloudy();
    }
  }
}

void indicateMode() {
  if (isManualOverride) {
    for (int i = 0; i < NUM_CLOUD_LEDS; i++) {
      cloudStrip.setPixelColor(i, cloudStrip.Color(0, 0, 255));
    }
    cloudStrip.show();
    delay(200);
    clearLEDs();
    delay(200);
    for (int i = 0; i < NUM_CLOUD_LEDS; i++) {
      cloudStrip.setPixelColor(i, cloudStrip.Color(0, 0, 255));
    }
    cloudStrip.show();
    delay(200);
    clearLEDs();
  } else {
    for (int i = 0; i < NUM_CLOUD_LEDS; i++) {
      cloudStrip.setPixelColor(i, sunStrip.Color(0, 255, 0));
    }
    cloudStrip.show();
    delay(200);
    clearLEDs();
  }
  delay(200);
}

void ensureWiFiConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.reconnect();
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
      delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
      indicateWiFiStatus(false);
      ESP.restart();
    } else {
      indicateWiFiStatus(true);
    }
  }
}

void updateWeather() {
  if (WiFi.status() == WL_CONNECTED && !isManualOverride) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" 
                 + city + "," + countryCode
                 + "&appid=" + weatherApiKey
                 + "&units=metric";

    int retries = 3;
    int delayMs = 1000;

    while (retries > 0) {
      http.begin(url);
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
          const char* descCond = doc["weather"][0]["description"];
          String descStr = String(descCond);
          descStr.toLowerCase();

          String newState = "";
          if (descStr == "clear sky") newState = "clear";
          else if (descStr == "few clouds" || descStr == "scattered clouds") newState = "clear+cloud";
          else if (descStr == "broken clouds" || descStr == "overcast clouds" ||
                   descStr == "mist" || descStr == "haze" || descStr == "fog") newState = "cloud";
          else newState = "thunder";

          if (newState != currentState) {
            currentState = newState;
            lastKnownState = newState;
            isInitialized = true;
            if (ledsEnabled) {
              clearLEDs();
              indicateMode();
              updateLEDs();
              if (currentState == "thunder") lastThunder = millis();
            }
          }
          http.end();
          return;
        }
      }
      http.end();
      retries--;
      delay(delayMs);
      delayMs *= 2;
    }
    currentState = lastKnownState;
  } else {
    currentState = lastKnownState;
  }
}

void showSunny() {
  float brightness = random(85, 101) / 100.0;
  uint8_t r = (uint8_t)(255 * brightness);
  uint8_t g = (uint8_t)(165 * brightness);
  uint8_t b = 0;
  for (int i = 0; i < NUM_SUN_LEDS; i++) {
    sunStrip.setPixelColor(i, sunStrip.Color(r, g, b));
  }
  sunStrip.show();
}

void showCloudy() {
  uint32_t color = cloudStrip.Color(150, 150, 150);
  for (int i = 0; i < NUM_CLOUD_LEDS; i++) {
    cloudStrip.setPixelColor(i, color);
  }
  cloudStrip.show();
}

void showLightningCloud() {
  const int flashes = 3;
  for (int i = 0; i < flashes; i++) {
    if (lightningStyle) {
      // Sequential flash (LED 1 to 5)
      for (int j = 0; j < NUM_CLOUD_LEDS; j++) {
        cloudStrip.setPixelColor(j, cloudStrip.Color(255, 255, 255));
        cloudStrip.show();
        delay(50);
        cloudStrip.setPixelColor(j, 0);
        if (j > 0) cloudStrip.setPixelColor(j - 1, cloudStrip.Color(255, 255, 255));
      }
      cloudStrip.show();
      delay(random(50, 150));
      for (int j = 0; j < NUM_CLOUD_LEDS; j++) {
        cloudStrip.setPixelColor(j, 0);
      }
      cloudStrip.show();
      delay(random(50, 150));
    } else {
      // All LEDs flash at once
      for (int j = 0; j < NUM_CLOUD_LEDS; j++) {
        cloudStrip.setPixelColor(j, cloudStrip.Color(255, 255, 255));
      }
      cloudStrip.show();
      delay(random(50, 150));
      for (int j = 0; j < NUM_CLOUD_LEDS; j++) {
        cloudStrip.setPixelColor(j, 0);
      }
      cloudStrip.show();
      delay(random(50, 150));
    }
  }
  showCloudy();
  lightningStyle = !lightningStyle;
}

void clearLEDs() {
  for (int i = 0; i < NUM_SUN_LEDS; i++) {
    sunStrip.setPixelColor(i, 0);
  }
  sunStrip.show();
  for (int i = 0; i < NUM_CLOUD_LEDS; i++) {
    cloudStrip.setPixelColor(i, 0);
  }
  cloudStrip.show();
}
