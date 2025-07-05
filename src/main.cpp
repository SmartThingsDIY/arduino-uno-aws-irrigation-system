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

#include "LocalMLEngine.h"
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <DHT.h>

// Pin definitions
const int MOISTURE_PINS[4] = {A0, A1, A2, A3};
const int RELAY_PINS[4] = {2, 3, 4, 5};
const int TEMP_HUMIDITY_PIN = 6; // DHT22 sensor
const int LIGHT_PIN = A4;        // LDR sensor
const int ESP32_RX_PIN = 7;      // Arduino TX -> ESP32 RX
const int ESP32_TX_PIN = 8;      // Arduino RX <- ESP32 TX

// DHT22 sensor configuration
#define DHT_TYPE DHT22
DHT dht(TEMP_HUMIDITY_PIN, DHT_TYPE);

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

// JSON buffer size with safety margin
const size_t JSON_BUFFER_SIZE = 300;

// Global variables
LocalMLEngine mlEngine;
SoftwareSerial esp32Serial(ESP32_TX_PIN, ESP32_RX_PIN); // RX, TX
DHT dht(TEMP_HUMIDITY_PIN, DHT22);
unsigned long lastSensorRead = 0;
unsigned long lastSerialReport = 0;
unsigned long lastESP32Send = 0;

// Non-blocking pump control
struct PumpState {
    bool isActive;
    unsigned long startTime;
    unsigned long duration;
    bool emergencyStop;
};
PumpState pumpStates[4] = {{false, 0, 0, false}};

// Sensor health monitoring (optimized for memory)
struct SensorHealth {
    uint16_t lastValidReading;    // Use uint16_t for moisture (0-1023)
    uint8_t consecutiveErrors;    // 8-bit is enough
    bool isDisconnected;
};
SensorHealth moistureSensorHealth[4] = {{0, 0, false}};
bool dhtSensorOK = true;

// Sensor health constants
const uint8_t MAX_CONSECUTIVE_ERRORS = 3;

// Statistics
unsigned long totalDecisions = 0;
unsigned long totalWateringActions = 0;
unsigned long totalAnomalies = 0;

// Function declarations
void processAllSensors();
void processSensor(int sensorIndex, float temperature, float humidity, float lightLevel);
void executeWateringAction(int sensorIndex, const Action &action);
void logWateringAction(int sensorIndex, const Action &action, unsigned long inferenceTime);
void handleAnomaly(int sensorIndex, const SensorData &sensorData);
void sendDataToESP32(int sensorIndex, const SensorData &sensorData, const Action &action, unsigned long inferenceTime);
void printStatusReport();
float readTemperature();
float readHumidity();
void updatePumpStates();
bool validateSensorReading(int sensorIndex, float value, float minVal, float maxVal);
void emergencyStopAllPumps();
void updateSensorHealth(int sensorIndex, uint16_t reading, bool isValid);
bool isSensorDisconnected(int sensorIndex);

void setup()
{
    Serial.begin(9600);
    esp32Serial.begin(9600);
    dht.begin();

    // Initialize pins
    for (int i = 0; i < 4; i++)
    {
        pinMode(MOISTURE_PINS[i], INPUT);
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], HIGH); // Turn off pumps initially (relays are active LOW)
    }

    pinMode(LIGHT_PIN, INPUT);

    // Initialize DHT22 sensor
    dht.begin();
    Serial.println("DHT22 sensor initialized");

    // Initialize ML engine
    if (!mlEngine.begin())
    {
        Serial.println("ERROR: Failed to initialize ML engine!");
        while (1)
            delay(1000); // Halt execution
    }

    // Configure plant types for each sensor
    mlEngine.setPlantType(0, TOMATO);
    mlEngine.setPlantType(1, LETTUCE);
    mlEngine.setPlantType(2, BASIL);
    mlEngine.setPlantType(3, MINT);

    // Set growth stages
    mlEngine.setGrowthStage(0, VEGETATIVE);
    mlEngine.setGrowthStage(1, VEGETATIVE);
    mlEngine.setGrowthStage(2, FLOWERING);
    mlEngine.setGrowthStage(3, MATURE);

    Serial.println("Smart Irrigation System Started!");
    Serial.println("Plants: Tomato, Lettuce, Basil, Mint");

    delay(1000); // Initial delay
}

