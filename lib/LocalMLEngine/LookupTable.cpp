/* * LookupTable.cpp
 *
 * This file implements the LookupTable class for managing plant characteristics and thresholds.
 * It provides methods to retrieve plant data, apply growth stage modifiers, and manage custom thresholds.
 *
 * Author: iLyas Bakouch
 * GitHub: https://github.com/SmartThingsDIY
 * Version: 0.78.3
 * Date: 2025
 */
#include "LocalMLEngine.h"
#include "LookupTable.h"

// Plant database stored in program memory to save RAM
const PlantCharacteristics LookupTable::PLANT_DATABASE[20] PROGMEM = {
    // Vegetables
    {"Tomato", 400, 24, 60, 700, 150, 0.8, 1.0, 1.2, 1.3, 1.0},
    {"Lettuce", 350, 18, 70, 500, 100, 0.9, 1.0, 1.1, 0.8, 0.7},
    {"Basil", 380, 22, 65, 600, 120, 0.8, 1.0, 1.2, 1.1, 0.9},
    {"Mint", 300, 20, 75, 450, 130, 0.9, 1.0, 1.1, 1.0, 0.8},
    {"Pepper", 420, 26, 55, 750, 140, 0.8, 1.0, 1.3, 1.4, 1.1},

    // Flowers
    {"Rose", 450, 22, 60, 650, 160, 0.7, 1.0, 1.4, 1.2, 1.0},
    {"Sunflower", 500, 25, 50, 800, 200, 0.8, 1.0, 1.5, 1.3, 1.1},
    {"Marigold", 400, 21, 55, 600, 110, 0.9, 1.0, 1.2, 1.1, 0.8},
    {"Petunia", 350, 20, 65, 550, 105, 0.8, 1.0, 1.3, 1.1, 0.9},
    {"Daisy", 370, 19, 60, 500, 95, 0.9, 1.0, 1.1, 1.0, 0.8},

    // Fruits
    {"Strawberry", 380, 20, 70, 550, 125, 0.8, 1.0, 1.2, 1.4, 1.2},
    {"Blueberry", 400, 22, 65, 600, 140, 0.7, 1.0, 1.3, 1.5, 1.3},
    {"Raspberry", 390, 21, 68, 580, 135, 0.8, 1.0, 1.2, 1.4, 1.2},
    {"Grape", 450, 24, 60, 700, 180, 0.6, 1.0, 1.4, 1.6, 1.4},

    // Specialty plants
    {"Cactus", 800, 28, 30, 900, 30, 0.5, 1.0, 1.1, 1.0, 0.9},
    {"Succulent", 750, 26, 35, 850, 35, 0.6, 1.0, 1.0, 0.9, 0.8},
    {"Fern", 250, 18, 85, 300, 90, 1.0, 1.0, 1.0, 0.9, 0.8},
    {"Orchid", 300, 23, 80, 400, 80, 0.9, 1.0, 1.2, 1.1, 1.0},
    {"Bamboo", 350, 22, 70, 550, 150, 0.8, 1.0, 1.1, 1.0, 0.9},
    {"Lavender", 500, 25, 45, 750, 100, 0.7, 1.0, 1.3, 1.2, 1.0}};

LookupTable::LookupTable()
{
    // Initialize custom thresholds
    for (int i = 0; i < 20; i++)
    {
        hasCustomThresholds[i] = false;
        for (int j = 0; j < 3; j++)
        {
            customThresholds[i][j] = 0;
        }
    }
}

bool LookupTable::begin()
{
    // No initialization required for lookup table
    return true;
}

float LookupTable::getMoistureThreshold(PlantType type, GrowthStage stage)
{
    if (!isValidPlantType(type))
    {
        return 400; // Default threshold
    }

    float baseThreshold;

    // Use custom threshold if available
    if (hasCustomThresholds[type])
    {
        baseThreshold = customThresholds[type][0];
    }
    else
    {
        // Read from program memory
        baseThreshold = pgm_read_float(&PLANT_DATABASE[type].moistureThreshold);
    }

    // Apply growth stage modifier
    float modifier = getStageModifier(type, stage);
    return baseThreshold * modifier;
}

float LookupTable::getTemperatureOptimal(PlantType type)
{
    if (!isValidPlantType(type))
    {
        return 22; // Default temperature
    }

    if (hasCustomThresholds[type])
    {
        return customThresholds[type][1];
    }
    else
    {
        return pgm_read_float(&PLANT_DATABASE[type].temperatureOptimal);
    }
}

float LookupTable::getHumidityOptimal(PlantType type)
{
    if (!isValidPlantType(type))
    {
        return 60; // Default humidity
    }

    if (hasCustomThresholds[type])
    {
        return customThresholds[type][2];
    }
    else
    {
        return pgm_read_float(&PLANT_DATABASE[type].humidityOptimal);
    }
}

float LookupTable::getLightRequirement(PlantType type)
{
    if (!isValidPlantType(type))
    {
        return 500; // Default light requirement
    }

    return pgm_read_float(&PLANT_DATABASE[type].lightRequirement);
}

