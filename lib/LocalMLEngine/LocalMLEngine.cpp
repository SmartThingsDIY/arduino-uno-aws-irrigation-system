/* * LocalMLEngine.cpp
 *
 * This file implements the LocalMLEngine class for managing local machine learning operations
 * in an Arduino-based irrigation system. It handles plant-specific configurations, water need predictions,
 * anomaly detection, and immediate actions based on sensor data.
 *
 * Author: iLyas Bakouch
 * GitHub: https://github.com/SmartThingsDIY
 * Version: 0.78.3
 * Date: 2025
 */
#include "LocalMLEngine.h"
#include "DecisionTree.h"
#include "LookupTable.h"
#include "AnomalyDetector.h"

// Performance tracking
static unsigned long inferenceCount = 0;
static unsigned long totalInferenceTime = 0;
static bool failsafeEnabled = true;

LocalMLEngine::LocalMLEngine()
{
    // Initialize plant configurations with defaults
    for (int i = 0; i < 4; i++)
    {
        plantTypes[i] = TOMATO;
        growthStages[i] = VEGETATIVE;
        lastWateringTime[i] = 0;
    }
    
    // Initialize component pointers
    irrigationTree = new DecisionTree();
    plantThresholds = new LookupTable();
    sensorMonitor = new AnomalyDetector();
}

bool LocalMLEngine::begin()
{
    bool success = true;

    // Initialize components
    success &= irrigationTree->begin();
    success &= plantThresholds->begin();
    success &= sensorMonitor->begin();

#ifdef DEBUG_ML
    if (success)
    {
        debugPrint("LocalMLEngine initialized successfully");
    }
    else
    {
        debugPrint("LocalMLEngine initialization failed");
    }
#endif

    return success;
}

void LocalMLEngine::setPlantType(int sensorIndex, PlantType type)
{
    if (sensorIndex >= 0 && sensorIndex < 4)
    {
        plantTypes[sensorIndex] = type;

#ifdef DEBUG_ML
        debugPrint("Plant type set for sensor", sensorIndex);
        debugPrint("Type", type);
#endif
    }
}

void LocalMLEngine::setGrowthStage(int sensorIndex, GrowthStage stage)
{
    if (sensorIndex >= 0 && sensorIndex < 4)
    {
        growthStages[sensorIndex] = stage;

#ifdef DEBUG_ML
        debugPrint("Growth stage set for sensor", sensorIndex);
        debugPrint("Stage", stage);
#endif
    }
}

float LocalMLEngine::predictWaterNeed(const SensorData &data)
{
    unsigned long startTime = millis();

    // Calculate feature score (0-1 scale)
    float featureScore = calculateFeatureScore(data);

    // Use decision tree for prediction
    float prediction = irrigationTree->predict(featureScore);

    // Apply plant-specific adjustments
    float threshold = plantThresholds->getMoistureThreshold(data.plantType, data.growthStage);
    float adjustedPrediction = prediction * threshold;

    // Update performance metrics
    inferenceCount++;
    totalInferenceTime += (millis() - startTime);

#ifdef DEBUG_ML
    debugPrint("Feature score", featureScore);
    debugPrint("Raw prediction", prediction);
    debugPrint("Adjusted prediction", adjustedPrediction);
    debugPrint("Inference time (ms)", millis() - startTime);
#endif

    return adjustedPrediction;
}

bool LocalMLEngine::detectAnomaly(const SensorData &data)
{
    // Use anomaly detector for sensor fault detection
    float anomalyScore = sensorMonitor->calculateAnomalyScore(data);

    // Threshold for anomaly detection (3 standard deviations)
    const float ANOMALY_THRESHOLD_VAL = 0.997; // 99.7% confidence

    bool isAnomaly = anomalyScore > ANOMALY_THRESHOLD_VAL;

#ifdef DEBUG_ML
    debugPrint("Anomaly score", anomalyScore);
    debugPrint("Is anomaly", isAnomaly ? 1 : 0);
#endif

    return isAnomaly;
}

