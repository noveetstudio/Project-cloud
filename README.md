# project-float

A weather-responsive LED display using an ESP32-C3 Super Mini, WS2812B LEDs, and a TTP223 touch sensor. This project visualizes weather conditions (clear, cloudy, thunder) with colorful LED patterns, controlled manually or via an OpenWeatherMap API, with WiFi connectivity and touch-based mode switching.

## Table of Contents
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Touch Controls](#touch-controls)
- [Customization](#customization)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgments](#acknowledgments)

## Features
- Displays weather states (clear, partly cloudy, cloudy, thunder) using two NeoPixel LED strips (sun and cloud).
- Connects to WiFi and fetches real-time weather data from OpenWeatherMap API.
- Manual mode to cycle through LED patterns independently of weather.
- Persistent WiFi credentials across power cycles.
- Alternating lightning effects (all-flash and sequential) for thunder state.
- Touch sensor (TTP223) for on/off toggling, mode switching, and WiFi reset.

## Hardware Requirements
- **ESP32-C3 Super Mini**: Microcontroller with WiFi capability.
- **NeoPixel LED Strips**:
  - 4 LEDs for sun (connected to GPIO 2).
  - 5 LEDs for cloud (connected to GPIO 4).
- **TTP223 Touch Sensor Module**: Connected to GPIO 3 with a 10kΩ pull-down resistor.
- **Power Supply**: 5V with at least 1A capacity (e.g., USB power bank or adapter).
- **Capacitor**: 1000µF, 6.3V or higher (optional, for NeoPixel power stability).
- **Breadboard and Jumper Wires**: For prototyping connections.
- **Computer with Arduino IDE**: For programming the ESP32-C3.

### Pin Connections
- **Sun Strip (NeoPixel)**: Data to GPIO 2 (A2), 5V, GND.
- **Cloud Strip (NeoPixel)**: Data to GPIO 4 (A4), 5V, GND.
- **TTP223**: SIG/TOUCH to GPIO 3, VCC to 3.3V, GND to GND, 10kΩ pull-down to GND.
- **ESP32-C3 Power**: 5V and GND to power supply, shared GND with NeoPixels.

## Software Requirements
- **Arduino IDE**: Version 2.x or later.
- **Libraries**:
  - `WiFi.h` (included with ESP32 board support).
  - `HTTPClient.h` (included with ESP32 board support).
  - `ArduinoJson.h` (install via Library Manager).
  - `Adafruit_NeoPixel.h` (install via Library Manager).
  - `WiFiManager.h` (install via Library Manager).
- **Board Support**: ESP32 by Espressif (install via Boards Manager).

## Installation
1. **Install Arduino IDE**:
   - Download and install from [arduino.cc](https://www.arduino.cc/en/software).

2. **Add ESP32 Board Support**:
   - Open Arduino IDE, go to `File > Preferences`.
   - Add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` to "Additional Boards Manager URLs".
   - Go to `Tools > Board > Boards Manager`, search for "ESP32", and install the Espressif ESP32 package.

3. **Install Libraries**:
   - Open `Sketch > Include Library > Manage Libraries`.
   - Search and install:
     - `ArduinoJson` by Benoit Blanchon.
     - `Adafruit NeoPixel` by Adafruit.
     - `WiFiManager` by tzapu.

4. **Connect Hardware**:
   - Wire the ESP32-C3, NeoPixels, and TTP223 as per the pin connections above.
   - Ensure a stable 5V power supply with shared GND.

5. **Upload Code**:
   - Open the `WeatherLED.ino` sketch in Arduino IDE.
   - Select your ESP32-C3 board under `Tools > Board > ESP32 Arduino > ESP32C3 Dev Module`.
   - Choose the correct port under `Tools > Port`.
   - Upload the sketch.

6. **Configure WiFi**:
   - On first boot, the ESP32 will create a "WeatherLED_AP" access point.
   - Connect to it with your phone or laptop, enter your home WiFi credentials, and provide:
     - Weather API Key (from [OpenWeatherMap](https://openweathermap.org/api)).
     - City and Country Code (e.g., "Seville", "ES").

## Usage
- **Power On**: LEDs will light up orange briefly, then show the current weather state (or last known state) after green WiFi connection flashes.
- **LED States**:
  - **Clear**: Sun strip glows yellow.
  - **Clear + Cloud**: Sun strip yellow, cloud strip gray.
  - **Cloud**: Cloud strip gray.
  - **Thunder**: Cloud strip gray with lightning flashes every 30 seconds (alternating all-flash and sequential).

## Touch Controls
- **< 1s**: Toggle LEDs on/off (no flash).
- **1-5s**: Cycle manual states (double blue flashes on cloud strip).
  - Cycles: "clear" → "clear+cloud" → "cloud" → "thunder" → "clear".
- **5-10s**: Switch to API mode (single green flash on sun strip, fetches weather).
- **> 10s**: Reset WiFi settings (AP reappears for reconfiguration).

## Customization
- **API Key and Location**:
  - Update `custom_api_key`, `custom_city`, and `custom_country` in the code with your OpenWeatherMap API key and desired location.
- **LED Pins**:
  - Modify `SUN_PIN` and `CLOUD_PIN` if using different GPIO pins.
- **Update Interval**:
  - Change `updateInterval` (in milliseconds) to adjust how often the weather updates (default: 10 minutes).
- **Lightning Effect**:
  - Adjust the `delay(50)` in `showLightningCloud()` to change the speed of the sequential flash.

## Contributing
Contributions are welcome! Please fork the repository and submit pull requests for:
- Bug fixes.
- New features (e.g., additional weather states, color customization).
- Documentation improvements.

## License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Acknowledgments
- Built with guidance from Grok 3 by xAI.
- Thanks to the Arduino, Adafruit, and WiFiManager communities for their libraries and support.
- Weather data provided by [OpenWeatherMap](https://openweathermap.org/).
