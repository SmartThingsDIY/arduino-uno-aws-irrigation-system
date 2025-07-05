#ifndef EDGE_INFERENCE_H
#define EDGE_INFERENCE_H

#include <Arduino.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "DataBuffer.h"
#include "../models/irrigation_models.h"

// Model size constraints for ESP32
#define MAX_MODEL_SIZE 100000  // 100KB max per model
#define TENSOR_ARENA_SIZE 60000 // 60KB tensor arena

// Prediction result structure
struct PredictionResult {
    float moistureForecast[24];  // 24-hour forecast
    float confidence;            // Prediction confidence (0-1)
    float anomalyScore;          // Anomaly detection score (0-1)
    unsigned long timestamp;     // Prediction timestamp
    bool isValid;               // Prediction validity
    
    PredictionResult() : confidence(0), anomalyScore(0), timestamp(0), isValid(false) {
        for (int i = 0; i < 24; i++) {
            moistureForecast[i] = 0;
        }
    }
};

// Model types
enum ModelType {
    MODEL_MOISTURE_LSTM = 0,
    MODEL_ANOMALY_AUTOENCODER = 1,
    MODEL_WEATHER_PATTERN = 2,
    MODEL_COUNT = 3
};

// Model status
enum ModelStatus {
    MODEL_NOT_LOADED = 0,
    MODEL_LOADED = 1,
    MODEL_ERROR = 2
};

class EdgeInference {
private:
    // TensorFlow Lite components
    tflite::MicroErrorReporter* errorReporter;
    tflite::AllOpsResolver* resolver;
    
    // Model interpreters (one per model type)
    tflite::MicroInterpreter* interpreters[MODEL_COUNT];
    ModelStatus modelStatus[MODEL_COUNT];
    
    // Model storage
    const unsigned char* modelData[MODEL_COUNT];
    size_t modelSizes[MODEL_COUNT];
    
    // Tensor arenas (separate for each model to avoid conflicts)
    uint8_t* tensorArenas[MODEL_COUNT];
    
    // Data management
    DataBuffer* sensorBuffer;
    
    // Internal methods
    bool loadModelFromMemory(ModelType type, const unsigned char* modelData, size_t modelSize);
    bool prepareInputTensor(ModelType type, float* inputData, size_t inputSize);
    bool runInference(ModelType type);
    bool extractOutputTensor(ModelType type, float* outputData, size_t outputSize);
    
    // Feature engineering
    void prepareFeatures(float* features, size_t windowSize);
    void normalizeFeatures(float* features, size_t count);
    float calculateConfidence(float* output, size_t outputSize);
    
    // Memory management
    void freeModel(ModelType type);
    size_t getAvailableMemory();
    
public:
    EdgeInference();
    ~EdgeInference();
    
    // Initialization
    bool begin();
    bool loadModel(ModelType type, const unsigned char* modelData, size_t modelSize);
    bool loadModelFromFile(ModelType type, const char* filename);
    
    // Main prediction methods
    PredictionResult predict24Hours();
    float detectAnomaly(const SensorData& currentData);
    float predictWeatherPattern();
    
    // Data management
    bool addSensorData(const SensorData& data);
    void clearSensorHistory();
    
    // Model management
    bool isModelLoaded(ModelType type);
    ModelStatus getModelStatus(ModelType type);
    void unloadModel(ModelType type);
    void unloadAllModels();
    
    // Performance monitoring
    unsigned long getLastInferenceTime(ModelType type);
    float getModelAccuracy(ModelType type);
    size_t getModelMemoryUsage(ModelType type);
    
    // Configuration
    void setConfidenceThreshold(float threshold);
    void setAnomalyThreshold(float threshold);
    void enableDebugOutput(bool enable);
    
    // Utilities
    bool hasEnoughData();
    size_t getTotalMemoryUsage();
    void printModelInfo();
    void printDebugInfo();
    
    // Production safety methods
    bool validatePrediction(const PredictionResult& result);
    PredictionResult getFallbackPrediction(const SensorData& currentData);
    bool isReasonablePrediction(float value, ModelType type);
    void sanitizePrediction(PredictionResult& result);
    bool performModelSanityCheck(ModelType type);
};

#endif // EDGE_INFERENCE_H