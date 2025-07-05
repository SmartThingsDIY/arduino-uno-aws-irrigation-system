#ifndef DECISION_TREE_H
#define DECISION_TREE_H

#include <Arduino.h>
#include "PlantTypes.h"

// Maximum tree depth to fit in Arduino memory
#define MAX_TREE_DEPTH 8
#define MAX_TREE_NODES 127  // 2^7 - 1 (fits in 8-bit index)

// Feature indices for decision tree
enum FeatureIndex {
    FEATURE_MOISTURE = 0,
    FEATURE_TEMPERATURE = 1,
    FEATURE_HUMIDITY = 2,
    FEATURE_LIGHT = 3,
    FEATURE_TIME = 4,
    FEATURE_PLANT_TYPE = 5,
    FEATURE_GROWTH_STAGE = 6,
    FEATURE_COUNT = 7
};

// Tree node structure (optimized for memory)
struct TreeNode {
    uint8_t featureIndex;    // Which feature to split on
    float threshold;         // Split threshold
    uint8_t leftChild;       // Index of left child (0 = no child)
    uint8_t rightChild;      // Index of right child (0 = no child)
    float value;             // Leaf value (for leaf nodes)
    
    // Constructor
    TreeNode() : featureIndex(0), threshold(0), leftChild(0), rightChild(0), value(0) {}
};

class DecisionTree {
private:
    TreeNode nodes[MAX_TREE_NODES];
    uint8_t nodeCount;
    uint8_t rootIndex;
    
    // Internal traversal method
    float traverseTree(uint8_t nodeIndex, float features[FEATURE_COUNT]);
    
    // Memory optimization
    void compactTree();
    
public:
    DecisionTree();
    
    // Initialization
    bool begin();
    
    // Prediction method
    float predict(float featureScore);
    float predict(float features[FEATURE_COUNT]);
    
    // Tree construction (for model loading)
    bool addNode(uint8_t index, uint8_t featureIndex, float threshold, 
                 uint8_t leftChild, uint8_t rightChild, float value);
    void setRootIndex(uint8_t index);
    
    // Tree information
    uint8_t getNodeCount() const;
    uint8_t getMaxDepth() const;
    size_t getMemoryUsage() const;
    
    // Default tree (simple rule-based fallback)
    void loadDefaultTree();
    
    // Debugging
    void printTree() const;
    void printNode(uint8_t index, int depth = 0) const;
};

#endif // DECISION_TREE_H