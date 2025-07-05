#include "WiFiManager.h"
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <Update.h>

WiFiManager::WiFiManager() {
    webServer = nullptr;
    dnsServer = nullptr;
    mqttClient = nullptr;
    currentStatus = WIFI_DISCONNECTED;
    lastReconnectAttempt = 0;
    apModeStartTime = 0;
    connectionCallback = nullptr;
    messageCallback = nullptr;
    debugOutput = false;
    webInterfaceEnabled = false;
    otaEnabled = false;
}

WiFiManager::~WiFiManager() {
    end();
}

bool WiFiManager::begin() {
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("WiFiManager: SPIFFS mount failed");
        return false;
    }
    
    // Load configuration
    loadConfig();
    
    // Initialize MQTT client
    mqttClient = new PubSubClient(wifiClient);
    
    // Set up MQTT callback
    mqttClient->setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->mqttCallback(topic, payload, length);
    });
    
    if (debugOutput) {
        Serial.println("WiFiManager: Initialized");
    }
    
    return true;
}

void WiFiManager::loop() {
    // Handle WiFi connection
    if (WiFi.status() == WL_CONNECTED) {
        if (currentStatus != WIFI_CONNECTED) {
            currentStatus = WIFI_CONNECTED;
            if (connectionCallback) {
                connectionCallback(currentStatus);
            }
        }
        
        // Handle MQTT
        if (config.enableCloud && mqttClient) {
            if (!mqttClient->connected()) {
                reconnectMQTT();
            }
            mqttClient->loop();
        }
        
        // Handle web server
        if (webServer) {
            webServer->handleClient();
        }
    } else {
        if (currentStatus == WIFI_CONNECTED) {
            currentStatus = WIFI_DISCONNECTED;
            if (connectionCallback) {
                connectionCallback(currentStatus);
            }
        }
        
        // Try to reconnect
        if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL_MS) {
            if (strlen(config.ssid) > 0) {
                connectToWiFi();
            }
            lastReconnectAttempt = millis();
        }
    }
    
    // Handle DNS server in AP mode
    if (dnsServer) {
        dnsServer->processNextRequest();
    }
}

void WiFiManager::end() {
    if (webServer) {
        webServer->stop();
        delete webServer;
        webServer = nullptr;
    }
    
    if (dnsServer) {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }
    
    if (mqttClient) {
        mqttClient->disconnect();
        delete mqttClient;
        mqttClient = nullptr;
    }
    
    WiFi.disconnect();
}

bool WiFiManager::connect(const char* ssid, const char* password) {
    strncpy(config.ssid, ssid, sizeof(config.ssid) - 1);
    strncpy(config.password, password, sizeof(config.password) - 1);
    config.ssid[sizeof(config.ssid) - 1] = '\0';
    config.password[sizeof(config.password) - 1] = '\0';
    
    return connectToWiFi();
}

void WiFiManager::disconnect() {
    WiFi.disconnect();
    currentStatus = WIFI_DISCONNECTED;
}

bool WiFiManager::connectToWiFi() {
    if (strlen(config.ssid) == 0) {
        if (debugOutput) {
            Serial.println("WiFiManager: No SSID configured");
        }
        return false;
    }
    
    if (debugOutput) {
        Serial.print("WiFiManager: Connecting to ");
        Serial.println(config.ssid);
    }
    
    WiFi.begin(config.ssid, config.password);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT_MS) {
        delay(500);
        if (debugOutput) {
            Serial.print(".");
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        if (debugOutput) {
            Serial.println();
            Serial.print("WiFiManager: Connected to ");
            Serial.println(config.ssid);
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
        }
        return true;
    } else {
        if (debugOutput) {
            Serial.println();
            Serial.println("WiFiManager: Connection failed");
        }
        return false;
    }
}

void WiFiManager::setConfig(const WiFiConfig& newConfig) {
    config = newConfig;
    saveConfig();
}

WiFiConfig WiFiManager::getConfig() const {
    return config;
}

void WiFiManager::setDeviceId(const char* deviceId) {
    strncpy(config.deviceId, deviceId, sizeof(config.deviceId) - 1);
    config.deviceId[sizeof(config.deviceId) - 1] = '\0';
}

void WiFiManager::enableCloudConnection(bool enable) {
    config.enableCloud = enable;
}

WiFiStatus WiFiManager::getStatus() const {
    return currentStatus;
}

bool WiFiManager::hasValidConfig() const {
    return strlen(config.ssid) > 0;
}

IPAddress WiFiManager::getLocalIP() const {
    return WiFi.localIP();
}

String WiFiManager::getMACAddress() const {
    return WiFi.macAddress();
}

int WiFiManager::getSignalStrength() const {
    return WiFi.RSSI();
}