float LookupTable::getWaterAmount(PlantType type, GrowthStage stage)
{
    if (!isValidPlantType(type))
    {
        return 100; // Default water amount
    }

    float baseAmount = pgm_read_float(&PLANT_DATABASE[type].waterAmount);
    float modifier = getStageModifier(type, stage);

    return baseAmount * modifier;
}

const char *LookupTable::getPlantName(PlantType type)
{
    if (!isValidPlantType(type))
    {
        return "Unknown";
    }

    // Note: This reads from program memory, but the string is small
    return (const char *)pgm_read_ptr(&PLANT_DATABASE[type].name);
}

PlantCharacteristics LookupTable::getPlantCharacteristics(PlantType type)
{
    PlantCharacteristics characteristics;

    if (!isValidPlantType(type))
    {
        // Return default characteristics
        characteristics.name = "Unknown";
        characteristics.moistureThreshold = 400;
        characteristics.temperatureOptimal = 22;
        characteristics.humidityOptimal = 60;
        characteristics.lightRequirement = 500;
        characteristics.waterAmount = 100;
        characteristics.seedlingModifier = 0.8;
        characteristics.vegetativeModifier = 1.0;
        characteristics.floweringModifier = 1.2;
        characteristics.fruitingModifier = 1.3;
        characteristics.matureModifier = 1.0;
        return characteristics;
    }

    // Read from program memory
    memcpy_P(&characteristics, &PLANT_DATABASE[type], sizeof(PlantCharacteristics));

    // Apply custom thresholds if available
    if (hasCustomThresholds[type])
    {
        characteristics.moistureThreshold = customThresholds[type][0];
        characteristics.temperatureOptimal = customThresholds[type][1];
        characteristics.humidityOptimal = customThresholds[type][2];
    }

    return characteristics;
}

void LookupTable::updateThresholds(PlantType type, float moistureThreshold,
                                   float tempOptimal, float humidityOptimal)
{
    if (!isValidPlantType(type))
    {
        return;
    }

    customThresholds[type][0] = moistureThreshold;
    customThresholds[type][1] = tempOptimal;
    customThresholds[type][2] = humidityOptimal;
    hasCustomThresholds[type] = true;
}

void LookupTable::resetToDefaults(PlantType type)
{
    if (!isValidPlantType(type))
    {
        return;
    }

    hasCustomThresholds[type] = false;
}

void LookupTable::resetAllToDefaults()
{
    for (int i = 0; i < 20; i++)
    {
        hasCustomThresholds[i] = false;
    }
}

float LookupTable::getStageModifier(PlantType type, GrowthStage stage)
{
    if (!isValidPlantType(type) || !isValidGrowthStage(stage))
    {
        return 1.0; // Default modifier
    }

    float modifier;

    switch (stage)
    {
    case SEEDLING:
        modifier = pgm_read_float(&PLANT_DATABASE[type].seedlingModifier);
        break;
    case VEGETATIVE:
        modifier = pgm_read_float(&PLANT_DATABASE[type].vegetativeModifier);
        break;
    case FLOWERING:
        modifier = pgm_read_float(&PLANT_DATABASE[type].floweringModifier);
        break;
    case FRUITING:
        modifier = pgm_read_float(&PLANT_DATABASE[type].fruitingModifier);
        break;
    case MATURE:
        modifier = pgm_read_float(&PLANT_DATABASE[type].matureModifier);
        break;
    default:
        modifier = 1.0;
    }

    return modifier;
}

bool LookupTable::isValidPlantType(PlantType type)
{
    return (type >= 0 && type < 20);
}

bool LookupTable::isValidGrowthStage(GrowthStage stage)
{
    return (stage >= 0 && stage < 5);
}

size_t LookupTable::getMemoryUsage() const
{
    size_t usage = sizeof(PLANT_DATABASE); // Program memory
    usage += sizeof(customThresholds);     // RAM for custom thresholds
    usage += sizeof(hasCustomThresholds);  // RAM for flags
    return usage;
}

void LookupTable::printPlantDatabase() const
{
    Serial.println("Plant Database:");
    Serial.println("Type\tName\t\tMoisture\tTemp\tHumidity\tLight\tWater");

    for (int i = 0; i < 20; i++)
    {
        PlantCharacteristics plant;
        memcpy_P(&plant, &PLANT_DATABASE[i], sizeof(PlantCharacteristics));

        Serial.print(i);
        Serial.print("\t");
        Serial.print(plant.name);
        Serial.print("\t\t");
        Serial.print(plant.moistureThreshold);
        Serial.print("\t\t");
        Serial.print(plant.temperatureOptimal);
        Serial.print("\t");
        Serial.print(plant.humidityOptimal);
        Serial.print("\t\t");
        Serial.print(plant.lightRequirement);
        Serial.print("\t");
        Serial.print(plant.waterAmount);

        if (hasCustomThresholds[i])
        {
            Serial.print(" (Custom)");
        }

        Serial.println();
    }

    Serial.print("Memory usage: ");
    Serial.print(getMemoryUsage());
    Serial.println(" bytes");
}