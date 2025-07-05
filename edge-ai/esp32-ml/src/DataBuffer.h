#ifndef DATA_BUFFER_H
#define DATA_BUFFER_H

#include <Arduino.h>

// Buffer configuration
#define MAX_SENSOR_HISTORY 168  // 7 days * 24 hours
#define FEATURES_PER_SAMPLE 7   // moisture, temp, humidity, light, hour, day, month

// Sensor data structure
struct SensorData {
    float moisture;        // Analog reading (0-4095 on ESP32)
    float temperature;     // Celsius
    float humidity;        // Percentage
    float lightLevel;      // Analog reading (0-4095)
    float pressure;        // hPa (if BME280 available)
    uint8_t hour;          // Hour of day (0-23)
    uint8_t dayOfWeek;     // Day of week (0-6)
    uint8_t month;         // Month (1-12)
    unsigned long timestamp; // Unix timestamp
    
    // Constructor with defaults
    SensorData() : moisture(0), temperature(25), humidity(50), lightLevel(500), 
                   pressure(1013.25), hour(12), dayOfWeek(0), month(1), timestamp(0) {}
                   
    SensorData(float m, float t, float h, float l) : 
        moisture(m), temperature(t), humidity(h), lightLevel(l),
        pressure(1013.25), hour(12), dayOfWeek(0), month(1), timestamp(millis()) {}
};

// Statistical summary for features
struct FeatureStats {
    float mean;
    float stdDev;
    float min;
    float max;
    uint16_t sampleCount;
    
    FeatureStats() : mean(0), stdDev(0), min(99999), max(-99999), sampleCount(0) {}
};

class DataBuffer {
private:
    // Circular buffer for sensor history
    SensorData* sensorHistory;
    uint16_t bufferSize;
    uint16_t bufferCapacity;
    uint16_t writeIndex;
    
    // Feature statistics for normalization
    FeatureStats featureStats[FEATURES_PER_SAMPLE];
    bool statsValid;
    
    // Internal methods
    void updateStatistics();
    void calculateFeatureStats(int featureIndex);
    float normalizeFeature(float value, int featureIndex);
    
    // Memory management
    bool allocateBuffer(uint16_t capacity);
    void deallocateBuffer();
    
public:
    DataBuffer();
    DataBuffer(uint16_t capacity);
    ~DataBuffer();
    
    // Initialization
    bool begin(uint16_t capacity = MAX_SENSOR_HISTORY);
    void end();
    
    // Data management
    bool addSample(const SensorData& data);
    bool getSample(uint16_t index, SensorData& data) const;
    SensorData getLatestSample() const;
    
    // Feature extraction
    bool extractFeatures(float* features, uint16_t windowSize, bool normalize = true);
    bool extractRawWindow(float* window, uint16_t windowSize, int featureIndex);
    bool extractTimeSeriesWindow(float* window, uint16_t windowSize);
    
    // Statistics
    uint16_t getSize() const;
    uint16_t getCapacity() const;
    bool isEmpty() const;
    bool isFull() const;
    bool hasMinimumData(uint16_t minSamples = 24) const;
    
    // Feature statistics
    FeatureStats getFeatureStats(int featureIndex) const;
    void updateFeatureStats();
    bool areStatsValid() const;
    void invalidateStats();
    
    // Data quality
    bool validateSample(const SensorData& data) const;
    float detectOutliers(int featureIndex, float threshold = 3.0) const;
    uint16_t countValidSamples() const;
    
    // Utilities
    void clear();
    size_t getMemoryUsage() const;
    void printBuffer(uint16_t maxSamples = 10) const;
    void printStatistics() const;
    
    // Export/Import
    bool exportToArray(SensorData* array, uint16_t maxSize) const;
    bool importFromArray(const SensorData* array, uint16_t size);
    
    // Time-based operations
    bool getSamplesInRange(unsigned long startTime, unsigned long endTime, 
                          SensorData* results, uint16_t maxResults) const;
    SensorData interpolateSample(unsigned long timestamp) const;
    
    // Advanced analytics
    float calculateTrend(int featureIndex, uint16_t windowSize = 24) const;
    float calculateSeasonality(int featureIndex, uint16_t period = 24) const;
    bool detectAnomalies(float* anomalyScores, uint16_t windowSize = 24) const;
};

#endif // DATA_BUFFER_H