#include "DataBuffer.h"
#include <math.h>

DataBuffer::DataBuffer() : sensorHistory(nullptr), bufferSize(0), bufferCapacity(0), 
                           writeIndex(0), statsValid(false) {
}

DataBuffer::DataBuffer(uint16_t capacity) : sensorHistory(nullptr), bufferSize(0), 
                                           bufferCapacity(0), writeIndex(0), statsValid(false) {
    begin(capacity);
}

DataBuffer::~DataBuffer() {
    end();
}

bool DataBuffer::begin(uint16_t capacity) {
    if (capacity == 0 || capacity > MAX_SENSOR_HISTORY) {
        Serial.println("Error: Invalid buffer capacity");
        return false;
    }
    
    if (!allocateBuffer(capacity)) {
        Serial.println("Error: Failed to allocate buffer memory");
        return false;
    }
    
    Serial.print("DataBuffer initialized with capacity: ");
    Serial.println(capacity);
    return true;
}

void DataBuffer::end() {
    deallocateBuffer();
    bufferSize = 0;
    bufferCapacity = 0;
    writeIndex = 0;
    statsValid = false;
}

bool DataBuffer::allocateBuffer(uint16_t capacity) {
    // Free existing buffer if any
    deallocateBuffer();
    
    // Allocate new buffer
    sensorHistory = (SensorData*)malloc(capacity * sizeof(SensorData));
    if (sensorHistory == nullptr) {
        return false;
    }
    
    bufferCapacity = capacity;
    
    // Initialize buffer
    for (uint16_t i = 0; i < capacity; i++) {
        sensorHistory[i] = SensorData();
    }
    
    return true;
}

void DataBuffer::deallocateBuffer() {
    if (sensorHistory != nullptr) {
        free(sensorHistory);
        sensorHistory = nullptr;
    }
}

bool DataBuffer::addSample(const SensorData& data) {
    if (sensorHistory == nullptr) {
        return false;
    }
    
    // Validate sample
    if (!validateSample(data)) {
        Serial.println("Warning: Invalid sensor data rejected");
        return false;
    }
    
    // Add to circular buffer
    sensorHistory[writeIndex] = data;
    writeIndex = (writeIndex + 1) % bufferCapacity;
    
    // Update buffer size
    if (bufferSize < bufferCapacity) {
        bufferSize++;
    }
    
    // Invalidate statistics (will be recalculated when needed)
    statsValid = false;
    
    return true;
}

bool DataBuffer::getSample(uint16_t index, SensorData& data) const {
    if (sensorHistory == nullptr || index >= bufferSize) {
        return false;
    }
    
    // Calculate actual index in circular buffer
    uint16_t actualIndex;
    if (bufferSize < bufferCapacity) {
        actualIndex = index;
    } else {
        actualIndex = (writeIndex + index) % bufferCapacity;
    }
    
    data = sensorHistory[actualIndex];
    return true;
}

SensorData DataBuffer::getLatestSample() const {
    if (bufferSize == 0) {
        return SensorData();
    }
    
    uint16_t latestIndex = (writeIndex - 1 + bufferCapacity) % bufferCapacity;
    return sensorHistory[latestIndex];
}

bool DataBuffer::extractFeatures(float* features, uint16_t windowSize, bool normalize) {
    if (features == nullptr || windowSize == 0 || windowSize > bufferSize) {
        return false;
    }
    
    // Update statistics if needed for normalization
    if (normalize && !statsValid) {
        updateStatistics();
    }
    
    // Extract features for the specified window
    for (uint16_t i = 0; i < windowSize; i++) {
        SensorData sample;
        if (!getSample(bufferSize - windowSize + i, sample)) {
            return false;
        }
        
        // Pack features into array
        uint16_t baseIndex = i * FEATURES_PER_SAMPLE;
        features[baseIndex + 0] = normalize ? normalizeFeature(sample.moisture, 0) : sample.moisture;
        features[baseIndex + 1] = normalize ? normalizeFeature(sample.temperature, 1) : sample.temperature;
        features[baseIndex + 2] = normalize ? normalizeFeature(sample.humidity, 2) : sample.humidity;
        features[baseIndex + 3] = normalize ? normalizeFeature(sample.lightLevel, 3) : sample.lightLevel;
        features[baseIndex + 4] = normalize ? normalizeFeature(sample.hour, 4) : sample.hour;
        features[baseIndex + 5] = normalize ? normalizeFeature(sample.dayOfWeek, 5) : sample.dayOfWeek;
        features[baseIndex + 6] = normalize ? normalizeFeature(sample.month, 6) : sample.month;
    }
    
    return true;
}

