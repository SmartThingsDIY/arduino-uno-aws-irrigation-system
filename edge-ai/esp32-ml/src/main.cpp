/*
 * ESP32 Edge AI Smart Irrigation System
 * 
 * This firmware demonstrates Phase 2 and 3 of the Edge AI implementation:
 * - TensorFlow Lite Micro for moisture prediction and anomaly detection
 * - WiFi connectivity with web interface
 * - MQTT cloud communication
 * - OTA firmware updates
 * - Communication with Arduino via Serial
 * 
 * Hardware Requirements:
 * - ESP32 DevKit (240MHz dual-core, 520KB RAM)
 * - Connection to Arduino Uno via Serial
 * - Optional: BME280 sensor for environmental data
 * - Optional: MicroSD card for data logging
 * 
 * Author: Smart Irrigation AI Team
 * Version: 1.0
 * Date: 2024
 */

#include "EdgeInference.h"
#include "WiFiManager.h"
#include <ArduinoJson.h>
#include <time.h>
#include <SPIFFS.h>

// Model data (these would be loaded from SPIFFS in production)
// For now, we'll use placeholder arrays
extern const unsigned char moisture_lstm_model[] PROGMEM;
extern const unsigned char anomaly_autoencoder_model[] PROGMEM;
extern const size_t moisture_lstm_model_len;
extern const size_t anomaly_autoencoder_model_len;

// Core components
EdgeInference edgeAI;
WiFiManager wifiManager;

// Communication with Arduino
HardwareSerial arduinoSerial(1); // Use Serial1 for Arduino communication
const int ARDUINO_RX_PIN = 16;
const int ARDUINO_TX_PIN = 17;

// Timing constants
const unsigned long PREDICTION_INTERVAL = 60000;  // 1 minute
const unsigned long CLOUD_SYNC_INTERVAL = 300000; // 5 minutes
const unsigned long STATUS_REPORT_INTERVAL = 30000; // 30 seconds

// Global variables
unsigned long lastPrediction = 0;
unsigned long lastCloudSync = 0;
unsigned long lastStatusReport = 0;
unsigned long systemStartTime = 0;

// Statistics
struct SystemStats {
    unsigned long totalPredictions;
    unsigned long totalAnomalies;
    unsigned long successfulCloudSyncs;
    unsigned long failedCloudSyncs;
    float averagePredictionTime;
    float lastBatteryVoltage;
    
    SystemStats() : totalPredictions(0), totalAnomalies(0), 
                    successfulCloudSyncs(0), failedCloudSyncs(0),
                    averagePredictionTime(0), lastBatteryVoltage(3.3) {}
} stats;

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32 Edge AI Smart Irrigation System Starting...");
    
    systemStartTime = millis();
    
    // Initialize SPIFFS for file storage
    if (!SPIFFS.begin(true)) {
        Serial.println("ERROR: Failed to initialize SPIFFS");
        while (1) delay(1000);
    }
    
    // Initialize Arduino communication
    arduinoSerial.begin(9600, SERIAL_8N1, ARDUINO_RX_PIN, ARDUINO_TX_PIN);
    Serial.println("Arduino Serial communication initialized");
    
    // Initialize WiFi Manager
    if (!wifiManager.begin()) {
        Serial.println("ERROR: Failed to initialize WiFi Manager");
        while (1) delay(1000);
    }
    
    // Set WiFi callbacks
    wifiManager.setConnectionCallback(onWiFiStatusChange);
    wifiManager.setMessageCallback(onMQTTMessage);
    
    // Initialize Edge AI
    if (!edgeAI.begin()) {
        Serial.println("ERROR: Failed to initialize Edge AI");
        while (1) delay(1000);
    }
    
    // Load ML models
    loadMLModels();
    
    // Configure time
    configureTime();
    
    // Enable OTA updates
    wifiManager.enableOTA(true);
    
    Serial.println("ESP32 Edge AI System Ready!");
    printSystemInfo();
}

