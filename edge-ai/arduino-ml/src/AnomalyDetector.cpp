/* * AnomalyDetector.cpp
 *
 * This file implements the AnomalyDetector class for detecting anomalies in sensor data.
 * It uses statistical methods to analyze sensor readings and identify potential anomalies.
 *
 * Author: iLyas Bakouch
 * GitHub: https://github.com/isbkch
 * Version: 0.78.3
 * Date: 2025
 */

#include "AnomalyDetector.h"
#include <math.h>

// Global anomaly threshold
static float anomalyThreshold = ANOMALY_THRESHOLD;

AnomalyDetector::AnomalyDetector() : bufferIndex(0), bufferSize(0)
{
    // Initialize sensor history buffer
    for (int i = 0; i < ANOMALY_BUFFER_SIZE; i++)
    {
        sensorHistory[i] = SensorData();
    }
}

bool AnomalyDetector::begin()
{
    // Reset all statistics
    resetStatistics();
    return true;
}

float AnomalyDetector::calculateAnomalyScore(const SensorData &data)
{
    // Add new data to history
    addToBuffer(data);

    // Update statistics if we have enough data
    if (bufferSize >= MIN_SAMPLES)
    {
        updateStatistics();
    }
    else
    {
        // Not enough data for reliable anomaly detection
        return 0.0;
    }

    // Calculate z-scores for each sensor
    float moistureZ = calculateZScore(data.moisture, moistureStats);
    float temperatureZ = calculateZScore(data.temperature, temperatureStats);
    float humidityZ = calculateZScore(data.humidity, humidityStats);
    float lightZ = calculateZScore(data.lightLevel, lightStats);

    // Calculate combined anomaly score using maximum z-score
    float maxZ = max(max(abs(moistureZ), abs(temperatureZ)),
                     max(abs(humidityZ), abs(lightZ)));

    // Convert z-score to probability (using cumulative distribution function approximation)
    float probability = 0.5 * (1.0 + tanh(maxZ / sqrt(2.0)));

    return probability;
}

bool AnomalyDetector::isMoistureAnomaly(float moisture)
{
    if (!hasEnoughData())
        return false;

    float zScore = calculateZScore(moisture, moistureStats);
    return abs(zScore) > anomalyThreshold;
}

bool AnomalyDetector::isTemperatureAnomaly(float temperature)
{
    if (!hasEnoughData())
        return false;

    float zScore = calculateZScore(temperature, temperatureStats);
    return abs(zScore) > anomalyThreshold;
}

bool AnomalyDetector::isHumidityAnomaly(float humidity)
{
    if (!hasEnoughData())
        return false;

    float zScore = calculateZScore(humidity, humidityStats);
    return abs(zScore) > anomalyThreshold;
}

bool AnomalyDetector::isLightAnomaly(float light)
{
    if (!hasEnoughData())
        return false;

    float zScore = calculateZScore(light, lightStats);
    return abs(zScore) > anomalyThreshold;
}

bool AnomalyDetector::isSensorFault(const SensorData &data)
{
    // Check for sensor disconnection (readings at extremes)
    if (isSensorDisconnected(data))
        return true;

    // Check for out-of-range values
    if (isSensorOutOfRange(data))
        return true;

    // Check for statistical anomalies
    return calculateAnomalyScore(data) > 0.997; // 99.7% confidence
}

bool AnomalyDetector::isSensorDisconnected(const SensorData &data)
{
    // Check for typical disconnection patterns

    // Moisture sensor disconnected (usually reads 1023 or 0)
    if (data.moisture <= 5 || data.moisture >= 1018)
        return true;

    // Temperature sensor disconnected (usually reads extreme values)
    if (data.temperature <= -50 || data.temperature >= 80)
        return true;

    // Humidity sensor disconnected (usually reads 0 or 100)
    if (data.humidity <= 1 || data.humidity >= 99)
        return true;

    // Light sensor disconnected (usually reads 0 or 1023)
    if (data.lightLevel <= 5 || data.lightLevel >= 1018)
        return true;

    return false;
}

bool AnomalyDetector::isSensorOutOfRange(const SensorData &data)
{
    // Check for physically impossible values

    // Moisture: 0-1023 (analog reading)
    if (data.moisture < 0 || data.moisture > 1023)
        return true;

    // Temperature: -40 to 70°C (typical sensor range)
    if (data.temperature < -40 || data.temperature > 70)
        return true;

    // Humidity: 0-100%
    if (data.humidity < 0 || data.humidity > 100)
        return true;

    // Light: 0-1023 (analog reading)
    if (data.lightLevel < 0 || data.lightLevel > 1023)
        return true;

    return false;
}

void AnomalyDetector::addToBuffer(const SensorData &data)
{
    // Add data to circular buffer
    sensorHistory[bufferIndex] = data;

    // Update buffer index
    bufferIndex = (bufferIndex + 1) % ANOMALY_BUFFER_SIZE;

    // Update buffer size (up to maximum)
    if (bufferSize < ANOMALY_BUFFER_SIZE)
    {
        bufferSize++;
    }
}

void AnomalyDetector::updateStatistics()
{
    // Extract sensor values from history
    float moisture[ANOMALY_BUFFER_SIZE];
    float temperature[ANOMALY_BUFFER_SIZE];
    float humidity[ANOMALY_BUFFER_SIZE];
    float light[ANOMALY_BUFFER_SIZE];

    extractSensorValues(moisture, temperature, humidity, light);

    // Calculate statistics for each sensor
    calculateStats(moisture, bufferSize, moistureStats);
    calculateStats(temperature, bufferSize, temperatureStats);
    calculateStats(humidity, bufferSize, humidityStats);
    calculateStats(light, bufferSize, lightStats);
}

