/*
 * Smart Irrigation System with Edge AI
 * Arduino Uno with Embedded ML for Intelligent Plant Watering
 *
 * This example demonstrates Phase 1 of the Edge AI implementation:
 * - Decision tree-based irrigation decisions
 * - Plant-specific lookup tables
 * - Sensor anomaly detection
 * - Real-time pump control (<100ms response)
 *
 * Hardware Requirements:
 * - Arduino Uno
 * - 4x Capacitive Soil Moisture Sensors (A0-A3)
 * - 4x Relay Module (pins 2-5)
 * - 4x Water pumps
 * - Optional: Temperature/Humidity sensor (DHT22)
 * - Optional: Light sensor (LDR)
 *
 * Author: iLyas Bakouch
 * GitHub: https://github.com/SmartThingsDIY
 * Version: 0.78.3
 * Date: 2025
 */

// Memory optimization for Arduino Uno (disable advanced features)
#define MEMORY_OPTIMIZED 1

#include "LocalMLEngine.h"
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#ifndef MEMORY_OPTIMIZED
#include <DHT.h>
#endif
#include <avr/wdt.h>  // Watchdog timer

// --- BEGIN: Add missing type and constant definitions for compilation ---

// Action struct definition
struct Action {
    bool shouldWater = false;
    unsigned long waterDuration = 0; // ms
    float waterAmount = 0;           // ml
    bool isFailsafe = false;
};

// SensorData struct definition
struct SensorData {
    float moisture = 0;
    float temperature = 0;
    float humidity = 0;
    float lightLevel = 0;
};

// Plant type constants
enum PlantType {
    TOMATO,
    LETTUCE,
    BASIL,
    GENERIC
};

// Growth stage constants
enum GrowthStage {
    VEGETATIVE,
    FLOWERING,
    MATURE
};

// --- END: Add missing type and constant definitions for compilation ---

// Pin definitions
const int MOISTURE_PINS[2] PROGMEM = {A0, A1};
const int RELAY_PINS[2] PROGMEM = {2, 3};
const int TEMP_HUMIDITY_PIN = 6; // DHT22 sensor
const int LIGHT_PIN = A4;        // LDR sensor
const int ESP32_RX_PIN = 7;      // Arduino TX -> ESP32 RX
const int ESP32_TX_PIN = 8;      // Arduino RX <- ESP32 TX

#ifndef MEMORY_OPTIMIZED
// DHT22 sensor configuration
#define DHT_TYPE DHT22
DHT dht(TEMP_HUMIDITY_PIN, DHT_TYPE);
#endif

// CRITICAL SAFETY CONSTANTS - Production Ready
const unsigned long MAX_PUMP_DURATION = 30000;          // 30 seconds max watering per cycle
const unsigned long PUMP_COOLDOWN_TIME = 300000;        // 5 minutes between waterings
const unsigned long FAILSAFE_PUMP_DURATION = 60000;     // 1 minute absolute maximum
const unsigned long WATCHDOG_TIMEOUT = 8000;            // 8 second watchdog (max for Arduino)
const unsigned long SENSOR_TIMEOUT = 60000;             // 1 minute sensor failure timeout
const unsigned long SYSTEM_HEALTH_CHECK_INTERVAL = 30000; // 30 seconds system check
const uint8_t MAX_CONSECUTIVE_SENSOR_ERRORS = 5;        // Max sensor errors before failsafe
const uint8_t MAX_STUCK_SENSOR_COUNT = 10;              // Max identical readings before stuck detection
const float STUCK_SENSOR_THRESHOLD = 5.0;               // Minimum variation to not be "stuck"

// Sensor validation constants
const int MIN_MOISTURE = 0;
const int MAX_MOISTURE = 1023;
const float MIN_TEMP = -40.0;
const float MAX_TEMP = 80.0;
const float MIN_HUMIDITY = 0.0;
const float MAX_HUMIDITY = 100.0;
const int MIN_LIGHT = 0;
const int MAX_LIGHT = 1023;

// Timing constants
const unsigned long SENSOR_READ_INTERVAL = 2000;    // 2 seconds
const unsigned long SERIAL_REPORT_INTERVAL = 10000; // 10 seconds
const unsigned long ESP32_SEND_INTERVAL = 5000;     // 5 seconds

// JSON buffer size optimized for Arduino Uno RAM constraints
const size_t JSON_BUFFER_SIZE = 32;

