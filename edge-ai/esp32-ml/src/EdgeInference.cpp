#include "EdgeInference.h"
#include "../models/irrigation_models.h"

// Global configuration
static float confidenceThreshold = 0.7;
static float anomalyThreshold = 0.8;
static bool debugOutput = false;

EdgeInference::EdgeInference() : errorReporter(nullptr), resolver(nullptr), sensorBuffer(nullptr) {
    // Initialize arrays
    for (int i = 0; i < MODEL_COUNT; i++) {
        interpreters[i] = nullptr;
        modelStatus[i] = MODEL_NOT_LOADED;
        modelData[i] = nullptr;
        modelSizes[i] = 0;
        tensorArenas[i] = nullptr;
    }
}

EdgeInference::~EdgeInference() {
    // Cleanup
    unloadAllModels();
    
    if (sensorBuffer != nullptr) {
        delete sensorBuffer;
    }
    
    if (resolver != nullptr) {
        delete resolver;
    }
    
    if (errorReporter != nullptr) {
        delete errorReporter;
    }
}

bool EdgeInference::begin() {
    Serial.println("Initializing EdgeInference...");
    
    // Initialize TensorFlow Lite components
    errorReporter = new tflite::MicroErrorReporter();
    if (errorReporter == nullptr) {
        Serial.println("Error: Failed to create error reporter");
        return false;
    }
    
    resolver = new tflite::AllOpsResolver();
    if (resolver == nullptr) {
        Serial.println("Error: Failed to create ops resolver");
        return false;
    }
    
    // Initialize sensor data buffer
    sensorBuffer = new DataBuffer(MAX_SENSOR_HISTORY);
    if (sensorBuffer == nullptr) {
        Serial.println("Error: Failed to create sensor buffer");
        return false;
    }
    
    if (!sensorBuffer->begin()) {
        Serial.println("Error: Failed to initialize sensor buffer");
        return false;
    }
    
    Serial.print("EdgeInference initialized. Available memory: ");
    Serial.print(getAvailableMemory());
    Serial.println(" bytes");
    
    // Load default models
    bool modelsLoaded = true;
    
    Serial.println("Loading LSTM moisture prediction model...");
    if (!loadModel(MODEL_MOISTURE_LSTM, moisture_lstm_model, moisture_lstm_model_len)) {
        Serial.println("Warning: Failed to load LSTM model, fallback mode will be used");
        modelsLoaded = false;
    } else {
        if (!performModelSanityCheck(MODEL_MOISTURE_LSTM)) {
            Serial.println("Warning: LSTM model failed sanity check");
            unloadModel(MODEL_MOISTURE_LSTM);
            modelsLoaded = false;
        }
    }
    
    Serial.println("Loading anomaly detection model...");
    if (!loadModel(MODEL_ANOMALY_AUTOENCODER, anomaly_autoencoder_model, anomaly_autoencoder_model_len)) {
        Serial.println("Warning: Failed to load anomaly model, basic detection will be used");
        modelsLoaded = false;
    } else {
        if (!performModelSanityCheck(MODEL_ANOMALY_AUTOENCODER)) {
            Serial.println("Warning: Anomaly model failed sanity check");
            unloadModel(MODEL_ANOMALY_AUTOENCODER);
            modelsLoaded = false;
        }
    }
    
    if (modelsLoaded) {
        Serial.println("All ML models loaded and validated successfully");
    } else {
        Serial.println("Some models failed to load - system will use fallback algorithms");
    }
    
    return true;
}

bool EdgeInference::loadModel(ModelType type, const unsigned char* modelData, size_t modelSize) {
    if (type >= MODEL_COUNT || modelData == nullptr || modelSize == 0) {
        Serial.println("Error: Invalid model parameters");
        return false;
    }
    
    // Check if model is already loaded
    if (modelStatus[type] == MODEL_LOADED) {
        Serial.println("Warning: Model already loaded, unloading first");
        unloadModel(type);
    }
    
    // Check memory constraints
    if (modelSize > MAX_MODEL_SIZE) {
        Serial.print("Error: Model too large (");
        Serial.print(modelSize);
        Serial.print(" > ");
        Serial.print(MAX_MODEL_SIZE);
        Serial.println(")");
        return false;
    }
    
    return loadModelFromMemory(type, modelData, modelSize);
}

