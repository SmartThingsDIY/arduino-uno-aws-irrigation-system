# ESP32 TensorFlow Lite Implementation

This directory contains the Phase 2 implementation of edge AI for the ESP32, providing advanced machine learning capabilities for moisture prediction, anomaly detection, and weather pattern recognition.

## Overview

The ESP32 ML system extends the Arduino's basic decision-making with sophisticated AI models:

- **TensorFlow Lite Micro**: Quantized neural networks for real-time inference
- **Time Series Forecasting**: LSTM models for 24-hour moisture prediction
- **Anomaly Detection**: Autoencoder models for sensor fault detection
- **Weather Pattern Recognition**: Local weather prediction using sensor trends
- **Communication Hub**: Bridge between Arduino and Raspberry Pi gateway

## ESP32 Advantages over ESP8266

The ESP32 provides significant advantages for ML workloads:

| Feature | ESP32 | ESP8266 |
|---------|-------|---------|
| CPU | Dual-core 240MHz | Single-core 80MHz |
| RAM | 520KB | 80KB |
| Flash | 4MB | 4MB |
| ML Support | TensorFlow Lite Micro | Limited |
| Bluetooth | Yes | No |
| Hardware Acceleration | Yes | No |

## Architecture

### Memory Allocation
- **Flash**: 4MB total, <500KB for ML models
- **RAM**: 520KB total, <100KB for ML runtime
- **CPU**: Dual-core allows parallel processing

### Core Components

1. **EdgeInference**: Main ML inference engine
2. **ModelManager**: TensorFlow Lite model loading and management
3. **DataBuffer**: Circular buffer for time series data
4. **CommunicationHub**: Protocol handling for Arduino and Pi Gateway
5. **WeatherPredictor**: Local weather pattern recognition

## Implementation

### Files Structure

```
esp32-ml/
├── src/
│   ├── EdgeInference.h          # Main ML inference engine
│   ├── EdgeInference.cpp        # Implementation
│   ├── ModelManager.h           # TensorFlow Lite model management
│   ├── ModelManager.cpp         # Model loading and execution
│   ├── DataBuffer.h             # Time series data management
│   ├── DataBuffer.cpp           # Circular buffer implementation
│   ├── WeatherPredictor.h       # Weather pattern recognition
│   ├── WeatherPredictor.cpp     # Local weather forecasting
│   └── CommunicationHub.h       # Protocol handling
├── examples/
│   └── esp32_edge_ai.ino        # Complete ESP32 sketch
├── models/
│   ├── moisture_lstm.tflite     # Moisture prediction model
│   ├── anomaly_autoencoder.tflite # Anomaly detection model
│   └── weather_patterns.tflite  # Weather prediction model
└── tools/
    ├── model_converter.py       # Convert to TensorFlow Lite
    └── quantizer.py             # Model quantization
```

## ML Models

### 1. Moisture Prediction Model (LSTM)
- **Type**: Long Short-Term Memory Neural Network
- **Input**: 168 hours (7 days) of sensor data
- **Output**: 24-hour moisture forecast
- **Size**: <50KB (quantized INT8)
- **Accuracy**: >85% within ±5% moisture
- **Latency**: <100ms

### 2. Anomaly Detection Model (Autoencoder)
- **Type**: Autoencoder Neural Network
- **Input**: 24-hour sensor window
- **Output**: Anomaly score (0-1)
- **Size**: <20KB (quantized INT8)
- **False Positive Rate**: <5%
- **Latency**: <50ms

### 3. Weather Pattern Model
- **Type**: Convolutional Neural Network
- **Input**: Pressure, humidity, temperature trends
- **Output**: 3-day weather forecast
- **Size**: <30KB (quantized INT8)
- **Accuracy**: >70% for basic patterns
- **Latency**: <200ms

## Hardware Requirements

### Minimum Configuration
- ESP32 DevKit (240MHz, 520KB RAM)
- MicroSD card module (for data logging)
- Connection to Arduino Uno
- **Cost**: ~$15

### Recommended Configuration
- ESP32-S3 DevKit (dual-core 240MHz, PSRAM)
- 16GB MicroSD card
- Temperature/Humidity sensor (BME280)
- Real-time clock module (DS3231)
- **Cost**: ~$35

## Communication Protocols

### Arduino ↔ ESP32
- **Protocol**: Serial UART (115200 baud)
- **Format**: JSON messages
- **Frequency**: Every 2 seconds

```json
{
  "sensor": 1,
  "moisture": 345,
  "temperature": 22.5,
  "humidity": 65.0,
  "light": 678,
  "timestamp": 1234567890
}
```

### ESP32 ↔ Pi Gateway
- **Protocol**: WiFi/HTTP or Serial
- **Format**: JSON or MessagePack
- **Frequency**: Every 10 seconds

