#ifndef IRRIGATION_MODELS_H
#define IRRIGATION_MODELS_H

#include <stdint.h>

// Model version and metadata
#define MODEL_VERSION "1.0.0"
#define TRAINED_DATE "2025-01-01"
#define MIN_ACCURACY 0.85f
#define VALIDATION_SAMPLES 1000

// LSTM Moisture Prediction Model (for 24-hour forecasting)
// This is a simplified but functional TensorFlow Lite model
// Trained on synthetic irrigation data with seasonal patterns
extern const unsigned char moisture_lstm_model[];
extern const int moisture_lstm_model_len;

// Autoencoder Anomaly Detection Model
// Detects sensor malfunctions and unusual readings
extern const unsigned char anomaly_autoencoder_model[];
extern const int anomaly_autoencoder_model_len;

// Model validation data for sanity checking
struct ModelValidation {
    float minOutput;
    float maxOutput;
    float expectedMean;
    float expectedStdDev;
    uint16_t inputSize;
    uint16_t outputSize;
};

extern const ModelValidation lstm_validation;
extern const ModelValidation autoencoder_validation;

// Fallback decision tree for when ML models fail
struct DecisionNode {
    float threshold;
    uint8_t featureIndex;
    uint8_t leftChild;
    uint8_t rightChild;
    float prediction;
    bool isLeaf;
};

extern const DecisionNode fallback_tree[];
extern const uint8_t fallback_tree_size;

#endif // IRRIGATION_MODELS_H