Action LocalMLEngine::getImmediateAction(int sensorIndex, const SensorData &data)
{
    Action action;

    // Validate sensor index
    if (sensorIndex < 0 || sensorIndex >= 4)
    {
        return action; // Return default (no action)
    }

    // Create modified sensor data with plant-specific info
    SensorData modifiedData = data;
    modifiedData.plantType = plantTypes[sensorIndex];
    modifiedData.growthStage = growthStages[sensorIndex];
    modifiedData.lastWatered = (millis() - lastWateringTime[sensorIndex]) / 3600000; // Convert to hours

    // Check for sensor anomalies first
    if (detectAnomaly(modifiedData))
    {
        if (failsafeEnabled)
        {
            // Failsafe watering based on simple thresholds
            float threshold = getMoistureThreshold(modifiedData.plantType, modifiedData.growthStage);
            if (modifiedData.moisture > threshold * 1.2)
            { // 20% above threshold
                action.shouldWater = true;
                action.waterDuration = calculateWaterDuration(MEDIUM_WATER);
                action.waterAmount = 100; // ml
                action.isFailsafe = true;

#ifdef DEBUG_ML
                debugPrint("Failsafe watering activated");
#endif
            }
        }
        return action;
    }

    // Normal ML prediction
    float waterNeed = predictWaterNeed(modifiedData);
    WaterAmount amount = mapToWaterAmount(waterNeed);

    if (amount > NO_WATER)
    {
        // Check time constraint (minimum 6 hours between watering)
        if (isTimeToWater(sensorIndex, millis()))
        {
            action.shouldWater = true;
            action.waterDuration = calculateWaterDuration(amount);
            action.waterAmount = amount * 50; // 50ml per unit
            action.isFailsafe = false;

            // Record watering time
            recordWatering(sensorIndex, millis());

#ifdef DEBUG_ML
            debugPrint("ML watering decision");
            debugPrint("Water amount", action.waterAmount);
            debugPrint("Duration (ms)", action.waterDuration);
#endif
        }
    }

    return action;
}

float LocalMLEngine::calculateFeatureScore(const SensorData &data)
{
    // Normalize features to 0-1 scale and calculate weighted score
    float moistureScore = constrain(data.moisture / 1023.0, 0.0, 1.0);
    float tempScore = constrain((data.temperature - 10) / 30.0, 0.0, 1.0); // 10-40°C range
    float humidityScore = constrain(data.humidity / 100.0, 0.0, 1.0);
    float lightScore = constrain(data.lightLevel / 1023.0, 0.0, 1.0);
    float timeScore = constrain(data.lastWatered / 48.0, 0.0, 1.0); // 48 hour max

    // Weighted combination (moisture is most important)
    float score = (moistureScore * 0.4) +
                  (tempScore * 0.2) +
                  (humidityScore * 0.2) +
                  (lightScore * 0.1) +
                  (timeScore * 0.1);

    return score;
}

WaterAmount LocalMLEngine::mapToWaterAmount(float prediction)
{
    // Map prediction to discrete water amounts
    if (prediction > 0.75)
        return HIGH_WATER;
    if (prediction > 0.5)
        return MEDIUM_WATER;
    if (prediction > 0.25)
        return LOW_WATER;
    return NO_WATER;
}

unsigned int LocalMLEngine::calculateWaterDuration(WaterAmount amount)
{
    // Calculate pump duration based on water amount
    // Assumes pump rate of 100ml/second
    switch (amount)
    {
    case LOW_WATER:
        return 500; // 0.5 seconds = 50ml
    case MEDIUM_WATER:
        return 1000; // 1 second = 100ml
    case HIGH_WATER:
        return 2000; // 2 seconds = 200ml
    default:
        return 0;
    }
}

void LocalMLEngine::updatePlantThresholds(PlantType type, float moistureThreshold,
                                          float tempOptimal, float humidityOptimal)
{
    plantThresholds->updateThresholds(type, moistureThreshold, tempOptimal, humidityOptimal);
}

void LocalMLEngine::setFailsafeMode(bool enabled)
{
    failsafeEnabled = enabled;

#ifdef DEBUG_ML
    debugPrint("Failsafe mode", enabled ? 1 : 0);
#endif
}

bool LocalMLEngine::isTimeToWater(int sensorIndex, unsigned long currentTime)
{
    const unsigned long MIN_WATERING_INTERVAL = 6 * 3600000; // 6 hours in milliseconds

    if (lastWateringTime[sensorIndex] == 0)
    {
        return true; // First watering
    }

    return (currentTime - lastWateringTime[sensorIndex]) > MIN_WATERING_INTERVAL;
}

float LocalMLEngine::getMoistureThreshold(PlantType type, GrowthStage stage)
{
    return plantThresholds->getMoistureThreshold(type, stage);
}

void LocalMLEngine::recordWatering(int sensorIndex, unsigned long timestamp)
{
    if (sensorIndex >= 0 && sensorIndex < 4)
    {
        lastWateringTime[sensorIndex] = timestamp;
    }
}

unsigned long LocalMLEngine::getInferenceCount()
{
    return inferenceCount;
}

float LocalMLEngine::getAverageInferenceTime()
{
    if (inferenceCount == 0)
        return 0;
    return (float)totalInferenceTime / inferenceCount;
}

void LocalMLEngine::resetStats()
{
    inferenceCount = 0;
    totalInferenceTime = 0;
}

#ifdef DEBUG_ML
void LocalMLEngine::debugPrint(const char *message, float value)
{
    Serial.print("[ML Debug] ");
    Serial.print(message);
    if (value != 0)
    {
        Serial.print(": ");
        Serial.print(value);
    }
    Serial.println();
}
#endif