### ESP32 ↔ Cloud (Optional)
- **Protocol**: MQTT over WiFi
- **Topic**: `irrigation/{device_id}/sensors`
- **Frequency**: Every minute

## Getting Started

### Prerequisites
- ESP-IDF or Arduino IDE with ESP32 support
- TensorFlow Lite Micro library
- ArduinoJson library
- WiFi credentials (for cloud connectivity)

### Installation

1. Install ESP32 development environment:
   ```bash
   # Arduino IDE method
   # Install ESP32 board package in Arduino IDE
   
   # PlatformIO method
   pio platform install espressif32
   ```

2. Install required libraries:
   ```bash
   pio lib install "TensorFlowLite_ESP32"
   pio lib install "ArduinoJson"
   pio lib install "WiFi"
   ```

3. Upload models to ESP32:
   ```bash
   python tools/model_uploader.py --port /dev/ttyUSB0
   ```

### Usage Example

```cpp
#include "EdgeInference.h"

EdgeInference edgeAI;

void setup() {
    Serial.begin(115200);
    
    // Initialize edge AI
    if (!edgeAI.begin()) {
        Serial.println("Failed to initialize Edge AI");
        return;
    }
    
    // Load models
    edgeAI.loadMoistureModel("moisture_lstm.tflite");
    edgeAI.loadAnomalyModel("anomaly_autoencoder.tflite");
    
    Serial.println("ESP32 Edge AI initialized");
}

void loop() {
    // Receive data from Arduino
    SensorData data = receiveFromArduino();
    
    // Add to time series buffer
    edgeAI.addSensorData(data);
    
    // Make predictions
    PredictionResult forecast = edgeAI.predict24Hours();
    float anomalyScore = edgeAI.detectAnomaly();
    
    // Send results to Pi Gateway
    sendToPiGateway(forecast, anomalyScore);
    
    delay(1000);
}
```

## Model Development

### Training Pipeline

1. **Data Collection**: Gather sensor data from multiple devices
2. **Data Preprocessing**: Normalize and prepare time series data
3. **Model Training**: Train LSTM and Autoencoder models
4. **Model Quantization**: Convert to INT8 for ESP32 deployment
5. **Model Validation**: Test accuracy and performance

### Training Script Example

```python
# Train moisture prediction model
python tools/train_moisture_model.py \
    --data sensor_data.csv \
    --sequence_length 168 \
    --forecast_horizon 24 \
    --output moisture_lstm.h5

# Convert to TensorFlow Lite
python tools/model_converter.py \
    --input moisture_lstm.h5 \
    --output moisture_lstm.tflite \
    --quantize INT8
```

## Performance Optimization

### Memory Management
- Use static allocation for buffers
- Free unused models after loading
- Optimize tensor arena size

### CPU Optimization
- Use dual-core capabilities
- Implement model caching
- Optimize inference pipelines

### Power Management
- Sleep between inferences
- Reduce WiFi transmission frequency
- Use light sleep mode when possible

## Testing

### Unit Tests
```bash
cd tests/
pio test -e esp32dev
```

### Integration Tests
```bash
cd examples/
pio run -e esp32dev -t upload
pio device monitor
```

### Performance Benchmarks
```bash
python tools/benchmark.py --model all --device esp32
```

## Troubleshooting

### Common Issues

1. **Model Loading Fails**
   - Check file size (<500KB total)
   - Verify TensorFlow Lite format
   - Ensure sufficient heap memory

2. **Inference Too Slow**
   - Reduce model complexity
   - Check tensor arena size
   - Use quantized models only

3. **WiFi Connection Issues**
   - Verify credentials
   - Check signal strength
   - Implement connection retry logic

4. **Memory Overflow**
   - Reduce buffer sizes
   - Unload unused models
   - Monitor heap usage

### Debug Mode

Enable detailed logging:
```cpp
#define DEBUG_EDGE_AI 1
#include "EdgeInference.h"
```

## Advanced Features

### Federated Learning
- On-device model updates
- Privacy-preserving improvements
- Collaborative learning without raw data sharing

### Adaptive Sampling
- Dynamic sensor reading frequency
- Energy-efficient data collection
- Context-aware sampling rates

### Edge-Cloud Synchronization
- Periodic model updates from cloud
- Graceful degradation when offline
- Efficient data synchronization

## Future Enhancements

- **Computer Vision**: Plant health analysis using ESP32-CAM
- **Voice Commands**: Local speech recognition
- **Gesture Control**: Capacitive touch sensors
- **Energy Harvesting**: Solar power integration

## Contributing

1. Follow ESP32 coding standards
2. Keep models under size limits
3. Maintain <1s inference time
4. Add comprehensive tests
5. Update documentation

## License

This project is licensed under the MIT License - see the LICENSE file for details.