bool EdgeInference::loadModelFromMemory(ModelType type, const unsigned char* data, size_t size) {
    // Store model data reference
    this->modelData[type] = data;
    this->modelSizes[type] = size;
    
    // Allocate tensor arena
    tensorArenas[type] = (uint8_t*)malloc(TENSOR_ARENA_SIZE);
    if (tensorArenas[type] == nullptr) {
        Serial.println("Error: Failed to allocate tensor arena");
        modelStatus[type] = MODEL_ERROR;
        return false;
    }
    
    // Parse the model
    const tflite::Model* model = tflite::GetModel(data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.print("Error: Model schema version mismatch (");
        Serial.print(model->version());
        Serial.print(" vs ");
        Serial.print(TFLITE_SCHEMA_VERSION);
        Serial.println(")");
        modelStatus[type] = MODEL_ERROR;
        return false;
    }
    
    // Create interpreter
    interpreters[type] = new tflite::MicroInterpreter(
        model, *resolver, tensorArenas[type], TENSOR_ARENA_SIZE, errorReporter
    );
    
    if (interpreters[type] == nullptr) {
        Serial.println("Error: Failed to create interpreter");
        modelStatus[type] = MODEL_ERROR;
        return false;
    }
    
    // Allocate tensors
    TfLiteStatus allocate_status = interpreters[type]->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        Serial.println("Error: Failed to allocate tensors");
        modelStatus[type] = MODEL_ERROR;
        return false;
    }
    
    modelStatus[type] = MODEL_LOADED;
    
    Serial.print("Model ");
    Serial.print(type);
    Serial.print(" loaded successfully (");
    Serial.print(size);
    Serial.println(" bytes)");
    
    return true;
}

PredictionResult EdgeInference::predict24Hours() {
    PredictionResult result;
    
    // Check if ML model is available and working
    if (!isModelLoaded(MODEL_MOISTURE_LSTM)) {
        Serial.println("Warning: LSTM model not available, using fallback prediction");
        SensorData currentData;
        if (sensorBuffer->getLatestData(&currentData)) {
            return getFallbackPrediction(currentData);
        }
        return result; // Return empty result if no data
    }
    
    if (!sensorBuffer->hasMinimumData(168)) { // Need 7 days of data
        Serial.println("Warning: Insufficient data for ML prediction, using simplified fallback");
        SensorData currentData;
        if (sensorBuffer->getLatestData(&currentData)) {
            return getFallbackPrediction(currentData);
        }
        return result;
    }
    
    unsigned long startTime = millis();
    
    // Extract features for LSTM input (168 hours * 7 features)
    float* features = (float*)malloc(168 * FEATURES_PER_SAMPLE * sizeof(float));
    if (features == nullptr) {
        Serial.println("Error: Failed to allocate features array");
        return result;
    }
    
    if (!sensorBuffer->extractFeatures(features, 168, true)) {
        Serial.println("Error: Failed to extract features");
        free(features);
        return result;
    }
    
    // Prepare input tensor
    if (!prepareInputTensor(MODEL_MOISTURE_LSTM, features, 168 * FEATURES_PER_SAMPLE)) {
        Serial.println("Error: Failed to prepare input tensor");
        free(features);
        return result;
    }
    
    // Run inference
    if (!runInference(MODEL_MOISTURE_LSTM)) {
        Serial.println("Error: LSTM inference failed");
        free(features);
        return result;
    }
    
    // Extract output (24-hour forecast)
    if (!extractOutputTensor(MODEL_MOISTURE_LSTM, result.moistureForecast, 24)) {
        Serial.println("Error: Failed to extract output tensor");
        free(features);
        return result;
    }
    
    // Calculate confidence
    result.confidence = calculateConfidence(result.moistureForecast, 24);
    result.timestamp = millis();
    
    // Validate and sanitize prediction
    if (validatePrediction(result)) {
        sanitizePrediction(result);
        result.isValid = (result.confidence > confidenceThreshold);
    } else {
        Serial.println("Warning: ML prediction failed validation, using fallback");
        free(features);
        SensorData currentData;
        if (sensorBuffer->getLatestData(&currentData)) {
            return getFallbackPrediction(currentData);
        }
        return result; // Return empty result
    }
    
    free(features);
    
    unsigned long inferenceTime = millis() - startTime;
    
    if (debugOutput) {
        Serial.print("24-hour prediction completed in ");
        Serial.print(inferenceTime);
        Serial.print("ms, confidence: ");
        Serial.println(result.confidence);
    }
    
    return result;
}