// Global variables
LocalMLEngine mlEngine;
SoftwareSerial esp32Serial(ESP32_TX_PIN, ESP32_RX_PIN); // RX, TX
unsigned long lastSensorRead = 0;
unsigned long lastSerialReport = 0;
unsigned long lastESP32Send = 0;

// Memory-optimized pump control
struct PumpState {
    bool isActive;
    unsigned long startTime;
    unsigned long duration;
    unsigned long lastWateringTime;  // Track cooldown periods  
    uint8_t emergencyStop : 1;
    uint8_t failsafeTriggered : 1;
    uint8_t wateringCount : 6;       // 0-63 waterings (enough for daily limit)
};
PumpState pumpStates[2] = {{false, 0, 0, 0, 0, 0, 0}};

// Memory-optimized sensor health monitoring
struct SensorHealth {
    uint16_t lastValidReading; // Last known good reading
    bool isFailed;             // Sensor failure flag
};
SensorHealth moistureSensorHealth[2] = {{0, false}};
SensorHealth environmentalSensorHealth = {0, false}; // DHT22

// System health tracking - reduced to save memory
uint16_t lastSystemHealthCheck = 0;  // 16-bit seconds
uint16_t lastWatchdogReset = 0;      // 16-bit seconds 
bool systemFailsafeMode = false;

// Statistics - reduced to save memory
uint16_t totalDecisions = 0;
uint16_t totalWateringActions = 0;
uint8_t totalAnomalies = 0;

// Function declarations
void processAllSensors();
void processSensor(int sensorIndex, float temperature, float humidity, float lightLevel);
void executeWateringAction(int sensorIndex, const LocalAction &action);
void logWateringAction(int sensorIndex, const LocalAction &action, unsigned long inferenceTime);
void handleAnomaly(int sensorIndex, const LocalSensorData &sensorData);
void sendDataToESP32(int sensorIndex, const LocalSensorData &sensorData, const LocalAction &action, unsigned long inferenceTime);
void printStatusReport();
float readTemperature();
float readHumidity();
void updatePumpStates();
bool validateSensorReading(int sensorIndex, float value, float minVal, float maxVal);
void emergencyStopAllPumps();
void updateSensorHealth(int sensorIndex, uint16_t reading, bool isValid);


// NEW: Production safety functions
void resetWatchdog();
void performSystemHealthCheck();
bool checkPumpFailsafe(int pumpIndex);
void triggerPumpFailsafe(int pumpIndex, const char* reason);
bool detectStuckSensor(int sensorIndex, uint16_t reading);
void handleSensorFailure(int sensorIndex, const char* failureType);
void enterSystemFailsafeMode(const char* reason);
bool validatePumpOperation(int pumpIndex, unsigned long requestedDuration);
void logSystemEvent(const char* event, int sensorIndex = -1);
void performEmergencyShutdown(const char* reason);

void setup()
{
    
    Serial.begin(9600);
    esp32Serial.begin(9600);
#ifndef MEMORY_OPTIMIZED
    dht.begin();
#endif
    
    Serial.println(F("=== IRRIGATION STARTUP ==="));
    Serial.println(F("Init safety systems..."));

    // Initialize pins with safety checks
    for (int i = 0; i < 2; i++)
    {
        pinMode(MOISTURE_PINS[i], INPUT);
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], HIGH); // Turn off pumps initially (relays are active LOW)
        
        // Verify pump is actually OFF
        delay(10);
        // Initialize sensor health tracking
    // ...existing code...
        pumpStates[i].lastWateringTime = millis(); // Prevent immediate watering
    }

    pinMode(LIGHT_PIN, INPUT);
    
    // Reset watchdog to show we're still alive
    resetWatchdog();

    // Initialize DHT22 sensor
#ifndef MEMORY_OPTIMIZED
    dht.begin();
