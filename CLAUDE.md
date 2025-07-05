# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a smart irrigation system that combines Arduino hardware control with AWS IoT cloud services and AI/ML capabilities for intelligent plant watering.

## IMPORTANT

- Always build for production. Never use mock data.

## Key Commands

### Arduino Development

- **Build**: `platformio run`
- **Upload to Arduino**: `platformio run --target upload`
- **Serial Monitor**: `platformio device monitor` (9600 baud)
- **Clean**: `platformio run --target clean`

### Python Development

- **Setup virtual environment**: `python -m venv .venv && source .venv/bin/activate`
- **Install dependencies**: `pip install -r requirements.txt`
- **Run linting**: `flake8`
- **Format code**: `black .`
- **Run tests**: `pytest` (when tests are implemented)

## Architecture Overview

### Hardware Layer

- **Arduino Uno** with 4 moisture sensors (A0-A3) and 4-channel relay module (pins 2-5)
- **ESP8266** communicates via SoftwareSerial (pins 2,3) to send data to cloud
- Automatic watering triggered when moisture < 450 (dry threshold)

### Data Flow

1. Arduino reads sensors every 2 seconds
2. Controls pumps based on moisture thresholds
3. Sends JSON telemetry to ESP8266: `{"sensor1Value": "45.2", "pump1Status": "ON", ...}`
4. ESP8266 forwards to AWS IoT Core (separate repository)
5. Cloud services provide AI predictions, weather integration, and plant health analysis

### Key Implementation Details

- **Moisture threshold**: Values > 450 indicate dry soil requiring watering
- **Serial communication**: 9600 baud between Arduino and ESP8266
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
- ESP8266 firmware is maintained in a separate repository
- Python code follows black formatting and flake8 linting standards