void loop() {
    unsigned long currentTime = millis();
    
    // Handle WiFi and web server
    wifiManager.loop();
    
    // Handle OTA updates
    wifiManager.handleOTA();
    
    // Process Arduino data
    processArduinoData();
    
    // Make AI predictions
    if (currentTime - lastPrediction >= PREDICTION_INTERVAL) {
        makePredictions();
        lastPrediction = currentTime;
    }
    
    // Sync with cloud
    if (currentTime - lastCloudSync >= CLOUD_SYNC_INTERVAL) {
        syncWithCloud();
        lastCloudSync = currentTime;
    }
    
    // Status report
    if (currentTime - lastStatusReport >= STATUS_REPORT_INTERVAL) {
        sendStatusReport();
        lastStatusReport = currentTime;
    }
    
    // Small delay to prevent watchdog reset
    delay(10);
}

void loadMLModels() {
    Serial.println("Loading ML models...");
    
    // Check if models exist in SPIFFS
    if (SPIFFS.exists("/models/moisture_lstm.tflite")) {
        Serial.println("Loading moisture LSTM model from SPIFFS");
        if (!loadModelFromFile(MODEL_MOISTURE_LSTM, "/models/moisture_lstm.tflite")) {
            Serial.println("Warning: Failed to load LSTM model from file, using default");
        }
    } else {
        Serial.println("LSTM model file not found, using embedded model");
        // In production, you would load from embedded arrays
        // edgeAI.loadModel(MODEL_MOISTURE_LSTM, moisture_lstm_model, moisture_lstm_model_len);
    }
    
    if (SPIFFS.exists("/models/anomaly_autoencoder.tflite")) {
        Serial.println("Loading anomaly detection model from SPIFFS");
        if (!loadModelFromFile(MODEL_ANOMALY_AUTOENCODER, "/models/anomaly_autoencoder.tflite")) {
            Serial.println("Warning: Failed to load anomaly model from file");
        }
    } else {
        Serial.println("Anomaly model file not found, using embedded model");
        // edgeAI.loadModel(MODEL_ANOMALY_AUTOENCODER, anomaly_autoencoder_model, anomaly_autoencoder_model_len);
    }
    
    edgeAI.printModelInfo();
}

bool loadModelFromFile(ModelType type, const char* filename) {
    File file = SPIFFS.open(filename, "r");
    if (!file) {
        Serial.print("Failed to open model file: ");
        Serial.println(filename);
        return false;
    }
    
    size_t fileSize = file.size();
    if (fileSize > MAX_MODEL_SIZE) {
        Serial.print("Model file too large: ");
        Serial.print(fileSize);
        Serial.print(" > ");
        Serial.println(MAX_MODEL_SIZE);
        file.close();
        return false;
    }
    
    // Allocate buffer for model data
    unsigned char* modelData = (unsigned char*)malloc(fileSize);
    if (modelData == nullptr) {
        Serial.println("Failed to allocate memory for model");
        file.close();
        return false;
    }
    
    // Read model data
    size_t bytesRead = file.readBytes((char*)modelData, fileSize);
    file.close();
    
    if (bytesRead != fileSize) {
        Serial.println("Failed to read complete model file");
        free(modelData);
        return false;
    }
    
    // Load model
    bool success = edgeAI.loadModel(type, modelData, fileSize);
    
    // Note: In production, you'd want to keep the model data in memory
    // For this example, we'll free it after loading (TFLite makes a copy)
    free(modelData);
    
    return success;
}

void processArduinoData() {
    if (arduinoSerial.available()) {
        String jsonString = arduinoSerial.readStringUntil('\n');
        jsonString.trim(); // Remove whitespace
        
        // Input validation
        if (jsonString.length() == 0) {
            return; // Empty string
        }
        
        if (jsonString.length() > 500) { // Prevent memory overflow
            Serial.print("WARNING: Arduino JSON too large: ");
            Serial.print(jsonString.length());
            Serial.println(" bytes, discarding");
            return;
        }
        
        // Rate limiting to prevent data flooding
        static unsigned long lastProcessTime = 0;
        unsigned long currentTime = millis();
        if (currentTime - lastProcessTime < 1000) { // Max 1 msg/second
            return;
        }
        lastProcessTime = currentTime;
        
        parseArduinoData(jsonString);
    }
}

