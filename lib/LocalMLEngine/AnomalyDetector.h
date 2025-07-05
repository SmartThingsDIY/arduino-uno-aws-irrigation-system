#ifndef ANOMALY_DETECTOR_H
#define ANOMALY_DETECTOR_H

#include <Arduino.h>
#include "PlantTypes.h"

// Configuration constants
#define ANOMALY_BUFFER_SIZE 24    // 24-hour rolling window
#define ANOMALY_THRESHOLD 3.0     // 3 standard deviations
#define MIN_SAMPLES 5             // Minimum samples for statistics

// Forward declaration - SensorData defined in LocalMLEngine.h
struct SensorData;

// Statistical data for each sensor type
struct SensorStats {
    float mean;
    float variance;
    float stdDev;
    float minVal;
    float maxVal;
    uint8_t sampleCount;
    
    // Constructor
    SensorStats() : mean(0), variance(0), stdDev(0), minVal(999999), maxVal(-999999), sampleCount(0) {}
};

class AnomalyDetector {
private:
    // Circular buffer for sensor history
    SensorData sensorHistory[ANOMALY_BUFFER_SIZE];
    uint8_t bufferIndex;
    uint8_t bufferSize;
    
    // Statistical data for each sensor type
    SensorStats moistureStats;
    SensorStats temperatureStats;
    SensorStats humidityStats;
    SensorStats lightStats;
    
    // Internal methods
    void updateStatistics();
    void calculateStats(float* values, uint8_t count, SensorStats& stats);
    float calculateZScore(float value, const SensorStats& stats);
    
    // Utility methods
    void addToBuffer(const SensorData& data);
    void extractSensorValues(float* moisture, float* temperature, float* humidity, float* light);
    
public:
    AnomalyDetector();
    
    // Initialization
    bool begin();
    
    // Main anomaly detection method
    float calculateAnomalyScore(const SensorData& data);
    
    // Individual sensor anomaly detection
    bool isMoistureAnomaly(float moisture);
    bool isTemperatureAnomaly(float temperature);
    bool isHumidityAnomaly(float humidity);
    bool isLightAnomaly(float light);
    
    // Sensor fault detection
    bool isSensorFault(const SensorData& data);
    bool isSensorDisconnected(const SensorData& data);
    bool isSensorOutOfRange(const SensorData& data);
    
    // Statistics access
    SensorStats getMoistureStats() const;
    SensorStats getTemperatureStats() const;
    SensorStats getHumidityStats() const;
    SensorStats getLightStats() const;
    
    // Configuration
    void setAnomalyThreshold(float threshold);
    void resetStatistics();
    void clearHistory();
    
    // Utility methods
    uint8_t getHistoryCount() const;
    bool hasEnoughData() const;
    size_t getMemoryUsage() const;
    
    // Debugging
    void printStatistics() const;
    void printHistory() const;
};

#endif // ANOMALY_DETECTOR_H