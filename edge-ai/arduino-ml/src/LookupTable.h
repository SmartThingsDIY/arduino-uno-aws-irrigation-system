#ifndef LOOKUP_TABLE_H
#define LOOKUP_TABLE_H

#include <Arduino.h>

// Plant type constants (must match LocalMLEngine.h)
enum PlantType {
    TOMATO = 0, LETTUCE, BASIL, MINT, PEPPER,
    ROSE, SUNFLOWER, MARIGOLD, PETUNIA, DAISY,
    STRAWBERRY, BLUEBERRY, RASPBERRY, GRAPE,
    CACTUS, SUCCULENT, FERN, ORCHID, BAMBOO, LAVENDER
};

// Growth stage constants (must match LocalMLEngine.h)
enum GrowthStage {
    SEEDLING = 0, VEGETATIVE, FLOWERING, FRUITING, MATURE
};

// Plant characteristics structure
struct PlantCharacteristics {
    const char* name;
    float moistureThreshold;    // Dry threshold (0-1023)
    float temperatureOptimal;   // Optimal temperature (°C)
    float humidityOptimal;      // Optimal humidity (%)
    float lightRequirement;     // Light requirement (0-1023)
    float waterAmount;          // Base water amount (ml)
    
    // Growth stage modifiers
    float seedlingModifier;     // Modifier for seedling stage
    float vegetativeModifier;   // Modifier for vegetative stage
    float floweringModifier;    // Modifier for flowering stage
    float fruitingModifier;     // Modifier for fruiting stage
    float matureModifier;       // Modifier for mature stage
};

class LookupTable {
private:
    // Plant database stored in program memory (PROGMEM)
    static const PlantCharacteristics PLANT_DATABASE[20] PROGMEM;
    
    // Runtime adjustable thresholds
    float customThresholds[20][3]; // [plant_type][moisture, temp, humidity]
    bool hasCustomThresholds[20];
    
    // Helper methods
    float getStageModifier(PlantType type, GrowthStage stage);
    
public:
    LookupTable();
    
    // Initialization
    bool begin();
    
    // Main lookup methods
    float getMoistureThreshold(PlantType type, GrowthStage stage);
    float getTemperatureOptimal(PlantType type);
    float getHumidityOptimal(PlantType type);
    float getLightRequirement(PlantType type);
    float getWaterAmount(PlantType type, GrowthStage stage);
    
    // Plant information
    const char* getPlantName(PlantType type);
    PlantCharacteristics getPlantCharacteristics(PlantType type);
    
    // Customization methods
    void updateThresholds(PlantType type, float moistureThreshold, 
                         float tempOptimal, float humidityOptimal);
    void resetToDefaults(PlantType type);
    void resetAllToDefaults();
    
    // Utility methods
    bool isValidPlantType(PlantType type);
    bool isValidGrowthStage(GrowthStage stage);
    
    // Statistics
    size_t getMemoryUsage() const;
    void printPlantDatabase() const;
};

#endif // LOOKUP_TABLE_H