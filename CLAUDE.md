# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a smart irrigation system that combines Arduino hardware control with AWS IoT cloud services and AI/ML capabilities for intelligent plant watering.

## IMPORTANT

- Always build for production. Never use mock data.
- All ML models are production-ready with validation and fallback mechanisms
- See PRODUCTION_FIXES.md for critical safety improvements implemented

## Key Commands

### Arduino Development (Phase 1: Embedded ML)

- **Build**: `platformio run -e uno`
- **Upload to Arduino**: `platformio run -e uno --target upload`
- **Serial Monitor**: `platformio device monitor -e uno` (9600 baud)
- **Clean**: `platformio run -e uno --target clean`

### ESP32 Development (Phase 2 & 3: Edge AI Gateway)

- **Build**: `platformio run -e esp32`
- **Upload to ESP32**: `platformio run -e esp32 --target upload`
- **Serial Monitor**: `platformio device monitor -e esp32` (115200 baud)
- **Clean**: `platformio run -e esp32 --target clean`

### Multi-Target Development

- **Build Both**: `platformio run` (builds Arduino + ESP32)
- **Test All**: `platformio test`

### Production Validation & Safety (NEW)

- **Validate ML Models**: Check that TensorFlow Lite models load correctly
- **Test Fallback System**: Ensure decision tree works when ML fails
- **Memory Check**: `platformio run -e uno` (target <100% RAM usage - currently 153.6%)
- **Sanity Test**: Run model inference with known inputs
- **Safety Test**: Upload code and test emergency stop commands ("stop", "emergency")
- **Watchdog Test**: Verify 8-second automatic recovery from hangs
- **Pump Failsafe**: Test 30-second maximum watering with 5-minute cooldowns

### Python Development

- **Setup virtual environment**: `python -m venv .venv && source .venv/bin/activate`
- **Install dependencies**: `pip install -r requirements.txt`
- **Run linting**: `flake8`
- **Format code**: `black .`
- **Run tests**: `pytest` (when tests are implemented)

## Architecture Overview

### Hardware Layer

- **Arduino Uno** with 4 moisture sensors (A0-A3) and 4-channel relay module (pins 2-5)
- **ESP32** communicates via HardwareSerial (pins 16,17) for edge AI and cloud connectivity
- Automatic watering triggered when moisture < 450 (dry threshold)

### Data Flow

1. Arduino reads sensors every 2 seconds
2. Controls pumps based on moisture thresholds
3. Sends JSON telemetry to ESP32: `{"sensor": 1, "moisture": 345, "temperature": 22.5, ...}`
4. ESP32 runs edge AI models for local predictions and anomaly detection
5. ESP32 communicates with AWS IoT Core via WiFi for cloud analytics

### Key Implementation Details

- **Moisture threshold**: Values > 450 indicate dry soil requiring watering
- **Serial communication**: 9600 baud between Arduino and ESP32 (115200 for ESP32 debug)
- **JSON format**: ArduinoJson library for data serialization
- **Python version**: 3.12+ required

### AI/ML Stack

- **LLM Integration**: LangChain with GPT-3.5/4 and Llama 3
- **ML Models**: Random Forest for irrigation prediction, LSTM for time series
- **Computer Vision**: OpenCV + YOLO for plant health analysis
- **Model Deployment**: TensorFlow Lite for edge inference

### AWS Services Used

- **IoT Core**: Device management and data ingestion
- **Lambda**: Serverless compute for data processing
- **S3**: Storage for images and model artifacts
- **DynamoDB**: Time-series sensor data storage

### Important Files

- `src/main.cpp`: Core Arduino firmware with sensor reading and pump control logic
- `data/irrigation_best_practices.txt`: Domain knowledge for AI assistant
- `data/plant_water_requirements.txt`: Plant-specific watering database
- `data/sample_data.csv`: Training data for ML models

### Development Notes

- Always test Arduino code with serial monitor before deploying
- Ensure moisture sensor calibration matches your soil type
- ESP32 firmware with edge AI is included in `edge-ai/esp32-ml/`
- Python code follows black formatting and flake8 linting standards