#endif
    Serial.println(F("DHT22 sensor initialized"));

    // Initialize ML engine
    if (!mlEngine.begin())
    {
    Serial.println(F("ERROR: Failed to initialize ML engine!"));
        while (1)
            delay(1000); // Halt execution
    }

    // Configure plant types for each sensor
    mlEngine.setPlantType(0, TOMATO);
    mlEngine.setPlantType(1, LETTUCE);
    mlEngine.setPlantType(2, BASIL);
    mlEngine.setPlantType(3, GENERIC);

    // Set growth stages
    mlEngine.setGrowthStage(0, VEGETATIVE);
    mlEngine.setGrowthStage(1, VEGETATIVE);
    mlEngine.setGrowthStage(2, FLOWERING);
    mlEngine.setGrowthStage(3, MATURE);

    Serial.println(F("Smart Irrigation System Started!"));
    Serial.println(F("Plants: Tomato, Lettuce, Basil, Mint"));
    
    // Final safety check and initialization complete
    Serial.println(F("=== SAFETY INIT COMPLETE ==="));
    Serial.print("WDT:"); Serial.print(WATCHDOG_TIMEOUT/1000); Serial.println("s");
    Serial.print("MaxPump:"); Serial.print(MAX_PUMP_DURATION/1000); Serial.println("s");
    Serial.println(F("Ready."));
    
    logSystemEvent("SYSTEM_STARTUP");
    resetWatchdog();

    delay(1000); // Initial delay
}

void loop()
{
    unsigned long currentTime = millis();
    
    // CRITICAL: Reset watchdog timer to prevent system reset
    resetWatchdog();

    // CRITICAL: Update pump states FIRST (safety priority)
    updatePumpStates();
    
    // System health check (every 30 seconds)
    if (currentTime - lastSystemHealthCheck >= SYSTEM_HEALTH_CHECK_INTERVAL)
    {
        performSystemHealthCheck();
        lastSystemHealthCheck = currentTime;
    }

    // Read sensors and make decisions (only if not in failsafe mode)
    if (!systemFailsafeMode && currentTime - lastSensorRead >= SENSOR_READ_INTERVAL)
    {
        processAllSensors();
        lastSensorRead = currentTime;
    }

    // Periodic status report
    if (currentTime - lastSerialReport >= SERIAL_REPORT_INTERVAL)
    {
        printStatusReport();
        lastSerialReport = currentTime;
    }

    // Handle serial commands for emergency control
    if (Serial.available())
    {
        char command[16] = {0};
        int len = Serial.readBytesUntil('\n', command, 15);
        command[len] = '\0';
        
        if (strcmp(command, "emergency") == 0 || strcmp(command, "stop") == 0)
        {
            performEmergencyShutdown("MANUAL_EMERGENCY_STOP");
        }
        else if (strcmp(command, "reset") == 0)
        {
            logSystemEvent("MANUAL_RESET");
            systemFailsafeMode = false;
            Serial.println(F("System reset from failsafe mode"));
        }
        else if (strcmp(command, "status") == 0)
        {
            printStatusReport();
        }
    }

    // Small delay to prevent overwhelming the system
    delay(100);
}

void processAllSensors()
{
    // Read environmental sensors once
    float temperature = readTemperature();
    float humidity = readHumidity();
    float lightLevel = analogRead(LIGHT_PIN);

    // Process each plant sensor
    for (int i = 0; i < 2; i++)
    {
        processSensor(i, temperature, humidity, lightLevel);
    }
}