bool DataBuffer::extractRawWindow(float* window, uint16_t windowSize, int featureIndex) {
    if (window == nullptr || windowSize == 0 || windowSize > bufferSize || 
        featureIndex < 0 || featureIndex >= FEATURES_PER_SAMPLE) {
        return false;
    }
    
    for (uint16_t i = 0; i < windowSize; i++) {
        SensorData sample;
        if (!getSample(bufferSize - windowSize + i, sample)) {
            return false;
        }
        
        switch (featureIndex) {
            case 0: window[i] = sample.moisture; break;
            case 1: window[i] = sample.temperature; break;
            case 2: window[i] = sample.humidity; break;
            case 3: window[i] = sample.lightLevel; break;
            case 4: window[i] = sample.hour; break;
            case 5: window[i] = sample.dayOfWeek; break;
            case 6: window[i] = sample.month; break;
            default: return false;
        }
    }
    
    return true;
}

bool DataBuffer::extractTimeSeriesWindow(float* window, uint16_t windowSize) {
    // Extract moisture values as the primary time series
    return extractRawWindow(window, windowSize, 0);
}

void DataBuffer::updateStatistics() {
    if (bufferSize < 2) {
        statsValid = false;
        return;
    }
    
    for (int feature = 0; feature < FEATURES_PER_SAMPLE; feature++) {
        calculateFeatureStats(feature);
    }
    
    statsValid = true;
}

void DataBuffer::calculateFeatureStats(int featureIndex) {
    if (featureIndex < 0 || featureIndex >= FEATURES_PER_SAMPLE) {
        return;
    }
    
    FeatureStats& stats = featureStats[featureIndex];
    
    // Reset stats
    stats = FeatureStats();
    
    // Calculate mean, min, max
    float sum = 0;
    for (uint16_t i = 0; i < bufferSize; i++) {
        SensorData sample;
        getSample(i, sample);
        
        float value;
        switch (featureIndex) {
            case 0: value = sample.moisture; break;
            case 1: value = sample.temperature; break;
            case 2: value = sample.humidity; break;
            case 3: value = sample.lightLevel; break;
            case 4: value = sample.hour; break;
            case 5: value = sample.dayOfWeek; break;
            case 6: value = sample.month; break;
            default: return;
        }
        
        sum += value;
        if (value < stats.min) stats.min = value;
        if (value > stats.max) stats.max = value;
    }
    
    stats.mean = sum / bufferSize;
    stats.sampleCount = bufferSize;
    
    // Calculate standard deviation
    float varianceSum = 0;
    for (uint16_t i = 0; i < bufferSize; i++) {
        SensorData sample;
        getSample(i, sample);
        
        float value;
        switch (featureIndex) {
            case 0: value = sample.moisture; break;
            case 1: value = sample.temperature; break;
            case 2: value = sample.humidity; break;
            case 3: value = sample.lightLevel; break;
            case 4: value = sample.hour; break;
            case 5: value = sample.dayOfWeek; break;
            case 6: value = sample.month; break;
            default: return;
        }
        
        float diff = value - stats.mean;
        varianceSum += diff * diff;
    }
    
    float variance = varianceSum / bufferSize;
    stats.stdDev = sqrt(variance);
}

