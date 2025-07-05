/* * DecisionTree.cpp
 *
 * This file implements the DecisionTree class for making predictions based on a decision tree model.
 * It supports adding nodes, traversing the tree, and making predictions based on feature scores.
 *
 * Author: iLyas Bakouch
 * GitHub: https://github.com/SmartThingsDIY
 * Version: 0.78.3
 * Date: 2025
 */
#include "DecisionTree.h"

DecisionTree::DecisionTree() : nodeCount(0), rootIndex(1)
{
    // Initialize nodes array
    for (int i = 0; i < MAX_TREE_NODES; i++)
    {
        nodes[i] = TreeNode();
    }
}

bool DecisionTree::begin()
{
    // Load default tree if no tree is loaded
    if (nodeCount == 0)
    {
        loadDefaultTree();
    }

    return nodeCount > 0;
}

float DecisionTree::predict(float featureScore)
{
    // Simple prediction based on feature score
    // This is a fallback when full feature array is not available
    float features[FEATURE_COUNT];
    features[FEATURE_MOISTURE] = featureScore;
    features[FEATURE_TEMPERATURE] = 0.5; // Normalized default
    features[FEATURE_HUMIDITY] = 0.5;
    features[FEATURE_LIGHT] = 0.5;
    features[FEATURE_TIME] = 0.5;
    features[FEATURE_PLANT_TYPE] = 0.0;   // Default to first plant type
    features[FEATURE_GROWTH_STAGE] = 0.4; // Default to vegetative stage

    return predict(features);
}

float DecisionTree::predict(float features[FEATURE_COUNT])
{
    if (nodeCount == 0 || rootIndex == 0)
    {
        return 0.0; // No tree loaded
    }

    return traverseTree(rootIndex, features);
}

float DecisionTree::traverseTree(uint8_t nodeIndex, float features[FEATURE_COUNT])
{
    if (nodeIndex == 0 || nodeIndex > nodeCount)
    {
        return 0.0; // Invalid node
    }

    TreeNode &node = nodes[nodeIndex];

    // Check if this is a leaf node
    if (node.leftChild == 0 && node.rightChild == 0)
    {
        return node.value;
    }

    // Internal node - make decision based on feature
    float featureValue = features[node.featureIndex];

    if (featureValue <= node.threshold)
    {
        return traverseTree(node.leftChild, features);
    }
    else
    {
        return traverseTree(node.rightChild, features);
    }
}

bool DecisionTree::addNode(uint8_t index, uint8_t featureIndex, float threshold,
                           uint8_t leftChild, uint8_t rightChild, float value)
{
    if (index == 0 || index >= MAX_TREE_NODES)
    {
        return false;
    }

    nodes[index].featureIndex = featureIndex;
    nodes[index].threshold = threshold;
    nodes[index].leftChild = leftChild;
    nodes[index].rightChild = rightChild;
    nodes[index].value = value;

    if (index > nodeCount)
    {
        nodeCount = index;
    }

    return true;
}

void DecisionTree::setRootIndex(uint8_t index)
{
    rootIndex = index;
}

uint8_t DecisionTree::getNodeCount() const
{
    return nodeCount;
}

uint8_t DecisionTree::getMaxDepth() const
{
    // Calculate maximum depth by traversing tree
    // This is a simplified version - real implementation would be recursive
    return MAX_TREE_DEPTH;
}

size_t DecisionTree::getMemoryUsage() const
{
    return nodeCount * sizeof(TreeNode);
}

void DecisionTree::loadDefaultTree()
{
    // Simple rule-based decision tree
    // This provides a baseline when no trained model is available

    // Root node: Check moisture level
    addNode(1, FEATURE_MOISTURE, 0.6, 2, 3, 0);

    // Left branch: Low moisture
    addNode(2, FEATURE_TEMPERATURE, 0.7, 4, 5, 0);

    // Right branch: High moisture
    addNode(3, FEATURE_TIME, 0.5, 6, 7, 0);

    // Leaf nodes with watering decisions
    addNode(4, 0, 0, 0, 0, 0.8); // Low moisture + high temp = high water
    addNode(5, 0, 0, 0, 0, 0.6); // Low moisture + low temp = medium water
    addNode(6, 0, 0, 0, 0, 0.3); // High moisture + recent watering = low water
    addNode(7, 0, 0, 0, 0, 0.0); // High moisture + old watering = no water

    nodeCount = 7;
    rootIndex = 1;
}

void DecisionTree::compactTree()
{
    // Remove unused nodes to save memory
    // This is a placeholder for optimization
}

void DecisionTree::printTree() const
{
    Serial.println("Decision Tree Structure:");
    Serial.print("Nodes: ");
    Serial.println(nodeCount);
    Serial.print("Root: ");
    Serial.println(rootIndex);
    Serial.print("Memory: ");
    Serial.print(getMemoryUsage());
    Serial.println(" bytes");

    if (nodeCount > 0)
    {
        printNode(rootIndex, 0);
    }
}

void DecisionTree::printNode(uint8_t index, int depth) const
{
    if (index == 0 || index > nodeCount)
    {
        return;
    }

    const TreeNode &node = nodes[index];

    // Print indentation
    for (int i = 0; i < depth; i++)
    {
        Serial.print("  ");
    }

    Serial.print("Node ");
    Serial.print(index);
    Serial.print(": ");

    if (node.leftChild == 0 && node.rightChild == 0)
    {
        // Leaf node
        Serial.print("Leaf value = ");
        Serial.println(node.value);
    }
    else
    {
        // Internal node
        Serial.print("Feature ");
        Serial.print(node.featureIndex);
        Serial.print(" <= ");
        Serial.print(node.threshold);
        Serial.print(" ? Node ");
        Serial.print(node.leftChild);
        Serial.print(" : Node ");
        Serial.println(node.rightChild);

        // Recursively print children
        if (depth < MAX_TREE_DEPTH)
        {
            printNode(node.leftChild, depth + 1);
            printNode(node.rightChild, depth + 1);
        }
    }
}