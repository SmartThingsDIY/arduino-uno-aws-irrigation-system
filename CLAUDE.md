# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Smart irrigation system with edge AI/ML capabilities, combining Arduino Uno hardware control, ESP32 edge AI gateway, and optional AWS IoT cloud services. The system operates autonomously with local decision-making or leverages cloud services for advanced analytics.

## CRITICAL SAFETY REQUIREMENTS

- Always build for production. Never use mock data.
- All ML models MUST include validation and fallback mechanisms
- See PRODUCTION_FIXES.md for critical safety improvements
- Memory usage on Arduino Uno currently at 153.6% - target <100%
- All sensor readings MUST be bounds-checked (moisture: 0-1023, temp: -40-80°C, humidity: 0-100%)
- Pump operations MUST respect failsafes: 30s max duration, 5min cooldown
- Emergency stop commands ("stop", "emergency") MUST halt all pumps immediately

## Key Commands

### Arduino Uno (Embedded ML)

- **Build**: `platformio run -e uno`
- **Upload**: `platformio run -e uno --target upload`
- **Monitor**: `platformio device monitor -e uno` (9600 baud)
- **Clean**: `platformio run -e uno --target clean`

### ESP32 (Edge AI Gateway)

- **Build**: `platformio run -e esp32`
- **Upload**: `platformio run -e esp32 --target upload`
- **Monitor**: `platformio device monitor -e esp32` (115200 baud)
- **Clean**: `platformio run -e esp32 --target clean`

### ESP32 Standalone (from edge-ai/esp32-ml/)

```bash
cd edge-ai/esp32-ml
platformio run              # Build
platformio run -t upload    # Upload
```

### Multi-Target Development

- **Build both targets**: `platformio run`
- **Run tests**: `platformio test`

### Serial Commands for Testing

Send via serial monitor (9600 baud for Arduino, 115200 for ESP32):
- `status` - Print current system status
- `reset` - Reset all statistics
- `stop` or `emergency` - Emergency stop all pumps
- `debug` - Enable debug mode

## Architecture Overview

### Three-Tier Edge Computing Architecture

**Tier 1: Arduino Uno (Immediate Response <100ms)**
- 4 capacitive moisture sensors (A0-A3)
- 4-channel relay module for pump control (pins 2-5)
- DHT22 temperature/humidity sensor (pin 6)
- LDR light sensor (A4)
- Embedded ML: Decision trees, plant lookup tables, z-score anomaly detection
- Location: `src/main.cpp`, `lib/LocalMLEngine/`

**Tier 2: ESP32 (Short-term Planning <1s)**
- Dual-core 240MHz processor with 520KB RAM
- TensorFlow Lite Micro models (LSTM moisture prediction, autoencoder anomaly detection)
- WiFi connectivity for cloud communication
- Serial communication with Arduino (pins 16-17, 9600 baud)
- Location: `edge-ai/esp32-ml/`

**Tier 3: Cloud (When Available)**
- AWS IoT Core for device management
- Lambda for serverless compute
- S3 for historical data and model artifacts
- DynamoDB for time-series storage

### Data Flow & Communication

**Arduino ↔ ESP32 Protocol:**
- Hardware: SoftwareSerial pins 7-8 (Arduino) ↔ Serial1 pins 16-17 (ESP32)
- Baud rate: 9600
- Format: Compact JSON to optimize Arduino's 2KB RAM
- Rate limit: Max 1 message per 5 seconds
- Example: `{"s":1,"m":450,"t":24,"h":65,"l":580,"w":1,"a":100}`

**Processing Pipeline:**
1. Arduino reads sensors every 2 seconds
2. LocalMLEngine makes irrigation decisions using decision tree
3. Controls pumps directly (< 100ms latency)
4. Sends telemetry to ESP32 every 5 seconds
5. ESP32 runs TensorFlow Lite models for 24-hour forecasting
6. ESP32 optionally sends data to AWS IoT Core

### Critical Thresholds & Constraints

- **Moisture threshold**: Values > 450 indicate dry soil (0-1023 range)
- **Pump duration**: 30s max per cycle, 5min cooldown between cycles
- **Watchdog timer**: 8-second automatic recovery from system hangs
- **Memory limits**: Arduino has 2KB RAM (currently 153.6% usage - needs optimization)
- **Model size**: ESP32 models limited to 100KB each

### ML/AI Implementation

**Arduino Embedded ML (lib/LocalMLEngine/):**
- `DecisionTree.cpp`: 7-node irrigation decision tree
- `LookupTable.cpp`: Plant-specific thresholds for 20 plant types (stored in PROGMEM)
- `AnomalyDetector.cpp`: Z-score based sensor fault detection
- 91% accuracy compared to cloud models

**ESP32 Edge AI (edge-ai/esp32-ml/):**
- `EdgeInference.cpp`: TensorFlow Lite Micro integration
- `irrigation_models.cpp`: LSTM (608 bytes), Autoencoder (432 bytes)
- `WiFiManager.h`: Cloud connectivity and OTA updates
- Fallback decision tree when ML models fail

### Key Source Files

- `src/main.cpp`: Arduino Uno production firmware with safety features
- `src/main_legacy.cpp`: Original ESP8266 version (preserved for reference)
- `lib/LocalMLEngine/`: Arduino embedded ML library
- `edge-ai/arduino-ml/`: Standalone Arduino ML examples
- `edge-ai/esp32-ml/`: Complete ESP32 edge AI gateway
- `platformio.ini`: Multi-target build configuration

### Build Configuration

The project uses two PlatformIO configurations:
- **Root `platformio.ini`**: Builds both Arduino and ESP32 from project root
- **`edge-ai/esp32-ml/platformio.ini`**: Standalone ESP32 development