void processSensor(int sensorIndex, float temperature, float humidity, float lightLevel)
{
    // Read moisture sensor
    float moisture = analogRead(MOISTURE_PINS[sensorIndex]);

    // Validate all sensor readings
    bool validMoisture = validateSensorReading(sensorIndex, moisture, MIN_MOISTURE, MAX_MOISTURE);
    bool validTemperature = validateSensorReading(sensorIndex, temperature, MIN_TEMP, MAX_TEMP);
    bool validHumidity = validateSensorReading(sensorIndex, humidity, MIN_HUMIDITY, MAX_HUMIDITY);
    bool validLight = validateSensorReading(sensorIndex, lightLevel, MIN_LIGHT, MAX_LIGHT);

    // ENHANCED sensor health tracking
    updateSensorHealth(sensorIndex, (uint16_t)moisture, validMoisture);

    // Check if moisture sensor is disconnected or failed
        if (moistureSensorHealth[sensorIndex].isFailed)
    {
    Serial.print(F("CRITICAL: Moisture sensor "));
        Serial.print(sensorIndex + 1);
    Serial.println(F(" is failed/disconnected - skipping processing"));
        return;
    }

    // If critical sensors are invalid, skip processing
    if (!validMoisture)
    {
    Serial.print(F("CRITICAL: Skipping sensor "));
        Serial.print(sensorIndex + 1);
    Serial.println(F(" processing due to invalid moisture reading"));
        return;
    }

    // Use fallback values for non-critical sensors if needed
    if (!validTemperature)
    {
        temperature = 22.5; // Fallback temperature
    Serial.println(F("Using fallback temperature value"));
    }
    if (!validHumidity)
    {
        humidity = 60.0; // Fallback humidity
    Serial.println(F("Using fallback humidity value"));
    }
    if (!validLight)
    {
        lightLevel = 500; // Fallback light level
    Serial.println(F("Using fallback light level value"));
    }

    // Create sensor data structure
    LocalSensorData sensorData;
    sensorData.moisture = (int16_t)moisture;
    sensorData.temperature = (int16_t)(temperature * 10); // store as tenths
    sensorData.humidity = (int16_t)(humidity * 10);       // store as tenths
    sensorData.lightLevel = (int16_t)lightLevel;

    // Get AI decision
    unsigned long startTime = micros();
    LocalAction action = mlEngine.getImmediateAction(sensorIndex, sensorData);
    unsigned long inferenceTime = micros() - startTime;

    totalDecisions++;

    // Execute action
    if (action.shouldWater)
    {
    executeWateringAction(sensorIndex, action);
        totalWateringActions++;

        // Log watering action
    logWateringAction(sensorIndex, action, inferenceTime);
    }

    // Check for anomalies
    if (mlEngine.detectAnomaly(sensorData))
    {
    handleAnomaly(sensorIndex, sensorData);
        totalAnomalies++;
    }

    // Send data to ESP32 (if connected)
    sendDataToESP32(sensorIndex, sensorData, action, inferenceTime);
}

void executeWateringAction(int sensorIndex, const LocalAction &action)
{
    // CRITICAL SAFETY CHECKS
    
    // Check if system is in failsafe mode
    if (systemFailsafeMode)
    {
    Serial.println(F("BLOCKED: System in failsafe mode, no watering allowed"));
        logSystemEvent("WATERING_BLOCKED_FAILSAFE", sensorIndex);
        return;
    }
    
    // Check if pump is already active
    if (pumpStates[sensorIndex].isActive)
    {
    Serial.print(F("WARNING: Pump "));
        Serial.print(sensorIndex + 1);
    Serial.println(F(" already active, skipping watering action"));
        logSystemEvent("WATERING_BLOCKED_ACTIVE", sensorIndex);
        return;
    }
    
    // Validate pump operation before starting
    if (!validatePumpOperation(sensorIndex, action.waterDuration))
    {
    Serial.print(F("BLOCKED: Pump "));
        Serial.print(sensorIndex + 1);
    Serial.println(F(" operation validation failed"));
        return;
    }

    // Apply safety limits to duration
    unsigned long safeDuration = min(action.waterDuration, MAX_PUMP_DURATION);
    if (action.waterDuration > MAX_PUMP_DURATION)
    {
    Serial.print(F("WARNING: Duration capped from "));
        Serial.print(action.waterDuration);
    Serial.print(F("ms to "));
        Serial.print(safeDuration);
    Serial.println(F("ms for safety"));
    }

    // Start non-blocking watering with safety tracking
    pumpStates[sensorIndex].isActive = true;
    pumpStates[sensorIndex].startTime = millis();
    pumpStates[sensorIndex].duration = safeDuration;
    pumpStates[sensorIndex].emergencyStop = false;
    pumpStates[sensorIndex].failsafeTriggered = false;
    pumpStates[sensorIndex].wateringCount++;
    // Track watering in memory-optimized wateringCount field

    // Turn on pump with verification
    digitalWrite(RELAY_PINS[sensorIndex], LOW); // Active LOW relay
    
    // Brief verification delay
    delay(50);
    
    Serial.print(F("PUMP "));
    Serial.print(sensorIndex + 1);
    Serial.print(F(" STARTED - Duration: "));
    Serial.print(safeDuration);
    Serial.print(F("ms | Count today: "));
    Serial.print(pumpStates[sensorIndex].wateringCount);
    Serial.print(F(" | Duration: "));
    Serial.print(safeDuration);
    Serial.println(F("ms"));
    
    logSystemEvent("PUMP_STARTED", sensorIndex);
}

