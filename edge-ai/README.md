# Edge AI for Smart Irrigation System

This directory contains the edge AI implementation for the smart irrigation system, enabling local decision-making with reduced latency and offline operation capabilities.

## Directory Structure

```
edge-ai/
├── arduino-ml/     # Phase 1: Embedded ML on Arduino
├── esp32-ml/       # Phase 2: TensorFlow Lite on ESP32
├── pi-gateway/     # Phase 3: Advanced edge gateway
├── models/         # Pre-trained and quantized models
└── tools/          # Utilities for model conversion and deployment
```

## Implementation Phases

### Phase 1: Arduino Embedded ML (Weeks 1-2)

- **Location**: `arduino-ml/`
- **Capabilities**: Decision trees, lookup tables, anomaly detection
- **Response Time**: <100ms
- **Memory**: <30KB Flash, <1KB RAM

### Phase 2: ESP32 TensorFlow Lite (Weeks 3-4)

- **Location**: `esp32-ml/`
- **Capabilities**: Moisture prediction, weather patterns, sensor fusion
- **Response Time**: <1s
- **Memory**: <100KB Flash, <20KB RAM

### Phase 3: Raspberry Pi Gateway (Weeks 5-6)

- **Location**: `pi-gateway/`
- **Capabilities**: Computer vision, local LLM, federated learning
- **Response Time**: <5s
- **Memory**: <5MB models

### Phase 4: System Integration (Weeks 7-8)

- Multi-tier decision making
- Graceful degradation
- Energy optimization

## Hardware Requirements

### Minimum (ESP32 Upgrade)

- Arduino Uno (existing)
- ESP32 DevKit (240MHz, 520KB RAM, 4MB Flash)
- MicroSD card module
- **Cost**: ~$15

### Recommended (Full Edge AI)

- Arduino Uno (existing)
- ESP32 DevKit
- Raspberry Pi Zero 2 W
- 16GB MicroSD card
- USB camera module
- **Cost**: ~$50

### Advanced (High Performance)

- Arduino Mega 2560
- ESP32-S3 with PSRAM
- Raspberry Pi 4 (2GB)
- Pi Camera Module v2
- **Cost**: ~$150

## Key Benefits

- **Latency**: 2-5s → <100ms
- **Offline**: 0% → 70% functionality
- **Energy**: 80% reduction in WiFi usage
- **Cost**: 90% reduction in API calls
- **Privacy**: Local data processing

## Getting Started

1. **Phase 1**: See `arduino-ml/README.md` for embedded ML implementation
2. **Phase 2**: See `esp32-ml/README.md` for TensorFlow Lite setup
3. **Phase 3**: See `pi-gateway/README.md` for advanced edge AI
4. **Integration**: Follow the main implementation guide

## Model Performance

| Model                    | Size  | Accuracy | Latency |
| ------------------------ | ----- | -------- | ------- |
| Decision Tree (Arduino)  | <30KB | >90%     | <10ms   |
| LSTM Moisture Prediction | <50KB | >85%     | <100ms  |
| Anomaly Detection        | <20KB | >95%     | <50ms   |
| Plant Health CV          | <5MB  | >80%     | <500ms  |

## Communication Protocol

```
Arduino ←→ ESP32 ←→ Pi Gateway ←→ Cloud
   |         |         |           |
  100ms     1s        5s        When available
```

Each tier handles decisions within its capability, with escalation to higher tiers for complex analysis.