void parseArduinoData(const String& jsonString) {
    DynamicJsonDocument doc(400); // Increased size for new fields
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        Serial.print("Input: ");
        Serial.println(jsonString.substring(0, 100)); // First 100 chars for debug
        return;
    }
    
    // Validate required fields
    if (!doc.containsKey("sensor") || !doc.containsKey("moisture")) {
        Serial.println("ERROR: Missing required JSON fields");
        return;
    }
    
    // Extract and validate sensor data with bounds checking
    SensorData data;
    data.moisture = constrain((float)(doc["moisture"] | 0.0), 0.0, 1023.0);
    data.temperature = constrain((float)(doc["temperature"] | 25.0), -40.0, 85.0);
    data.humidity = constrain((float)(doc["humidity"] | 50.0), 0.0, 100.0);
    data.lightLevel = constrain((float)(doc["light"] | 500.0), 0.0, 1023.0);
    data.timestamp = millis();
    
    // Additional fields with validation
    int sensorIndex = constrain((int)(doc["sensor"] | 1), 1, 4) - 1; // Convert to 0-based
    bool watered = doc["watered"] | false;
    float waterAmount = constrain((float)(doc["waterAmount"] | 0.0), 0.0, 1000.0);
    unsigned long inferenceTime = constrain((unsigned long)(doc["inferenceTime"] | 0), 0UL, 999999UL);
    
    // Store Arduino metadata
    data.sensorIndex = sensorIndex;
    data.watered = watered;
    data.waterAmount = waterAmount;
    data.arduinoInferenceTime = inferenceTime;
    
    // Add temporal features
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        data.hour = timeinfo.tm_hour;
        data.dayOfWeek = timeinfo.tm_wday;
        data.month = timeinfo.tm_mon + 1;
    }
    
    // Add to Edge AI buffer
    edgeAI.addSensorData(data);
    
    // Log received data
    if (wifiManager.getStatus() == WIFI_CONNECTED) {
        Serial.print("Received from Arduino - Sensor ");
        Serial.print(doc["sensor"].as<int>());
        Serial.print(": M=");
        Serial.print(data.moisture);
        Serial.print(", T=");
        Serial.print(data.temperature);
        Serial.print(", H=");
        Serial.print(data.humidity);
        Serial.print(", L=");
        Serial.println(data.lightLevel);
    }
}

void makePredictions() {
    if (!edgeAI.hasEnoughData()) {
        Serial.println("Insufficient data for predictions");
        return;
    }
    
    unsigned long startTime = millis();
    
    // Make 24-hour moisture prediction
    PredictionResult forecast = edgeAI.predict24Hours();
    
    // Detect anomalies using latest data
    SensorData latestData; // This would be the most recent data
    float anomalyScore = edgeAI.detectAnomaly(latestData);
    
    unsigned long predictionTime = millis() - startTime;
    
    // Update statistics
    stats.totalPredictions++;
    stats.averagePredictionTime = (stats.averagePredictionTime * (stats.totalPredictions - 1) + predictionTime) / stats.totalPredictions;
    
    if (anomalyScore > 0.8) {
        stats.totalAnomalies++;
        Serial.print("ANOMALY DETECTED! Score: ");
        Serial.println(anomalyScore);
    }
    
    // Send predictions to Arduino
    sendPredictionsToArduino(forecast, anomalyScore);
    
    // Send to cloud if connected
    if (wifiManager.isCloudConnected()) {
        sendPredictionsToCloud(forecast, anomalyScore);
    }
    
    Serial.print("Prediction completed in ");
    Serial.print(predictionTime);
    Serial.print("ms, confidence: ");
    Serial.println(forecast.confidence);
}