bool WiFiManager::publishSensorData(const String& data) {
    if (!mqttClient || !mqttClient->connected()) {
        return false;
    }
    
    String topic = "irrigation/" + String(config.deviceId) + "/sensors";
    return mqttClient->publish(topic.c_str(), data.c_str());
}

bool WiFiManager::publishStatus(const String& status) {
    if (!mqttClient || !mqttClient->connected()) {
        return false;
    }
    
    String topic = "irrigation/" + String(config.deviceId) + "/status";
    return mqttClient->publish(topic.c_str(), status.c_str());
}

bool WiFiManager::subscribe(const char* topic) {
    if (!mqttClient || !mqttClient->connected()) {
        return false;
    }
    
    return mqttClient->subscribe(topic);
}

bool WiFiManager::isCloudConnected() const {
    return mqttClient && mqttClient->connected();
}

void WiFiManager::enableOTA(bool enable) {
    otaEnabled = enable;
    
    if (enable) {
        // Configure ArduinoOTA
        ArduinoOTA.setHostname(config.deviceId);
        
        ArduinoOTA.onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH) {
                type = "sketch";
            } else { // U_SPIFFS
                type = "filesystem";
            }
            Serial.println("Start updating " + type);
        });
        
        ArduinoOTA.onEnd([]() {
            Serial.println("\nEnd");
        });
        
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        });
        
        ArduinoOTA.onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) {
                Serial.println("Auth Failed");
            } else if (error == OTA_BEGIN_ERROR) {
                Serial.println("Begin Failed");
            } else if (error == OTA_CONNECT_ERROR) {
                Serial.println("Connect Failed");
            } else if (error == OTA_RECEIVE_ERROR) {
                Serial.println("Receive Failed");
            } else if (error == OTA_END_ERROR) {
                Serial.println("End Failed");
            }
        });
        
        ArduinoOTA.begin();
        
        if (debugOutput) {
            Serial.println("WiFiManager: OTA enabled");
        }
    }
}

void WiFiManager::handleOTA() {
    if (otaEnabled) {
        ArduinoOTA.handle();
    }
}

bool WiFiManager::connectMQTT() {
    if (!config.enableCloud || strlen(config.mqttServer) == 0) {
        return false;
    }
    
    mqttClient->setServer(config.mqttServer, config.mqttPort);
    
    String clientId = "irrigation-" + String(config.deviceId);
    
    if (debugOutput) {
        Serial.print("WiFiManager: Connecting to MQTT server ");
        Serial.println(config.mqttServer);
    }
    
    bool connected = false;
    if (strlen(config.mqttUser) > 0) {
        connected = mqttClient->connect(clientId.c_str(), config.mqttUser, config.mqttPassword);
    } else {
        connected = mqttClient->connect(clientId.c_str());
    }
    
    if (connected) {
        if (debugOutput) {
            Serial.println("WiFiManager: MQTT connected");
        }
        
        // Subscribe to device-specific topics
        String baseTopic = "irrigation/" + String(config.deviceId) + "/";
        mqttClient->subscribe((baseTopic + "ota").c_str());
        mqttClient->subscribe((baseTopic + "config").c_str());
        mqttClient->subscribe((baseTopic + "models").c_str());
        
        return true;
    } else {
        if (debugOutput) {
            Serial.print("WiFiManager: MQTT connection failed, rc=");
            Serial.println(mqttClient->state());
        }
        return false;
    }
}

void WiFiManager::reconnectMQTT() {
    if (!config.enableCloud || !WiFi.isConnected()) {
        return;
    }
    
    static unsigned long lastMQTTReconnectAttempt = 0;
    unsigned long now = millis();
    
    if (now - lastMQTTReconnectAttempt > 5000) {
        lastMQTTReconnectAttempt = now;
        
        if (connectMQTT()) {
            lastMQTTReconnectAttempt = 0;
        }
    }
}

void WiFiManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (messageCallback) {
        char message[length + 1];
        memcpy(message, payload, length);
        message[length] = '\0';
        
        messageCallback(String(topic), String(message));
    }
}

