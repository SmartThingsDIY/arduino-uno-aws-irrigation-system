#ifndef LOCAL_ML_ENGINE_H
#define LOCAL_ML_ENGINE_H

#include <Arduino.h>
#include "DecisionTree.h"
#include "LookupTable.h"
#include "AnomalyDetector.h"

// Debug mode - comment out to disable
// #define DEBUG_ML 1

// Plant type constants
enum PlantType {
    TOMATO = 0, LETTUCE, BASIL, MINT, PEPPER,
    ROSE, SUNFLOWER, MARIGOLD, PETUNIA, DAISY,
    STRAWBERRY, BLUEBERRY, RASPBERRY, GRAPE,
    CACTUS, SUCCULENT, FERN, ORCHID, BAMBOO, LAVENDER
};

// Growth stage constants
enum GrowthStage {
    SEEDLING = 0, VEGETATIVE, FLOWERING, FRUITING, MATURE
};

// Sensor data structure
struct SensorData {
    float moisture;        // 0-1023 analog reading
    float temperature;     // Celsius
    float humidity;        // Percentage
    float lightLevel;      // 0-1023 analog reading
    unsigned long lastWatered; // Hours since last watering
    PlantType plantType;
    GrowthStage growthStage;
    
    // Constructor with defaults
    SensorData() : moisture(0), temperature(25), humidity(50), 
                   lightLevel(500), lastWatered(0), 
                   plantType(TOMATO), growthStage(VEGETATIVE) {}
};

// Action structure for pump control
struct Action {
    bool shouldWater;
    unsigned int waterDuration;  // milliseconds
    float waterAmount;           // ml (calculated)
    bool isFailsafe;            // emergency watering
    
    // Constructor with defaults
    Action() : shouldWater(false), waterDuration(0), 
               waterAmount(0), isFailsafe(false) {}
};

// Water amount constants
enum WaterAmount {
    NO_WATER = 0,
    LOW_WATER = 1,    // 50ml
    MEDIUM_WATER = 2, // 100ml
    HIGH_WATER = 3    // 200ml
};

class LocalMLEngine {
private:
    DecisionTree irrigationTree;
    LookupTable plantThresholds;
    AnomalyDetector sensorMonitor;
    
    // Plant configurations per sensor
    PlantType plantTypes[4];
    GrowthStage growthStages[4];
    unsigned long lastWateringTime[4];
    
    // Internal calculation methods
    float calculateFeatureScore(const SensorData& data);
    WaterAmount mapToWaterAmount(float prediction);
    unsigned int calculateWaterDuration(WaterAmount amount);
    
#ifdef DEBUG_ML
    void debugPrint(const char* message, float value = 0);
#endif

public:
    LocalMLEngine();
    
    // Initialization
    bool begin();
    void setPlantType(int sensorIndex, PlantType type);
    void setGrowthStage(int sensorIndex, GrowthStage stage);
    
    // Main inference methods
    float predictWaterNeed(const SensorData& data);
    bool detectAnomaly(const SensorData& data);
    Action getImmediateAction(int sensorIndex, const SensorData& data);
    
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