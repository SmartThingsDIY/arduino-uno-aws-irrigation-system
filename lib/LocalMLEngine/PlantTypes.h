#ifndef PLANT_TYPES_H
#define PLANT_TYPES_H

// =============================================================================
// SHARED PLANT TYPE DEFINITIONS
// =============================================================================
// This is the canonical definition of plant types and growth stages.
// All other headers should include this file instead of defining these enums.
//
// DO NOT DUPLICATE THESE DEFINITIONS - always include this header instead.
// =============================================================================

// Plant type constants - 5 supported plant types (memory optimized)
enum PlantType
{
    TOMATO = 0,
    LETTUCE,
    BASIL,
    GENERIC,  // Generic fallback plant
    SUCCULENT
};

// Growth stage constants - 5 growth stages
enum GrowthStage
{
    SEEDLING = 0,
    VEGETATIVE,
    FLOWERING,
    FRUITING,
    MATURE
};

// Water amount constants for irrigation control
enum WaterAmount
{
    NO_WATER = 0,
    LOW_WATER = 1,    // 50ml
    MEDIUM_WATER = 2, // 100ml
    HIGH_WATER = 3    // 200ml
};

#endif // PLANT_TYPES_H