float DataBuffer::normalizeFeature(float value, int featureIndex) {
    if (!statsValid || featureIndex < 0 || featureIndex >= FEATURES_PER_SAMPLE) {
        return value;
    }
    
    const FeatureStats& stats = featureStats[featureIndex];
    
    // Avoid division by zero
    if (stats.stdDev < 0.001) {
        return 0.0;
    }
    
    // Z-score normalization
    return (value - stats.mean) / stats.stdDev;
}

bool DataBuffer::validateSample(const SensorData& data) const {
    // Check for reasonable ranges
    if (data.moisture < 0 || data.moisture > 4095) return false;
    if (data.temperature < -40 || data.temperature > 80) return false;
    if (data.humidity < 0 || data.humidity > 100) return false;
    if (data.lightLevel < 0 || data.lightLevel > 4095) return false;
    if (data.hour > 23) return false;
    if (data.dayOfWeek > 6) return false;
    if (data.month < 1 || data.month > 12) return false;
    
    return true;
}

uint16_t DataBuffer::getSize() const {
    return bufferSize;
}

uint16_t DataBuffer::getCapacity() const {
    return bufferCapacity;
}

bool DataBuffer::isEmpty() const {
    return bufferSize == 0;
}

bool DataBuffer::isFull() const {
    return bufferSize == bufferCapacity;
}

bool DataBuffer::hasMinimumData(uint16_t minSamples) const {
    return bufferSize >= minSamples;
}

FeatureStats DataBuffer::getFeatureStats(int featureIndex) const {
    if (featureIndex >= 0 && featureIndex < FEATURES_PER_SAMPLE) {
        return featureStats[featureIndex];
    }
    return FeatureStats();
}

void DataBuffer::updateFeatureStats() {
    updateStatistics();
}

bool DataBuffer::areStatsValid() const {
    return statsValid;
}

void DataBuffer::invalidateStats() {
    statsValid = false;
}

void DataBuffer::clear() {
    bufferSize = 0;
    writeIndex = 0;
    statsValid = false;
    
    // Clear buffer contents
    if (sensorHistory != nullptr) {
        for (uint16_t i = 0; i < bufferCapacity; i++) {
            sensorHistory[i] = SensorData();
        }
    }
}

size_t DataBuffer::getMemoryUsage() const {
    return bufferCapacity * sizeof(SensorData) + sizeof(featureStats);
}

void DataBuffer::printBuffer(uint16_t maxSamples) const {
    Serial.println("=== DataBuffer Contents ===");
    Serial.print("Size: ");
    Serial.print(bufferSize);
    Serial.print(" / ");
    Serial.println(bufferCapacity);
    
    uint16_t samplesToShow = min(maxSamples, bufferSize);
    Serial.println("Recent samples:");
    Serial.println("Index\tMoist\tTemp\tHumid\tLight\tHour");
    
    for (uint16_t i = 0; i < samplesToShow; i++) {
        SensorData sample;
        if (getSample(bufferSize - samplesToShow + i, sample)) {
            Serial.print(i);
            Serial.print("\t");
            Serial.print(sample.moisture);
            Serial.print("\t");
            Serial.print(sample.temperature);
            Serial.print("\t");
            Serial.print(sample.humidity);
            Serial.print("\t");
            Serial.print(sample.lightLevel);
            Serial.print("\t");
            Serial.println(sample.hour);
        }
    }
}

void DataBuffer::printStatistics() const {
    if (!statsValid) {
        Serial.println("Statistics not available");
        return;
    }
    
    Serial.println("=== Feature Statistics ===");
    const char* featureNames[] = {"Moisture", "Temp", "Humidity", "Light", "Hour", "Day", "Month"};
    
    for (int i = 0; i < FEATURES_PER_SAMPLE; i++) {
        Serial.print(featureNames[i]);
        Serial.print(":\t");
        Serial.print("Mean=");
        Serial.print(featureStats[i].mean);
        Serial.print(", StdDev=");
        Serial.print(featureStats[i].stdDev);
        Serial.print(", Min=");
        Serial.print(featureStats[i].min);
        Serial.print(", Max=");
        Serial.println(featureStats[i].max);
    }
}