void sendPredictionsToArduino(const PredictionResult& forecast, float anomalyScore) {
    StaticJsonDocument<400> doc;
    
    doc["type"] = "prediction";
    doc["timestamp"] = millis();
    doc["confidence"] = forecast.confidence;
    doc["anomaly_score"] = anomalyScore;
    
    // Send next 6 hours of forecast (simplified)
    JsonArray forecastArray = doc.createNestedArray("forecast_6h");
    for (int i = 0; i < 6; i++) {
        forecastArray.add(forecast.moistureForecast[i]);
    }
    
    // Send irrigation recommendations
    JsonObject recommendations = doc.createNestedObject("recommendations");
    recommendations["water_soon"] = (forecast.moistureForecast[1] > 600); // Next hour
    recommendations["water_level"] = anomalyScore > 0.5 ? "high" : "normal";
    
    String jsonString;
    serializeJson(doc, jsonString);
    arduinoSerial.println(jsonString);
}

void sendPredictionsToCloud(const PredictionResult& forecast, float anomalyScore) {
    StaticJsonDocument<600> doc;
    
    doc["device_id"] = wifiManager.getConfig().deviceId;
    doc["timestamp"] = time(nullptr);
    doc["prediction"] = true;
    doc["confidence"] = forecast.confidence;
    doc["anomaly_score"] = anomalyScore;
    
    // Full 24-hour forecast
    JsonArray forecastArray = doc.createNestedArray("moisture_forecast_24h");
    for (int i = 0; i < 24; i++) {
        forecastArray.add(forecast.moistureForecast[i]);
    }
    
    // System metrics
    JsonObject metrics = doc.createNestedObject("system_metrics");
    metrics["total_predictions"] = stats.totalPredictions;
    metrics["total_anomalies"] = stats.totalAnomalies;
    metrics["avg_prediction_time_ms"] = stats.averagePredictionTime;
    metrics["free_heap"] = ESP.getFreeHeap();
    metrics["uptime_ms"] = millis() - systemStartTime;
    
    String payload;
    serializeJson(doc, payload);
    
    String topic = "irrigation/" + String(wifiManager.getConfig().deviceId) + "/predictions";
    
    if (wifiManager.publishSensorData(payload)) {
        stats.successfulCloudSyncs++;
    } else {
        stats.failedCloudSyncs++;
    }
}

void syncWithCloud() {
    if (!wifiManager.isCloudConnected()) {
        return;
    }
    
    Serial.println("Syncing with cloud...");
    
    // Send system status
    StaticJsonDocument<400> doc;
    doc["device_id"] = wifiManager.getConfig().deviceId;
    doc["timestamp"] = time(nullptr);
    doc["status"] = "online";
    doc["wifi_strength"] = wifiManager.getSignalStrength();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["uptime"] = (millis() - systemStartTime) / 1000;
    
    // Model status
    JsonObject models = doc.createNestedObject("models");
    models["lstm_loaded"] = edgeAI.isModelLoaded(MODEL_MOISTURE_LSTM);
    models["anomaly_loaded"] = edgeAI.isModelLoaded(MODEL_ANOMALY_AUTOENCODER);
    
    // Statistics
    JsonObject statistics = doc.createNestedObject("statistics");
    statistics["predictions"] = stats.totalPredictions;
    statistics["anomalies"] = stats.totalAnomalies;
    statistics["cloud_syncs"] = stats.successfulCloudSyncs;
    
    String payload;
    serializeJson(doc, payload);
    
    String topic = "irrigation/" + String(wifiManager.getConfig().deviceId) + "/status";
    wifiManager.publishStatus(payload);
}