void AnomalyDetector::extractSensorValues(float *moisture, float *temperature,
                                          float *humidity, float *light)
{
    for (int i = 0; i < bufferSize; i++)
    {
        moisture[i] = sensorHistory[i].moisture;
        temperature[i] = sensorHistory[i].temperature;
        humidity[i] = sensorHistory[i].humidity;
        light[i] = sensorHistory[i].lightLevel;
    }
}

void AnomalyDetector::calculateStats(float *values, uint8_t count, SensorStats &stats)
{
    if (count == 0)
        return;

    // Calculate mean
    float sum = 0;
    stats.min = values[0];
    stats.max = values[0];

    for (int i = 0; i < count; i++)
    {
        sum += values[i];
        if (values[i] < stats.min)
            stats.min = values[i];
        if (values[i] > stats.max)
            stats.max = values[i];
    }

    stats.mean = sum / count;

    // Calculate variance
    float varianceSum = 0;
    for (int i = 0; i < count; i++)
    {
        float diff = values[i] - stats.mean;
        varianceSum += diff * diff;
    }

    stats.variance = varianceSum / count;
    stats.stdDev = sqrt(stats.variance);
    stats.sampleCount = count;
}

float AnomalyDetector::calculateZScore(float value, const SensorStats &stats)
{
    if (stats.stdDev == 0)
        return 0; // Avoid division by zero

    return (value - stats.mean) / stats.stdDev;
}

SensorStats AnomalyDetector::getMoistureStats() const
{
    return moistureStats;
}

SensorStats AnomalyDetector::getTemperatureStats() const
{
    return temperatureStats;
}

SensorStats AnomalyDetector::getHumidityStats() const
{
    return humidityStats;
}

SensorStats AnomalyDetector::getLightStats() const
{
    return lightStats;
}

void AnomalyDetector::setAnomalyThreshold(float threshold)
{
    anomalyThreshold = threshold;
}

void AnomalyDetector::resetStatistics()
{
    moistureStats = SensorStats();
    temperatureStats = SensorStats();
    humidityStats = SensorStats();
    lightStats = SensorStats();
}

void AnomalyDetector::clearHistory()
{
    bufferIndex = 0;
    bufferSize = 0;

    for (int i = 0; i < ANOMALY_BUFFER_SIZE; i++)
    {
        sensorHistory[i] = SensorData();
    }

    resetStatistics();
}

uint8_t AnomalyDetector::getHistoryCount() const
{
    return bufferSize;
}

bool AnomalyDetector::hasEnoughData() const
{
    return bufferSize >= MIN_SAMPLES;
}

size_t AnomalyDetector::getMemoryUsage() const
{
    return sizeof(sensorHistory) + sizeof(moistureStats) +
           sizeof(temperatureStats) + sizeof(humidityStats) + sizeof(lightStats);
}

void AnomalyDetector::printStatistics() const
{
    Serial.println("Anomaly Detection Statistics:");
    Serial.print("Buffer size: ");
    Serial.println(bufferSize);

    Serial.println("\nMoisture Stats:");
    Serial.print("  Mean: ");
    Serial.println(moistureStats.mean);
    Serial.print("  StdDev: ");
    Serial.println(moistureStats.stdDev);
    Serial.print("  Min: ");
    Serial.println(moistureStats.min);
    Serial.print("  Max: ");
    Serial.println(moistureStats.max);

    Serial.println("\nTemperature Stats:");
    Serial.print("  Mean: ");
    Serial.println(temperatureStats.mean);
    Serial.print("  StdDev: ");
    Serial.println(temperatureStats.stdDev);
    Serial.print("  Min: ");
    Serial.println(temperatureStats.min);
    Serial.print("  Max: ");
    Serial.println(temperatureStats.max);

    Serial.println("\nHumidity Stats:");
    Serial.print("  Mean: ");
    Serial.println(humidityStats.mean);
    Serial.print("  StdDev: ");
    Serial.println(humidityStats.stdDev);
    Serial.print("  Min: ");
    Serial.println(humidityStats.min);
    Serial.print("  Max: ");
    Serial.println(humidityStats.max);

    Serial.println("\nLight Stats:");
    Serial.print("  Mean: ");
    Serial.println(lightStats.mean);
    Serial.print("  StdDev: ");
    Serial.println(lightStats.stdDev);
    Serial.print("  Min: ");
    Serial.println(lightStats.min);
    Serial.print("  Max: ");
    Serial.println(lightStats.max);

    Serial.print("\nMemory usage: ");
    Serial.print(getMemoryUsage());
    Serial.println(" bytes");
}

void AnomalyDetector::printHistory() const
{
    Serial.println("Sensor History:");
    Serial.println("Index\tMoisture\tTemp\tHumidity\tLight");

    for (int i = 0; i < bufferSize; i++)
    {
        Serial.print(i);
        Serial.print("\t");
        Serial.print(sensorHistory[i].moisture);
        Serial.print("\t\t");
        Serial.print(sensorHistory[i].temperature);
        Serial.print("\t");
        Serial.print(sensorHistory[i].humidity);
        Serial.print("\t\t");
        Serial.println(sensorHistory[i].lightLevel);
    }
}