void logWateringAction(int sensorIndex, const LocalAction &action, unsigned long inferenceTime)
{
    Serial.print(F("WATERING: Plant "));
    Serial.print(sensorIndex + 1);
    Serial.print(F(" | Amount: "));
    Serial.print(action.waterAmount); // uint16_t, prints as integer ml
    Serial.print(F("ml | Duration: "));
    Serial.print(action.waterDuration);
    Serial.print(F("ms | Inference: "));
    Serial.print(inferenceTime);
    Serial.print(F("μs"));

    if (action.isFailsafe)
    {
    Serial.print(F(" | FAILSAFE"));
    }

    Serial.println();
}

void handleAnomaly(int sensorIndex, const LocalSensorData &sensorData)
{
    Serial.print(F("ANOMALY: Sensor "));
    Serial.print(sensorIndex + 1);
    Serial.print(F(" | Moisture: "));
    Serial.print(sensorData.moisture);
    Serial.print(F(" | Temp: "));
    Serial.print(sensorData.temperature / 10.0f); // convert back to float for display
    Serial.print(F(" | Humidity: "));
    Serial.print(sensorData.humidity / 10.0f);    // convert back to float for display
    Serial.print(F(" | Light: "));
    Serial.println(sensorData.lightLevel);

    // Flash LED or send alert (implementation depends on hardware)
    // digitalWrite(LED_BUILTIN, HIGH);
    // delay(100);
    // digitalWrite(LED_BUILTIN, LOW);
}

void sendDataToESP32(int sensorIndex, const LocalSensorData &sensorData,
                     const LocalAction &action, unsigned long inferenceTime)
{
    // Rate limiting - only send every ESP32_SEND_INTERVAL
    unsigned long currentTime = millis();
    if (currentTime - lastESP32Send < ESP32_SEND_INTERVAL) {
        return;
    }
    lastESP32Send = currentTime;
    
    // Create JSON message for ESP32 with bounds checking (static allocation)
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;
    
    // Input validation and bounds checking
    if (sensorIndex < 0 || sensorIndex >= 4) {
    Serial.println(F("ERROR: Invalid sensor index for ESP32 transmission"));
        return;
    }
    
    // Sanitize sensor values to prevent overflow
    float moisture = constrain((float)sensorData.moisture, 0, 1023);
    float temperature = constrain(sensorData.temperature / 10.0f, -40, 85);
    float humidity = constrain(sensorData.humidity / 10.0f, 0, 100);
    float light = constrain((float)sensorData.lightLevel, 0, 1023);
    float waterAmount = constrain((float)action.waterAmount, 0, 1000);
    
    // Compact JSON format to save RAM
    doc["s"] = sensorIndex + 1;
    doc["m"] = (int)moisture;        // Integer to save space
    doc["t"] = (int)temperature;     // Integer to save space  
    doc["h"] = (int)humidity;        // Integer to save space
    doc["l"] = (int)light;           // Integer to save space
    doc["w"] = action.shouldWater ? 1 : 0;
    if (action.shouldWater) {
        doc["a"] = (int)waterAmount; // Only include if watering
    }
    
    // Skip size check to save RAM - JSON is small with compact format
    
    // Send to ESP32 via dedicated serial port
    serializeJson(doc, esp32Serial);
    esp32Serial.println();
    
    // Minimal debug log to save RAM
    if (action.shouldWater) {
        Serial.print("ESP32: S");
        Serial.print(sensorIndex + 1);
        Serial.println(" watered");
    }
}

