#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// Configuration constants
#define WIFI_TIMEOUT_MS 20000
#define AP_TIMEOUT_MS 300000  // 5 minutes
#define RECONNECT_INTERVAL_MS 30000
#define CONFIG_PORTAL_SSID "SmartIrrigation-Setup"
#define CONFIG_FILE "/config.json"

// Connection status
enum WiFiStatus {
    WIFI_DISCONNECTED = 0,
    WIFI_CONNECTING = 1,
    WIFI_CONNECTED = 2,
    WIFI_AP_MODE = 3,
    WIFI_ERROR = 4
};

// Configuration structure
struct WiFiConfig {
    char ssid[64];
    char password[64];
    char mqttServer[64];
    int mqttPort;
    char mqttUser[32];
    char mqttPassword[32];
    char deviceId[32];
    bool enableCloud;
    
    WiFiConfig() {
        strcpy(ssid, "");
        strcpy(password, "");
        strcpy(mqttServer, "");
        mqttPort = 1883;
        strcpy(mqttUser, "");
        strcpy(mqttPassword, "");
        strcpy(deviceId, "smart-irrigation-001");
        enableCloud = false;
    }
};

class WiFiManager {
private:
    // WiFi and web server
    WebServer* webServer;
    DNSServer* dnsServer;
    WiFiClient wifiClient;
    PubSubClient* mqttClient;
    
    // Configuration
    WiFiConfig config;
    WiFiStatus currentStatus;
    
    // Timing
    unsigned long lastReconnectAttempt;
    unsigned long apModeStartTime;
    
    // Internal methods
    void startConfigPortal();
    void handleConfigPortal();
    void setupWebHandlers();
    void handleRoot();
    void handleConfig();
    void handleSave();
    void handleRestart();
    void handleStatus();
    void handleAPI();
    
    // Configuration management
    bool loadConfig();
    bool saveConfig();
    void resetConfig();
    
    // WiFi connection
    bool connectToWiFi();
    bool isConnected();
    void handleDisconnection();
    
    // MQTT
    bool connectMQTT();
    void reconnectMQTT();
    void mqttCallback(char* topic, byte* payload, unsigned int length);
    
    // Utilities
    String getDeviceInfo();
    String getNetworkInfo();
    bool isConfigComplete();
    
public:
    WiFiManager();
    ~WiFiManager();
    
    // Initialization
    bool begin();
    void loop();
    void end();
    
    // WiFi management
    bool connect(const char* ssid, const char* password);
    void disconnect();
    void startAccessPoint();
    void stopAccessPoint();
    
    // Configuration
    void setConfig(const WiFiConfig& newConfig);
    WiFiConfig getConfig() const;
    void setDeviceId(const char* deviceId);
    void enableCloudConnection(bool enable);
    
    // Status
    WiFiStatus getStatus() const;
    bool hasValidConfig() const;
    String getStatusString() const;
    IPAddress getLocalIP() const;
    String getMACAddress() const;
    int getSignalStrength() const;
    
    // MQTT methods
    bool publishSensorData(const String& data);
    bool publishStatus(const String& status);
    bool subscribe(const char* topic);
    bool isCloudConnected() const;
    
    // Web interface
    void enableWebInterface(bool enable);
    bool isWebInterfaceEnabled() const;
    
    // OTA updates
    void enableOTA(bool enable);
    void handleOTA();
    
    // Network scanning
    String scanNetworks();
    bool isNetworkAvailable(const char* ssid);
    
    // Debugging
    void printConfig() const;
    void printNetworkInfo() const;
    void setDebugOutput(bool enable);
    
    // Callbacks
    typedef void (*ConnectionCallback)(WiFiStatus status);
    typedef void (*MessageCallback)(String topic, String message);
    
    void setConnectionCallback(ConnectionCallback callback);
    void setMessageCallback(MessageCallback callback);
    
private:
    ConnectionCallback connectionCallback;
    MessageCallback messageCallback;
    bool debugOutput;
    bool webInterfaceEnabled;
    bool otaEnabled;
};

#endif // WIFI_MANAGER_H