float EdgeInference::detectAnomaly(const SensorData& currentData) {
    if (!isModelLoaded(MODEL_ANOMALY_AUTOENCODER)) {
        Serial.println("Warning: Anomaly detection model not loaded");
        return 0.0;
    }
    
    if (!sensorBuffer->hasMinimumData(24)) { // Need 24 hours of context
        return 0.0;
    }
    
    // Extract recent window for anomaly detection
    float* window = (float*)malloc(24 * FEATURES_PER_SAMPLE * sizeof(float));
    if (window == nullptr) {
        Serial.println("Error: Failed to allocate window array");
        return 0.0;
    }
    
    if (!sensorBuffer->extractFeatures(window, 24, true)) {
        Serial.println("Error: Failed to extract features for anomaly detection");
        free(window);
        return 0.0;
    }
    
    // Prepare input tensor
    if (!prepareInputTensor(MODEL_ANOMALY_AUTOENCODER, window, 24 * FEATURES_PER_SAMPLE)) {
        free(window);
        return 0.0;
    }
    
    // Run inference
    if (!runInference(MODEL_ANOMALY_AUTOENCODER)) {
        free(window);
        return 0.0;
    }
    
    // Extract reconstruction
    float* reconstruction = (float*)malloc(24 * FEATURES_PER_SAMPLE * sizeof(float));
    if (reconstruction == nullptr) {
        free(window);
        return 0.0;
    }
    
    if (!extractOutputTensor(MODEL_ANOMALY_AUTOENCODER, reconstruction, 24 * FEATURES_PER_SAMPLE)) {
        free(window);
        free(reconstruction);
        return 0.0;
    }
    
    // Calculate reconstruction error (anomaly score)
    float totalError = 0.0;
    for (int i = 0; i < 24 * FEATURES_PER_SAMPLE; i++) {
        float error = window[i] - reconstruction[i];
        totalError += error * error;
    }
    
    float anomalyScore = totalError / (24 * FEATURES_PER_SAMPLE);
    
    // Normalize to 0-1 range (this would typically use training statistics)
    anomalyScore = constrain(anomalyScore, 0.0, 1.0);
    
    free(window);
    free(reconstruction);
    
    if (debugOutput && anomalyScore > anomalyThreshold) {
        Serial.print("Anomaly detected! Score: ");
        Serial.println(anomalyScore);
    }
    
    return anomalyScore;
}

bool EdgeInference::prepareInputTensor(ModelType type, float* inputData, size_t inputSize) {
    if (!isModelLoaded(type) || inputData == nullptr) {
        return false;
    }
    
    TfLiteTensor* input = interpreters[type]->input(0);
    if (input == nullptr) {
        Serial.println("Error: Failed to get input tensor");
        return false;
    }
    
    // Check input size
    size_t expectedSize = 1;
    for (int i = 0; i < input->dims->size; i++) {
        expectedSize *= input->dims->data[i];
    }
    
    if (inputSize != expectedSize) {
        Serial.print("Error: Input size mismatch (");
        Serial.print(inputSize);
        Serial.print(" vs ");
        Serial.print(expectedSize);
        Serial.println(")");
        return false;
    }
    
    // Copy data to input tensor
    if (input->type == kTfLiteFloat32) {
        memcpy(input->data.f, inputData, inputSize * sizeof(float));
    } else if (input->type == kTfLiteInt8) {
        // Quantize float32 to int8
        for (size_t i = 0; i < inputSize; i++) {
            input->data.int8[i] = (int8_t)(inputData[i] * 127.0f);
        }
    } else {
        Serial.println("Error: Unsupported input tensor type");
        return false;
    }
    
    return true;
}

