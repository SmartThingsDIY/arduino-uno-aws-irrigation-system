// Only use one set of include guards
#ifndef LOCALMLENGINE_H
#define LOCALMLENGINE_H

#include <Arduino.h>
#include "PlantTypes.h"

// Debug mode - comment out to disable
// #define DEBUG_ML 1

// Action structure for pump control
struct LocalAction {
    bool shouldWater;
    unsigned int waterDuration; // milliseconds
    float waterAmount;          // ml (calculated)
    bool isFailsafe;            // emergency watering

    // Constructor with defaults
    LocalAction() : shouldWater(false), waterDuration(0),
               waterAmount(0), isFailsafe(false) {}
};

// Sensor data structure
struct LocalSensorData {
    float moisture;            // 0-1023 analog reading
    float temperature;         // Celsius
    float humidity;            // Percentage
    float lightLevel;          // 0-1023 analog reading
    unsigned long lastWatered; // Hours since last watering
    PlantType plantType;
    GrowthStage growthStage;

    // Constructor with defaults
    LocalSensorData() : moisture(0), temperature(25), humidity(50),
                   lightLevel(500), lastWatered(0),
                   plantType(TOMATO), growthStage(VEGETATIVE) {}
};

// Forward declarations
class DecisionTree;
class LookupTable;
class AnomalyDetector;

class LocalMLEngine {
private:
    DecisionTree* irrigationTree;
    LookupTable* plantThresholds;
    AnomalyDetector* sensorMonitor;
    PlantType plantTypes[4];
    GrowthStage growthStages[4];
    unsigned long lastWateringTime[4];
public:
    LocalMLEngine();
    bool begin();
    void setPlantType(int sensorIndex, PlantType type);
    void setGrowthStage(int sensorIndex, GrowthStage stage);
    float predictWaterNeed(const LocalSensorData &data);
    bool detectAnomaly(const LocalSensorData &data);
    LocalAction getImmediateAction(int sensorIndex, const LocalSensorData &data);
    float calculateFeatureScore(const LocalSensorData &data);
    WaterAmount mapToWaterAmount(float prediction);
    unsigned int calculateWaterDuration(WaterAmount amount);
    void updatePlantThresholds(PlantType type, float moistureThreshold, float tempOptimal, float humidityOptimal);
    void setFailsafeMode(bool enabled);
    bool isTimeToWater(int sensorIndex, unsigned long currentTime);
    float getMoistureThreshold(PlantType type, GrowthStage stage);
    void recordWatering(int sensorIndex, unsigned long timestamp);
    unsigned long getInferenceCount();
    float getAverageInferenceTime();
    void resetStats();
};

#endif // LOCALMLENGINE_H