void sendStatusReport() {
    Serial.println("=== ESP32 EDGE AI STATUS ===");
    Serial.print("WiFi: ");
    Serial.println(wifiManager.getStatusString());
    Serial.print("IP: ");
    Serial.println(wifiManager.getLocalIP());
    Serial.print("Signal: ");
    Serial.print(wifiManager.getSignalStrength());
    Serial.println(" dBm");
    
    Serial.print("Models loaded: LSTM=");
    Serial.print(edgeAI.isModelLoaded(MODEL_MOISTURE_LSTM) ? "Yes" : "No");
    Serial.print(", Anomaly=");
    Serial.println(edgeAI.isModelLoaded(MODEL_ANOMALY_AUTOENCODER) ? "Yes" : "No");
    
    Serial.print("Data buffer: ");
    Serial.print(edgeAI.hasEnoughData() ? "Ready" : "Collecting");
    
    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    
    Serial.print("Predictions: ");
    Serial.print(stats.totalPredictions);
    Serial.print(", Anomalies: ");
    Serial.print(stats.totalAnomalies);
    Serial.print(", Avg time: ");
    Serial.print(stats.averagePredictionTime);
    Serial.println("ms");
    
    Serial.println("============================");
}

void configureTime() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    
    Serial.print("Waiting for NTP time sync");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println();
    
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.print("Current time: ");
    Serial.println(asctime(&timeinfo));
}

void printSystemInfo() {
    Serial.println("=== SYSTEM INFORMATION ===");
    Serial.print("Chip Model: ");
    Serial.println(ESP.getChipModel());
    Serial.print("Chip Revision: ");
    Serial.println(ESP.getChipRevision());
    Serial.print("CPU Frequency: ");
    Serial.print(ESP.getCpuFreqMHz());
    Serial.println(" MHz");
    Serial.print("Flash Size: ");
    Serial.print(ESP.getFlashChipSize() / 1024 / 1024);
    Serial.println(" MB");
    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    Serial.print("MAC Address: ");
    Serial.println(wifiManager.getMACAddress());
    Serial.println("==========================");
}

// WiFi callback functions
void onWiFiStatusChange(WiFiStatus status) {
    switch (status) {
        case WIFI_CONNECTED:
            Serial.println("WiFi connected!");
            Serial.print("IP address: ");
            Serial.println(wifiManager.getLocalIP());
            break;
        case WIFI_DISCONNECTED:
            Serial.println("WiFi disconnected");
            break;
        case WIFI_AP_MODE:
            Serial.println("WiFi in AP mode - visit 192.168.4.1 to configure");
            break;
        case WIFI_ERROR:
            Serial.println("WiFi error occurred");
            break;
        default:
            break;
    }
}

void onMQTTMessage(String topic, String message) {
    Serial.print("MQTT message received on topic: ");
    Serial.println(topic);
    Serial.print("Message: ");
    Serial.println(message);
    
    // Handle OTA update commands
    if (topic.endsWith("/ota")) {
        handleOTACommand(message);
    }
    
    // Handle model update commands
    if (topic.endsWith("/models")) {
        handleModelUpdateCommand(message);
    }
    
    // Handle configuration updates
    if (topic.endsWith("/config")) {
        handleConfigUpdateCommand(message);
    }
}

void handleOTACommand(const String& message) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, message);
    
    if (doc["action"] == "update" && doc.containsKey("url")) {
        Serial.println("OTA update requested");
        // Implement OTA update logic here
    }
}

void handleModelUpdateCommand(const String& message) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, message);
    
    if (doc["action"] == "reload") {
        Serial.println("Model reload requested");
        loadMLModels();
    }
}

void handleConfigUpdateCommand(const String& message) {
    StaticJsonDocument<300> doc;
    deserializeJson(doc, message);
    
    if (doc.containsKey("prediction_interval")) {
        // Update configuration as needed
        Serial.println("Configuration update received");
    }
}

// Placeholder model data (in production, these would be real trained models)
const unsigned char moisture_lstm_model[] PROGMEM = {0x00}; // Placeholder
const unsigned char anomaly_autoencoder_model[] PROGMEM = {0x00}; // Placeholder
const size_t moisture_lstm_model_len = 1;
const size_t anomaly_autoencoder_model_len = 1;