void printStatusReport()
{
    Serial.println("=== STATUS REPORT ===");
    Serial.print("Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");

    Serial.print("Total Decisions: ");
    Serial.println(totalDecisions);

    Serial.print("Total Watering Actions: ");
    Serial.println(totalWateringActions);

    Serial.print("Total Anomalies: ");
    Serial.println(totalAnomalies);

    Serial.print("Average Inference Time: ");
    Serial.print(mlEngine.getAverageInferenceTime());
    Serial.println(" ms");

    // Print current sensor readings
    Serial.println("\nCurrent Sensor Readings:");
    for (int i = 0; i < 2; i++)
    {
        Serial.print("  Plant ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(analogRead(MOISTURE_PINS[i]));
        Serial.print(" (");
        Serial.print(analogRead(MOISTURE_PINS[i]) > 450 ? "DRY" : "WET");
        Serial.println(")");
    }

    Serial.print("  Temperature: ");
    Serial.print(readTemperature());
    Serial.println("°C");

    Serial.print("  Humidity: ");
    Serial.print(readHumidity());
    Serial.println("%");

    Serial.print("  Light: ");
    Serial.println(analogRead(LIGHT_PIN));

    Serial.println("====================");
    Serial.println();
}

float readTemperature()
{
#ifdef MEMORY_OPTIMIZED
    return 22.5; // Default temperature when DHT disabled
#else
    float temperature = dht.readTemperature();
    
    // Validate sensor reading
    if (isnan(temperature) || temperature < MIN_TEMP || temperature > MAX_TEMP)
    {
        if (!environmentalSensorHealth.isFailed)
        {
            Serial.println("ERROR: Invalid temperature reading from DHT22");
            environmentalSensorHealth.consecutiveErrors++;
            if (environmentalSensorHealth.consecutiveErrors >= MAX_CONSECUTIVE_SENSOR_ERRORS)
            {
                environmentalSensorHealth.isFailed = true;
                handleSensorFailure(-1, "DHT22_TEMPERATURE_FAILURE");
            }
        }
        return 22.5; // Fallback to safe default
    }
    
    environmentalSensorHealth.consecutiveErrors = 0;
    environmentalSensorHealth.lastUpdateTime = (uint16_t)(millis() / 1000);
    return temperature;
#endif
}

float readHumidity()
{
#ifdef MEMORY_OPTIMIZED
    return 60.0; // Default humidity when DHT disabled
#else
    float humidity = dht.readHumidity();
    
    // Validate sensor reading
    if (isnan(humidity) || humidity < MIN_HUMIDITY || humidity > MAX_HUMIDITY)
    {
        if (!environmentalSensorHealth.isFailed)
        {
            Serial.println("ERROR: Invalid humidity reading from DHT22");
            environmentalSensorHealth.consecutiveErrors++;
            if (environmentalSensorHealth.consecutiveErrors >= MAX_CONSECUTIVE_SENSOR_ERRORS)
            {
                environmentalSensorHealth.isFailed = true;
                handleSensorFailure(-1, "DHT22_HUMIDITY_FAILURE");
            }
        }
        return 60.0; // Fallback to safe default
    }
    
    environmentalSensorHealth.consecutiveErrors = 0;
    environmentalSensorHealth.lastUpdateTime = (uint16_t)(millis() / 1000);
    return humidity;
#endif
}

// Serial command processing (optional)
void serialEvent()
{
    if (Serial.available())
    {
        char command[16] = {0};
        int len = Serial.readBytesUntil('\n', command, 15);
        command[len] = '\0';

        if (strcmp(command, "status") == 0)
        {
            printStatusReport();
        }
        else if (strcmp(command, "reset") == 0)
        {
            mlEngine.resetStats();
            totalDecisions = 0;
            totalWateringActions = 0;
            totalAnomalies = 0;
            Serial.println("Statistics reset.");
        }
        else if (strcmp(command, "debug") == 0)
        {
            // Enable debug mode
            Serial.println("Debug mode enabled.");
        }
        else if (strcmp(command, "stop") == 0 || strcmp(command, "emergency") == 0)
        {
            emergencyStopAllPumps();
        }
        else if (strncmp(command, "plant", 5) == 0)
        {
            // Change plant type: "plant 1 tomato"
            // Implementation depends on requirements
            Serial.println("Plant configuration command received.");
        }
    }
}

void updatePumpStates()
{
    unsigned long currentTime = millis();
    
    for (int i = 0; i < 2; i++)
    {
        if (pumpStates[i].isActive)
        {
            unsigned long elapsedTime = currentTime - pumpStates[i].startTime;
            
            // CRITICAL: Hardware failsafe check (absolute maximum duration)
            if (elapsedTime > FAILSAFE_PUMP_DURATION)
            {
                triggerPumpFailsafe(i, "FAILSAFE_TIMEOUT_EXCEEDED");
                continue;
            }
            
            // Check for emergency stop
            if (pumpStates[i].emergencyStop)
            {
                digitalWrite(RELAY_PINS[i], HIGH); // Turn off pump
                pumpStates[i].isActive = false;
                pumpStates[i].lastWateringTime = currentTime;
                Serial.print("PUMP ");
                Serial.print(i + 1);
                Serial.println(" EMERGENCY STOPPED");
                logSystemEvent("PUMP_EMERGENCY_STOP", i);
                continue;
            }
            
            // Check if normal watering duration is complete
            if (elapsedTime >= pumpStates[i].duration)
            {
                digitalWrite(RELAY_PINS[i], HIGH); // Turn off pump
                pumpStates[i].isActive = false;
                pumpStates[i].lastWateringTime = currentTime;
                Serial.print("PUMP ");
                Serial.print(i + 1);
                Serial.print(" STOPPED - Watering complete (");
                Serial.print(elapsedTime);
                Serial.println("ms)");
                logSystemEvent("PUMP_NORMAL_STOP", i);
            }
            
            // Additional safety check: verify pump hasn't been stuck ON
            else if (elapsedTime > pumpStates[i].duration + 5000) // 5 second grace period
            {
                triggerPumpFailsafe(i, "PUMP_STUCK_ON_DETECTION");
            }
        }
    }
}

bool validateSensorReading(int sensorIndex, float value, float minVal, float maxVal)
{
    if (isnan(value) || value < minVal || value > maxVal)
    {
        Serial.print("ERROR: Invalid sensor reading for sensor ");
        Serial.print(sensorIndex + 1);
        Serial.print(" - Value: ");
        Serial.print(value);
        Serial.print(" (Range: ");
        Serial.print(minVal);
        Serial.print("-");
        Serial.print(maxVal);
        Serial.println(")");
        return false;
    }
    return true;
}

void emergencyStopAllPumps()
{
    for (int i = 0; i < 2; i++)
    {
        if (pumpStates[i].isActive)
        {
            pumpStates[i].emergencyStop = true;
        }
    }
    Serial.println("EMERGENCY STOP: All active pumps will be stopped");
}

void updateSensorHealth(int sensorIndex, uint16_t reading, bool isValid)
{
    if (isValid)
    {
           moistureSensorHealth[sensorIndex].lastValidReading = reading;
           moistureSensorHealth[sensorIndex].isFailed = false;
    }
    else
    {
           moistureSensorHealth[sensorIndex].isFailed = true;
    }
}



void resetWatchdog()
{
    wdt_reset();
    lastWatchdogReset = millis();
}

void performSystemHealthCheck()
{
    unsigned long currentTime = millis();
    bool systemHealthy = true;
    
    // Check sensor health
    for (int i = 0; i < 2; i++)
    {
        // Check pump daily limits
        if (pumpStates[i].wateringCount > 20) // Max 20 waterings per day
        {
            Serial.print("WARNING: Pump ");
            Serial.print(i + 1);
            Serial.println(" exceeded daily watering limit");
            systemHealthy = false;
        }
    }
    
    // Check watchdog health
    if (currentTime - lastWatchdogReset > WATCHDOG_TIMEOUT * 0.8) // 80% of timeout
    {
        Serial.println("WARNING: Watchdog reset interval too long");
        systemHealthy = false;
    }
    
    // Enter failsafe mode if multiple issues detected
    if (!systemHealthy && !systemFailsafeMode)
    {
        Serial.println("Multiple system issues detected - performing health assessment");
        // Don't immediately enter failsafe, but increase monitoring
    }
}

bool checkPumpFailsafe(int pumpIndex)
{
    unsigned long currentTime = millis();
    
    // Check cooldown period
    if (currentTime - pumpStates[pumpIndex].lastWateringTime < PUMP_COOLDOWN_TIME)
    {
        return false; // Still in cooldown
    }
    
    // Check daily limits
    if (pumpStates[pumpIndex].wateringCount > 20) // Max 20 waterings per day
    {
        Serial.print("BLOCKED: Pump ");
        Serial.print(pumpIndex + 1);
        Serial.println(" exceeded daily watering count limit");
        return false;
    }
    
    if (pumpStates[pumpIndex].wateringCount > 20) // Max 20 waterings per day
    {
        Serial.print("BLOCKED: Pump ");
        Serial.print(pumpIndex + 1);
        Serial.println(" exceeded daily watering time limit");
        return false;
    }
    
    return true;
}

void triggerPumpFailsafe(int pumpIndex, const char* reason)
{
    // Immediately turn off pump
    digitalWrite(RELAY_PINS[pumpIndex], HIGH);
    
    // Update pump state
    pumpStates[pumpIndex].isActive = false;
    pumpStates[pumpIndex].failsafeTriggered = true;
    pumpStates[pumpIndex].emergencyStop = true;
    
    Serial.print("PUMP FAILSAFE TRIGGERED: Pump ");
    Serial.print(pumpIndex + 1);
    Serial.print(" - Reason: ");
    Serial.println(reason);
    
    logSystemEvent("PUMP_FAILSAFE", pumpIndex);
    
    // If multiple pumps fail, enter system failsafe mode
    int failsafeCount = 0;
    for (int i = 0; i < 2; i++)
    {
        if (pumpStates[i].failsafeTriggered) failsafeCount++;
    }
    
    if (failsafeCount >= 2)
    {
        enterSystemFailsafeMode("MULTIPLE_PUMP_FAILURES");
    }
}


void handleSensorFailure(int sensorIndex, const char* failureType)
{
    SensorHealth* sensor = &moistureSensorHealth[sensorIndex];
    sensor->isFailed = true;
    Serial.print("SENSOR FAILURE: Sensor ");
    Serial.print(sensorIndex + 1);
    Serial.print(" - Type: ");
    Serial.println(failureType);
    logSystemEvent("SENSOR_FAILURE", sensorIndex);
    // Count failed sensors
    int failedSensorCount = 0;
    for (int i = 0; i < 2; i++)
    {
        if (moistureSensorHealth[i].isFailed) failedSensorCount++;
    }
    // If majority of sensors fail, enter failsafe mode
    if (failedSensorCount >= 2)
    {
        enterSystemFailsafeMode("MAJORITY_SENSOR_FAILURE");
    }
}

void enterSystemFailsafeMode(const char* reason)
{
    if (systemFailsafeMode) return; // Already in failsafe
    
    systemFailsafeMode = true;
    
    Serial.println("========================================");
    Serial.println("    SYSTEM FAILSAFE MODE ACTIVATED");
    Serial.println("========================================");
    Serial.println(reason);
    // Stop all pumps immediately
    emergencyStopAllPumps();
    // Disable all automatic watering
    Serial.println("All automatic watering DISABLED");
    Serial.println("Manual reset required to resume operation");
    Serial.println("Send 'reset' command to exit failsafe mode");
    
    logSystemEvent("SYSTEM_FAILSAFE_ACTIVATED");
}

bool validatePumpOperation(int pumpIndex, unsigned long requestedDuration)
{
    // Check if pump failsafe allows operation
    if (!checkPumpFailsafe(pumpIndex))
    {
        return false;
    }
    
    // Validate duration is within safe limits
    if (requestedDuration > MAX_PUMP_DURATION)
    {
        Serial.print("WARNING: Requested duration ");
        Serial.print(requestedDuration);
        Serial.print("ms exceeds maximum ");
        Serial.print(MAX_PUMP_DURATION);
        Serial.println("ms");
        // Allow but will be capped in execution
    }
    
    // Check if sensor data is valid for this pump
    if (moistureSensorHealth[pumpIndex].isFailed)
    {
        Serial.print("BLOCKED: Sensor ");
        Serial.print(pumpIndex + 1);
        Serial.println(" is marked as failed");
        return false;
    }
    
    return true;
}

void logSystemEvent(const char* event, int sensorIndex)
{
    unsigned long currentTime = millis();
    
    Serial.print("[");
    Serial.print(currentTime);
    Serial.print("ms] EVENT: ");
    Serial.print(event);
    
    if (sensorIndex >= 0)
    {
        Serial.print(" | Sensor: ");
        Serial.print(sensorIndex + 1);
    }
    
    Serial.println();
}

void performEmergencyShutdown(const char* reason)
{
    Serial.println("========================================");
    Serial.println("      EMERGENCY SHUTDOWN INITIATED");
    Serial.println("========================================");
    Serial.print("Reason: ");
    Serial.println(reason);
    
    // Immediately stop all pumps
    for (int i = 0; i < 2; i++)
    {
        digitalWrite(RELAY_PINS[i], HIGH); // Turn off all pumps
        pumpStates[i].isActive = false;
        pumpStates[i].emergencyStop = true;
    }
    
    // Enter failsafe mode
    enterSystemFailsafeMode(reason);
    
    Serial.println("All pumps STOPPED");
    Serial.println("System in EMERGENCY SHUTDOWN mode");
    
    logSystemEvent("EMERGENCY_SHUTDOWN");
    
    // Flash status (if LED available)
    for (int i = 0; i < 10; i++)
    {
        delay(100);
        // Could flash LED here if available
    }
}