bool WiFiManager::loadConfig() {
    File configFile = SPIFFS.open(CONFIG_FILE, "r");
    if (!configFile) {
        if (debugOutput) {
            Serial.println("WiFiManager: Config file not found, using defaults");
        }
        return false;
    }
    
    size_t size = configFile.size();
    if (size > 1024) {
        if (debugOutput) {
            Serial.println("WiFiManager: Config file size is too large");
        }
        configFile.close();
        return false;
    }
    
    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);
    configFile.close();
    
    StaticJsonDocument<1024> doc;
    auto error = deserializeJson(doc, buf.get());
    if (error) {
        if (debugOutput) {
            Serial.println("WiFiManager: Failed to parse config file");
        }
        return false;
    }
    
    if (doc.containsKey("ssid")) {
        strlcpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
    }
    if (doc.containsKey("password")) {
        strlcpy(config.password, doc["password"] | "", sizeof(config.password));
    }
    if (doc.containsKey("mqttServer")) {
        strlcpy(config.mqttServer, doc["mqttServer"] | "", sizeof(config.mqttServer));
    }
    if (doc.containsKey("mqttPort")) {
        config.mqttPort = doc["mqttPort"] | 1883;
    }
    if (doc.containsKey("mqttUser")) {
        strlcpy(config.mqttUser, doc["mqttUser"] | "", sizeof(config.mqttUser));
    }
    if (doc.containsKey("mqttPassword")) {
        strlcpy(config.mqttPassword, doc["mqttPassword"] | "", sizeof(config.mqttPassword));
    }
    if (doc.containsKey("deviceId")) {
        strlcpy(config.deviceId, doc["deviceId"] | "smart-irrigation-001", sizeof(config.deviceId));
    }
    if (doc.containsKey("enableCloud")) {
        config.enableCloud = doc["enableCloud"] | false;
    }
    
    if (debugOutput) {
        Serial.println("WiFiManager: Config loaded successfully");
    }
    
    return true;
}

bool WiFiManager::saveConfig() {
    StaticJsonDocument<1024> doc;
    
    doc["ssid"] = config.ssid;
    doc["password"] = config.password;
    doc["mqttServer"] = config.mqttServer;
    doc["mqttPort"] = config.mqttPort;
    doc["mqttUser"] = config.mqttUser;
    doc["mqttPassword"] = config.mqttPassword;
    doc["deviceId"] = config.deviceId;
    doc["enableCloud"] = config.enableCloud;
    
    File configFile = SPIFFS.open(CONFIG_FILE, "w");
    if (!configFile) {
        if (debugOutput) {
            Serial.println("WiFiManager: Failed to open config file for writing");
        }
        return false;
    }
    
    if (serializeJson(doc, configFile) == 0) {
        if (debugOutput) {
            Serial.println("WiFiManager: Failed to write config file");
        }
        configFile.close();
        return false;
    }
    
    configFile.close();
    
    if (debugOutput) {
        Serial.println("WiFiManager: Config saved successfully");
    }
    
    return true;
}

void WiFiManager::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback = callback;
}

void WiFiManager::setMessageCallback(MessageCallback callback) {
    messageCallback = callback;
}

void WiFiManager::setDebugOutput(bool enable) {
    debugOutput = enable;
}

// Stub implementations for other methods mentioned in header
void WiFiManager::startAccessPoint() {
    // Basic AP mode implementation
    WiFi.mode(WIFI_AP);
    WiFi.softAP(CONFIG_PORTAL_SSID);
    currentStatus = WIFI_AP_MODE;
    
    if (connectionCallback) {
        connectionCallback(currentStatus);
    }
}

void WiFiManager::stopAccessPoint() {
    WiFi.softAPdisconnect(true);
}

void WiFiManager::enableWebInterface(bool enable) {
    webInterfaceEnabled = enable;
    // Web interface implementation would go here
}

bool WiFiManager::isWebInterfaceEnabled() const {
    return webInterfaceEnabled;
}

String WiFiManager::getStatusString() const {
    switch (currentStatus) {
        case WIFI_CONNECTED: return "Connected";
        case WIFI_CONNECTING: return "Connecting";
        case WIFI_DISCONNECTED: return "Disconnected";
        case WIFI_AP_MODE: return "AP Mode";
        case WIFI_ERROR: return "Error";
        default: return "Unknown";
    }
}

String WiFiManager::scanNetworks() {
    // Basic network scanning implementation
    int n = WiFi.scanNetworks();
    String result = "[";
    
    for (int i = 0; i < n; i++) {
        if (i > 0) result += ",";
        result += "\"" + WiFi.SSID(i) + "\"";
    }
    
    result += "]";
    return result;
}

bool WiFiManager::isNetworkAvailable(const char* ssid) {
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) == ssid) {
            return true;
        }
    }
    return false;
}

void WiFiManager::printConfig() const {
    Serial.println("=== WiFi Configuration ===");
    Serial.print("SSID: ");
    Serial.println(config.ssid);
    Serial.print("Device ID: ");
    Serial.println(config.deviceId);
    Serial.print("MQTT Server: ");
    Serial.println(config.mqttServer);
    Serial.print("MQTT Port: ");
    Serial.println(config.mqttPort);
    Serial.print("Cloud Enabled: ");
    Serial.println(config.enableCloud ? "Yes" : "No");
    Serial.println("===========================");
}

void WiFiManager::printNetworkInfo() const {
    Serial.println("=== Network Information ===");
    Serial.print("Status: ");
    Serial.println(getStatusString());
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.println("============================");
}