void loop()
{
    unsigned long currentTime = millis();

    // Read sensors and make decisions
    if (currentTime - lastSensorRead >= SENSOR_READ_INTERVAL)
    {
        processAllSensors();
        lastSensorRead = currentTime;
    }

    // Update pump states (non-blocking pump control)
    updatePumpStates();

    // Periodic status report
    if (currentTime - lastSerialReport >= SERIAL_REPORT_INTERVAL)
    {
        printStatusReport();
        lastSerialReport = currentTime;
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
    for (int i = 0; i < 4; i++)
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

    // Update sensor health tracking
    updateSensorHealth(sensorIndex, (uint16_t)moisture, validMoisture);

    // Check if moisture sensor is disconnected
    if (isSensorDisconnected(sensorIndex))
    {
        Serial.print("CRITICAL: Moisture sensor ");
        Serial.print(sensorIndex + 1);
        Serial.println(" appears to be disconnected - skipping processing");
        return;
    }

    // If critical sensors are invalid, skip processing
    if (!validMoisture)
    {
        Serial.print("CRITICAL: Skipping sensor ");
        Serial.print(sensorIndex + 1);
        Serial.println(" processing due to invalid moisture reading");
        return;
    }

    // Use fallback values for non-critical sensors if needed
    if (!validTemperature)
    {
        temperature = 22.5; // Fallback temperature
        Serial.println("Using fallback temperature value");
    }
    if (!validHumidity)
    {
        humidity = 60.0; // Fallback humidity
        Serial.println("Using fallback humidity value");
    }
    if (!validLight)
    {
        lightLevel = 500; // Fallback light level
        Serial.println("Using fallback light level value");
    }

    // Create sensor data structure
    SensorData sensorData;
    sensorData.moisture = moisture;
    sensorData.temperature = temperature;
    sensorData.humidity = humidity;
    sensorData.lightLevel = lightLevel;

    // Get AI decision
    unsigned long startTime = micros();
    Action action = mlEngine.getImmediateAction(sensorIndex, sensorData);
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

void executeWateringAction(int sensorIndex, const Action &action)
{
    // Check if pump is already active
    if (pumpStates[sensorIndex].isActive)
    {
        Serial.print("WARNING: Pump ");
        Serial.print(sensorIndex + 1);
        Serial.println(" already active, skipping watering action");
        return;
    }

    // Start non-blocking watering
    pumpStates[sensorIndex].isActive = true;
    pumpStates[sensorIndex].startTime = millis();
    pumpStates[sensorIndex].duration = action.waterDuration;
    pumpStates[sensorIndex].emergencyStop = false;

    // Turn on pump
    digitalWrite(RELAY_PINS[sensorIndex], LOW); // Active LOW relay

    Serial.print("PUMP ");
    Serial.print(sensorIndex + 1);
    Serial.print(" STARTED - Duration: ");
    Serial.print(action.waterDuration);
    Serial.println("ms");
}

void logWateringAction(int sensorIndex, const Action &action, unsigned long inferenceTime)
{
    Serial.print("WATERING: Plant ");
    Serial.print(sensorIndex + 1);
    Serial.print(" | Amount: ");
    Serial.print(action.waterAmount);
    Serial.print("ml | Duration: ");
    Serial.print(action.waterDuration);
    Serial.print("ms | Inference: ");
    Serial.print(inferenceTime);
    Serial.print("μs");

    if (action.isFailsafe)
    {
        Serial.print(" | FAILSAFE");
    }

    Serial.println();
}

void handleAnomaly(int sensorIndex, const SensorData &sensorData)
{
    Serial.print("ANOMALY: Sensor ");
    Serial.print(sensorIndex + 1);
    Serial.print(" | Moisture: ");
    Serial.print(sensorData.moisture);
    Serial.print(" | Temp: ");
    Serial.print(sensorData.temperature);
    Serial.print(" | Humidity: ");
    Serial.print(sensorData.humidity);
    Serial.print(" | Light: ");
    Serial.println(sensorData.lightLevel);

    // Flash LED or send alert (implementation depends on hardware)
    // digitalWrite(LED_BUILTIN, HIGH);
    // delay(100);
    // digitalWrite(LED_BUILTIN, LOW);
}

void sendDataToESP32(int sensorIndex, const SensorData &sensorData,
                     const Action &action, unsigned long inferenceTime)
{
    // Create JSON message for ESP32
    StaticJsonDocument<200> doc;

    doc["sensor"] = sensorIndex + 1;
    doc["moisture"] = sensorData.moisture;
    doc["temperature"] = sensorData.temperature;
    doc["humidity"] = sensorData.humidity;
    doc["light"] = sensorData.lightLevel;
    doc["watered"] = action.shouldWater;
    doc["waterAmount"] = action.waterAmount;
    doc["inferenceTime"] = inferenceTime;
    doc["timestamp"] = millis();

    // Send to ESP32 via Serial (modify based on your communication setup)
    serializeJson(doc, Serial);
    Serial.println();
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
    for (int i = 0; i < 4; i++)
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
    float temperature = dht.readTemperature();
    
    // Validate sensor reading
    if (isnan(temperature) || temperature < MIN_TEMP || temperature > MAX_TEMP)
    {
        if (dhtSensorOK)
        {
            Serial.println("ERROR: Invalid temperature reading from DHT22");
            dhtSensorOK = false;
        }
        return 22.5; // Fallback to safe default
    }
    
    dhtSensorOK = true;
    return temperature;
}

float readHumidity()
{
    float humidity = dht.readHumidity();
    
    // Validate sensor reading
    if (isnan(humidity) || humidity < MIN_HUMIDITY || humidity > MAX_HUMIDITY)
    {
        if (dhtSensorOK)
        {
            Serial.println("ERROR: Invalid humidity reading from DHT22");
            dhtSensorOK = false;
        }
        return 60.0; // Fallback to safe default
    }
    
    dhtSensorOK = true;
    return humidity;
}

// Serial command processing (optional)
void serialEvent()
{
    if (Serial.available())
    {
        String command = Serial.readString();
        command.trim();

        if (command == "status")
        {
            printStatusReport();
        }
        else if (command == "reset")
        {
            mlEngine.resetStats();
            totalDecisions = 0;
            totalWateringActions = 0;
            totalAnomalies = 0;
            Serial.println("Statistics reset.");
        }
        else if (command == "debug")
        {
            // Enable debug mode
            Serial.println("Debug mode enabled.");
        }
        else if (command == "stop" || command == "emergency")
        {
            emergencyStopAllPumps();
        }
        else if (command.startsWith("plant"))
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
    
    for (int i = 0; i < 4; i++)
    {
        if (pumpStates[i].isActive)
        {
            // Check for emergency stop
            if (pumpStates[i].emergencyStop)
            {
                digitalWrite(RELAY_PINS[i], HIGH); // Turn off pump
                pumpStates[i].isActive = false;
                Serial.print("PUMP ");
                Serial.print(i + 1);
                Serial.println(" EMERGENCY STOPPED");
                continue;
            }
            
            // Check if watering duration is complete
            if (currentTime - pumpStates[i].startTime >= pumpStates[i].duration)
            {
                digitalWrite(RELAY_PINS[i], HIGH); // Turn off pump
                pumpStates[i].isActive = false;
                Serial.print("PUMP ");
                Serial.print(i + 1);
                Serial.println(" STOPPED - Watering complete");
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
    for (int i = 0; i < 4; i++)
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
        // Valid reading - reset error count
        moistureSensorHealth[sensorIndex].lastValidReading = reading;
        moistureSensorHealth[sensorIndex].consecutiveErrors = 0;
        moistureSensorHealth[sensorIndex].isDisconnected = false;
    }
    else
    {
        // Invalid reading - increment error count
        moistureSensorHealth[sensorIndex].consecutiveErrors++;
        
        // Check if sensor should be marked as disconnected
        if (moistureSensorHealth[sensorIndex].consecutiveErrors >= MAX_CONSECUTIVE_ERRORS)
        {
            if (!moistureSensorHealth[sensorIndex].isDisconnected)
            {
                Serial.print("WARNING: Moisture sensor ");
                Serial.print(sensorIndex + 1);
                Serial.println(" marked as disconnected");
                moistureSensorHealth[sensorIndex].isDisconnected = true;
            }
        }
    }
}

bool isSensorDisconnected(int sensorIndex)
{
    return moistureSensorHealth[sensorIndex].isDisconnected;
}