bool EdgeInference::runInference(ModelType type) {
    if (!isModelLoaded(type)) {
        return false;
    }
    
    TfLiteStatus invoke_status = interpreters[type]->Invoke();
    if (invoke_status != kTfLiteOk) {
        Serial.print("Error: Inference failed for model ");
        Serial.println(type);
        return false;
    }
    
    return true;
}

bool EdgeInference::extractOutputTensor(ModelType type, float* outputData, size_t outputSize) {
    if (!isModelLoaded(type) || outputData == nullptr) {
        return false;
    }
    
    TfLiteTensor* output = interpreters[type]->output(0);
    if (output == nullptr) {
        Serial.println("Error: Failed to get output tensor");
        return false;
    }
    
    // Check output size
    size_t actualSize = 1;
    for (int i = 0; i < output->dims->size; i++) {
        actualSize *= output->dims->data[i];
    }
    
    if (outputSize != actualSize) {
        Serial.print("Error: Output size mismatch (");
        Serial.print(outputSize);
        Serial.print(" vs ");
        Serial.print(actualSize);
        Serial.println(")");
        return false;
    }
    
    // Copy data from output tensor
    if (output->type == kTfLiteFloat32) {
        memcpy(outputData, output->data.f, outputSize * sizeof(float));
    } else if (output->type == kTfLiteInt8) {
        // Dequantize int8 to float32
        for (size_t i = 0; i < outputSize; i++) {
            outputData[i] = (float)output->data.int8[i] / 127.0f;
        }
    } else {
        Serial.println("Error: Unsupported output tensor type");
        return false;
    }
    
    return true;
}

float EdgeInference::calculateConfidence(float* output, size_t outputSize) {
    if (output == nullptr || outputSize == 0) {
        return 0.0;
    }
    
    // Simple confidence calculation based on output variance
    float mean = 0.0;
    for (size_t i = 0; i < outputSize; i++) {
        mean += output[i];
    }
    mean /= outputSize;
    
    float variance = 0.0;
    for (size_t i = 0; i < outputSize; i++) {
        float diff = output[i] - mean;
        variance += diff * diff;
    }
    variance /= outputSize;
    
    // Convert variance to confidence (lower variance = higher confidence)
    float confidence = 1.0 / (1.0 + variance);
    return constrain(confidence, 0.0, 1.0);
}

bool EdgeInference::addSensorData(const SensorData& data) {
    if (sensorBuffer == nullptr) {
        return false;
    }
    
    return sensorBuffer->addSample(data);
}

bool EdgeInference::isModelLoaded(ModelType type) {
    return (type < MODEL_COUNT && modelStatus[type] == MODEL_LOADED);
}

ModelStatus EdgeInference::getModelStatus(ModelType type) {
    if (type >= MODEL_COUNT) {
        return MODEL_ERROR;
    }
    return modelStatus[type];
}

void EdgeInference::unloadModel(ModelType type) {
    if (type >= MODEL_COUNT) {
        return;
    }
    
    if (interpreters[type] != nullptr) {
        delete interpreters[type];
        interpreters[type] = nullptr;
    }
    
    if (tensorArenas[type] != nullptr) {
        free(tensorArenas[type]);
        tensorArenas[type] = nullptr;
    }
    
    modelData[type] = nullptr;
    modelSizes[type] = 0;
    modelStatus[type] = MODEL_NOT_LOADED;
    
    Serial.print("Model ");
    Serial.print(type);
    Serial.println(" unloaded");
}

void EdgeInference::unloadAllModels() {
    for (int i = 0; i < MODEL_COUNT; i++) {
        unloadModel((ModelType)i);
    }
}

size_t EdgeInference::getAvailableMemory() {
    return ESP.getFreeHeap();
}

