#ifndef LOCALMLENGINE_H
#define LOCALMLENGINE_H

// Minimal stub implementation for LocalMLEngine for compilation and testing.
struct Action {
    bool shouldWater = false;
    unsigned long waterDuration = 0;
    float waterAmount = 0;
    bool isFailsafe = false;
};

struct SensorData {
    float moisture = 0;
    float temperature = 0;
    float humidity = 0;
    float lightLevel = 0;
};

class LocalMLEngine {
public:
    bool begin() { return true; }
    void setPlantType(int idx, int type) {}
    void setGrowthStage(int idx, int stage) {}
    Action getImmediateAction(int idx, const SensorData& data) { return Action(); }
    bool detectAnomaly(const SensorData& data) { return false; }
    float getAverageInferenceTime() { return 1.5; }
    void resetStats() {}
};

#endif // LOCALMLENGINE_H
#ifndef LOCAL_ML_ENGINE_H
#define LOCAL_ML_ENGINE_H

#include <Arduino.h>
#include "PlantTypes.h"

// Debug mode - comment out to disable
// #define DEBUG_ML 1

// Sensor data structure
struct SensorData
{
    float moisture;            // 0-1023 analog reading
    float temperature;         // Celsius
    float humidity;            // Percentage
    float lightLevel;          // 0-1023 analog reading
    unsigned long lastWatered; // Hours since last watering
    PlantType plantType;
    GrowthStage growthStage;

    // Constructor with defaults
    SensorData() : moisture(0), temperature(25), humidity(50),
                   lightLevel(500), lastWatered(0),
                   plantType(TOMATO), growthStage(VEGETATIVE) {}
};

// Action structure for pump control
struct Action
{
    bool shouldWater;
    unsigned int waterDuration; // milliseconds
    float waterAmount;          // ml (calculated)
    bool isFailsafe;            // emergency watering

    // Constructor with defaults
    Action() : shouldWater(false), waterDuration(0),
               waterAmount(0), isFailsafe(false) {}
};

// Forward declarations
class DecisionTree;
class LookupTable;
class AnomalyDetector;

class LocalMLEngine
{
private:
    DecisionTree* irrigationTree;
    LookupTable* plantThresholds;
    AnomalyDetector* sensorMonitor;

    // Plant configurations per sensor
    PlantType plantTypes[4];
    GrowthStage growthStages[4];
    unsigned long lastWateringTime[4];

    // Internal calculation methods
    float calculateFeatureScore(const SensorData &data);
    WaterAmount mapToWaterAmount(float prediction);
    unsigned int calculateWaterDuration(WaterAmount amount);

#ifdef DEBUG_ML
    void debugPrint(const char *message, float value = 0);
#endif

public:
    LocalMLEngine();

    // Initialization
    bool begin();
    void setPlantType(int sensorIndex, PlantType type);
    void setGrowthStage(int sensorIndex, GrowthStage stage);

    // Main inference methods
    float predictWaterNeed(const SensorData &data);
    bool detectAnomaly(const SensorData &data);
    Action getImmediateAction(int sensorIndex, const SensorData &data);

    // Configuration methods
    void updatePlantThresholds(PlantType type, float moistureThreshold,
                               float tempOptimal, float humidityOptimal);
    void setFailsafeMode(bool enabled);

    // Utility methods
    bool isTimeToWater(int sensorIndex, unsigned long currentTime);
    float getMoistureThreshold(PlantType type, GrowthStage stage);
    void recordWatering(int sensorIndex, unsigned long timestamp);

    // Statistics
    unsigned long getInferenceCount();
    float getAverageInferenceTime();
    void resetStats();
};

#endif // LOCAL_ML_ENGINE_H