# Build Guide - Smart Irrigation System

This project supports both Arduino Uno (embedded ML) and ESP32 (edge AI gateway) development with a unified build system.

## File Structure

```
├── platformio.ini                    # Root config - builds both targets
├── edge-ai/esp32-ml/platformio.ini   # ESP32-specific config (standalone)
├── src/main.cpp                      # Arduino Uno Edge AI firmware ✨ UPDATED
├── src/main_legacy.cpp               # Original ESP8266 version (preserved)
├── edge-ai/arduino-ml/               # Arduino ML library
└── edge-ai/esp32-ml/                 # ESP32 edge AI implementation
```

## PlatformIO Configuration Files

### 🔧 Root `platformio.ini` 
**Purpose**: Project-wide multi-target configuration
- Builds both Arduino and ESP32 from project root
- Unified CI/CD pipeline support
- Cross-platform development workflow

**Usage**:
```bash
# Build both targets
pio run

# Build specific target
pio run -e uno        # Arduino Uno
pio run -e esp32      # ESP32

# Upload to specific target
pio run -e uno -t upload
pio run -e esp32 -t upload
```

### 🔧 `edge-ai/esp32-ml/platformio.ini`
**Purpose**: Standalone ESP32 development
- Independent ESP32 development within subdirectory
- Specialized TensorFlow Lite dependencies
- Isolated testing and debugging

**Usage**:
```bash
cd edge-ai/esp32-ml
pio run                # Build ESP32 only
pio run -t upload      # Upload to ESP32
```

## Development Workflows

### 🔄 Full System Development
**Recommended for complete system work**

```bash
# From project root
pio run                           # Build both Arduino + ESP32
pio run -e uno -t upload          # Upload to Arduino
pio run -e esp32 -t upload        # Upload to ESP32
```

### 🎯 Arduino-Only Development
**For testing embedded ML features**

```bash
# From project root
pio run -e uno                    # Build Arduino
pio run -e uno -t upload          # Upload to Arduino
pio device monitor -e uno         # Monitor serial (9600 baud)
```

### 🧠 ESP32-Only Development
**For edge AI development and testing**

Option 1 - From root:
```bash
pio run -e esp32                  # Build ESP32
pio run -e esp32 -t upload        # Upload to ESP32
pio device monitor -e esp32       # Monitor serial (115200 baud)
```

Option 2 - Standalone:
```bash
cd edge-ai/esp32-ml
pio run                           # Build ESP32
pio run -t upload                 # Upload to ESP32
pio device monitor                # Monitor serial
```

## Target Environments

| Environment | Platform | Purpose | Serial | Libraries |
|-------------|----------|---------|---------|-----------|
| `uno` | Arduino Uno | Embedded ML | 9600 | LocalMLEngine, ArduinoJson |
| `esp32` | ESP32 DevKit | Edge AI Gateway | 115200 | TensorFlow Lite, WiFi, MQTT |
| `test_uno` | Arduino Uno | Unit Testing | 9600 | Test framework |
| `test_esp32` | ESP32 DevKit | Unit Testing | 115200 | Test framework |

## Build Flags

### Arduino Uno (`-e uno`)
- `-DARDUINO_UNO`: Platform identification
- `-DPHASE_1_EMBEDDED_ML`: Enable embedded ML features

### ESP32 (`-e esp32`)
- `-DESP32_EDGE_AI`: Platform identification
- `-DPHASE_2_EDGE_AI`: Enable TensorFlow Lite features
- `-DPHASE_3_WIFI_GATEWAY`: Enable WiFi and cloud features
- `-DBOARD_HAS_PSRAM`: Enable PSRAM support
- `-DCORE_DEBUG_LEVEL=3`: Enhanced debugging

## Common Commands

### Development
```bash
# Clean builds
pio run -e uno --target clean
pio run -e esp32 --target clean

# Run tests
pio test -e test_uno
pio test -e test_esp32

# Check code
pio check -e uno
pio check -e esp32
```

### Monitoring
```bash
# Arduino monitoring
pio device monitor -e uno --baud 9600

# ESP32 monitoring with exception decoder
pio device monitor -e esp32 --baud 115200 --filter esp32_exception_decoder
```

### Production Build
```bash
# Build optimized releases
pio run -e uno --target release
pio run -e esp32 --target release
```

## Why Two PlatformIO Files?

✅ **Keep Both Files** - They serve different purposes:

1. **Root `platformio.ini`**: 
   - Multi-target project management
   - CI/CD automation
   - Unified development workflow

2. **`edge-ai/esp32-ml/platformio.ini`**:
   - Standalone ESP32 development
   - Independent component testing
   - Modular development approach

This dual-configuration approach provides maximum flexibility for both integrated system development and component-specific work.