size_t EdgeInference::getTotalMemoryUsage() {
    size_t total = 0;
    
    for (int i = 0; i < MODEL_COUNT; i++) {
        if (modelStatus[i] == MODEL_LOADED) {
            total += TENSOR_ARENA_SIZE; // Tensor arena
            total += modelSizes[i];     // Model data
        }
    }
    
    if (sensorBuffer != nullptr) {
        total += sensorBuffer->getMemoryUsage();
    }
    
    return total;
}

void EdgeInference::setConfidenceThreshold(float threshold) {
    confidenceThreshold = constrain(threshold, 0.0, 1.0);
}

void EdgeInference::setAnomalyThreshold(float threshold) {
    anomalyThreshold = constrain(threshold, 0.0, 1.0);
}

void EdgeInference::enableDebugOutput(bool enable) {
    debugOutput = enable;
}

bool EdgeInference::hasEnoughData() {
    return sensorBuffer != nullptr && sensorBuffer->hasMinimumData(24);
}

void EdgeInference::printModelInfo() {
    Serial.println("=== EdgeInference Model Status ===");
    
    const char* modelNames[] = {"LSTM Moisture", "Anomaly Detector", "Weather Pattern"};
    const char* statusNames[] = {"Not Loaded", "Loaded", "Error"};
    
    for (int i = 0; i < MODEL_COUNT; i++) {
        Serial.print(modelNames[i]);
        Serial.print(": ");
        Serial.print(statusNames[modelStatus[i]]);
        
        if (modelStatus[i] == MODEL_LOADED) {
            Serial.print(" (");
            Serial.print(modelSizes[i]);
            Serial.print(" bytes)");
        }
        
        Serial.println();
    }
    
    Serial.print("Total memory usage: ");
    Serial.print(getTotalMemoryUsage());
    Serial.println(" bytes");
    
    Serial.print("Available memory: ");
    Serial.print(getAvailableMemory());
    Serial.println(" bytes");
}

void EdgeInference::printDebugInfo() {
    printModelInfo();
    
    if (sensorBuffer != nullptr) {
        Serial.print("Sensor buffer size: ");
        Serial.print(sensorBuffer->getSize());
        Serial.print(" / ");
        Serial.println(sensorBuffer->getCapacity());
        
        if (debugOutput) {
            sensorBuffer->printStatistics();
        }
    }
}

// Production safety methods implementation
bool EdgeInference::validatePrediction(const PredictionResult& result) {
    // Check if all values are reasonable for moisture predictions
    for (int i = 0; i < 24; i++) {
        if (!isReasonablePrediction(result.moistureForecast[i], MODEL_MOISTURE_LSTM)) {
            Serial.print("Unreasonable prediction at hour ");
            Serial.print(i);
            Serial.print(": ");
            Serial.println(result.moistureForecast[i]);
            return false;
        }
    }
    
    // Check confidence bounds
    if (result.confidence < 0.0f || result.confidence > 1.0f) {
        Serial.print("Invalid confidence: ");
        Serial.println(result.confidence);
        return false;
    }
    
    // Check for NaN or infinite values
    for (int i = 0; i < 24; i++) {
        if (isnan(result.moistureForecast[i]) || isinf(result.moistureForecast[i])) {
            Serial.println("NaN or infinite value in prediction");
            return false;
        }
    }
    
    return true;
}

PredictionResult EdgeInference::getFallbackPrediction(const SensorData& currentData) {
    PredictionResult result;
    
    Serial.println("Using fallback decision tree for prediction");
    
    // Use simple decision tree when ML models fail
    float features[7] = {
        currentData.moisture,
        currentData.temperature,
        currentData.humidity,
        currentData.lightLevel,
        currentData.soilTemperature,
        currentData.windSpeed,
        currentData.pressure
    };
    
    // Navigate decision tree
    uint8_t nodeIndex = 0;
    while (!fallback_tree[nodeIndex].isLeaf && nodeIndex < fallback_tree_size) {
        float feature = features[fallback_tree[nodeIndex].featureIndex];
        if (feature < fallback_tree[nodeIndex].threshold) {
            nodeIndex = fallback_tree[nodeIndex].leftChild;
        } else {
            nodeIndex = fallback_tree[nodeIndex].rightChild;
        }
    }
    
    // Get prediction from leaf node
    float baseValue = fallback_tree[nodeIndex].prediction;
    
    // Create simple 24-hour forecast with gradual change
    for (int i = 0; i < 24; i++) {
        // Simple linear interpolation with seasonal variation
        float hourFactor = 1.0f + 0.1f * sin(2.0f * PI * i / 24.0f); // Daily variation
        result.moistureForecast[i] = constrain(baseValue * hourFactor, 0.0f, 100.0f);
    }
    
    result.confidence = 0.6f; // Lower confidence for fallback
    result.timestamp = millis();
    result.isValid = true;
    result.anomalyScore = 0.0f;
    
    Serial.print("Fallback prediction: ");
    Serial.print(baseValue);
    Serial.println("% base moisture level");
    
    return result;
}

