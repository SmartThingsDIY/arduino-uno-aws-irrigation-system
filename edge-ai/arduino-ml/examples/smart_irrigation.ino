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
 * GitHub: https://github.com/isbkch
 * Version: 0.78.3
 * Date: 2025
 */

#include "LocalMLEngine.h"
#include <ArduinoJson.h>

// Pin definitions
const int MOISTURE_PINS[4] = {A0, A1, A2, A3};
const int RELAY_PINS[4] = {2, 3, 4, 5};
const int TEMP_HUMIDITY_PIN = 6; // DHT22 sensor
const int LIGHT_PIN = A4;        // LDR sensor

// Timing constants
const unsigned long SENSOR_READ_INTERVAL = 2000;    // 2 seconds
const unsigned long SERIAL_REPORT_INTERVAL = 10000; // 10 seconds

// Global variables
LocalMLEngine mlEngine;
unsigned long lastSensorRead = 0;
unsigned long lastSerialReport = 0;

// Statistics
unsigned long totalDecisions = 0;
unsigned long totalWateringActions = 0;
unsigned long totalAnomalies = 0;

void setup()
{
    Serial.begin(9600);

    // Initialize pins
    for (int i = 0; i < 4; i++)
    {
        pinMode(MOISTURE_PINS[i], INPUT);
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], HIGH); // Turn off pumps initially (relays are active LOW)
    }

    pinMode(LIGHT_PIN, INPUT);

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

    Serial.println("Smart Irrigation System with Edge AI Started!");
    Serial.println("Plant Configuration:");
    Serial.println("  Sensor 0: Tomato (Vegetative)");
    Serial.println("  Sensor 1: Lettuce (Vegetative)");
    Serial.println("  Sensor 2: Basil (Flowering)");
    Serial.println("  Sensor 3: Mint (Mature)");
    Serial.println();

    // Print system information
    Serial.print("ML Engine Memory Usage: ");
    Serial.print(sizeof(LocalMLEngine));
    Serial.println(" bytes");

    // Enable debug mode (uncomment to enable)
    // #define DEBUG_ML 1

    delay(2000); // Initial delay
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
    // Turn on pump
    digitalWrite(RELAY_PINS[sensorIndex], LOW); // Active LOW relay

    // Water for specified duration
    delay(action.waterDuration);

    // Turn off pump
    digitalWrite(RELAY_PINS[sensorIndex], HIGH);
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
    // Placeholder for temperature sensor reading
    // Replace with actual DHT22 or other sensor reading
    return 22.5; // Default temperature
}

float readHumidity()
{
    // Placeholder for humidity sensor reading
    // Replace with actual DHT22 or other sensor reading
    return 60.0; // Default humidity
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
        else if (command.startsWith("plant"))
        {
            // Change plant type: "plant 1 tomato"
            // Implementation depends on requirements
            Serial.println("Plant configuration command received.");
        }
    }
}