bool EdgeInference::isReasonablePrediction(float value, ModelType type) {
    switch (type) {
        case MODEL_MOISTURE_LSTM:
            return (value >= lstm_validation.minOutput && 
                    value <= lstm_validation.maxOutput);
                    
        case MODEL_ANOMALY_AUTOENCODER:
            return (value >= autoencoder_validation.minOutput && 
                    value <= autoencoder_validation.maxOutput);
                    
        default:
            return (value >= 0.0f && value <= 100.0f); // Generic bounds
    }
}

void EdgeInference::sanitizePrediction(PredictionResult& result) {
    // Clamp values to safe ranges
    for (int i = 0; i < 24; i++) {
        result.moistureForecast[i] = constrain(result.moistureForecast[i], 
                                             lstm_validation.minOutput, 
                                             lstm_validation.maxOutput);
    }
    
    // Ensure confidence is within bounds
    result.confidence = constrain(result.confidence, 0.0f, 1.0f);
    
    // Ensure anomaly score is within bounds
    result.anomalyScore = constrain(result.anomalyScore, 0.0f, 1.0f);
    
    // Apply smoothing to remove extreme jumps
    for (int i = 1; i < 24; i++) {
        float diff = abs(result.moistureForecast[i] - result.moistureForecast[i-1]);
        if (diff > 20.0f) { // Max 20% change per hour
            result.moistureForecast[i] = result.moistureForecast[i-1] + 
                                       (result.moistureForecast[i] > result.moistureForecast[i-1] ? 20.0f : -20.0f);
        }
    }
}

bool EdgeInference::performModelSanityCheck(ModelType type) {
    if (!isModelLoaded(type)) {
        return false;
    }
    
    Serial.print("Performing sanity check for model ");
    Serial.println(type);
    
    // Create test input with known values
    float testInput[168 * FEATURES_PER_SAMPLE];
    for (int i = 0; i < 168 * FEATURES_PER_SAMPLE; i++) {
        testInput[i] = 0.5f; // Neutral values
    }
    
    // Prepare input tensor
    if (!prepareInputTensor(type, testInput, 168 * FEATURES_PER_SAMPLE)) {
        Serial.println("Sanity check failed: Cannot prepare input");
        return false;
    }
    
    // Run inference
    TfLiteStatus status = interpreters[type]->Invoke();
    if (status != kTfLiteOk) {
        Serial.println("Sanity check failed: Inference error");
        return false;
    }
    
    // Get output tensor
    TfLiteTensor* output = interpreters[type]->output(0);
    if (output == nullptr) {
        Serial.println("Sanity check failed: No output tensor");
        return false;
    }
    
    // Check output reasonableness
    const ModelValidation* validation = (type == MODEL_MOISTURE_LSTM) ? 
                                       &lstm_validation : &autoencoder_validation;
    
    for (int i = 0; i < validation->outputSize; i++) {
        float value = output->data.f[i];
        if (isnan(value) || isinf(value) || 
            value < validation->minOutput || value > validation->maxOutput) {
            Serial.print("Sanity check failed: Invalid output at index ");
            Serial.print(i);
            Serial.print(": ");
            Serial.println(value);
            return false;
        }
    }
    
    Serial.println("Model sanity check passed");
    return true;
}