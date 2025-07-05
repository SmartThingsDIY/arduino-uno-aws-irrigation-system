/**
 * LEGACY VERSION - Original Smart Irrigation System
 * 
 * This is the original implementation using ESP8266 and simple threshold logic.
 * Preserved for reference and backward compatibility.
 * 
 * For the new Edge AI version, see: edge-ai/arduino-ml/examples/smart_irrigation.ino
 * 
 * @author iLyas Bakouch
 * @github https://github.com/SmartThingsDIY
 * @version 3.0 (Legacy)
 */

#include <Arduino.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson (use v6.xx)
#include <SoftwareSerial.h>

#define DEBUG true

// ESP TX => Uno Pin 2
// ESP RX => Uno Pin 3
SoftwareSerial wifi(2, 3);

// **************
String sendDataToWiFiBoard(String command, const int timeout, boolean debug);
String prepareDataForWiFi(float sensor1Value, float sensor2Value, float sensor3Value, float sensor4Value);
void setup();
void loop();
// **************

int IN1 = 2;
int IN2 = 3;
int IN3 = 4;
int IN4 = 5;

int Pin1 = A0;
int Pin2 = A1;
int Pin3 = A2;
int Pin4 = A3;

float sensor1Value = 0;
float sensor2Value = 0;
float sensor3Value = 0;
float sensor4Value = 0;

/**
 * Build and return a JSON document from the sensor data
 * @param sensor1Value
 * @param sensor2Value
 * @param sensor3Value
 * @param sensor4Value
 * @return
 */
String prepareDataForWiFi(float sensor1Value, float sensor2Value, float sensor3Value, float sensor4Value)
{
  StaticJsonDocument<200> doc;

  doc["sensor1Value"] = String(sensor1Value);
  doc["sensor2Value"] = String(sensor2Value);
  doc["sensor3Value"] = String(sensor3Value);
  doc["sensor4Value"] = String(sensor4Value);

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

  return jsonBuffer;
}

/**
 * Send data through Serial to ESP8266 module
 * @param command
 * @param timeout
 * @param debug
 * @return
 */
String sendDataToWiFiBoard(String command, const int timeout, boolean debug)
{
  String response = "";

  wifi.print(command); // send the read character to the esp8266

  long int time = millis();

  while ((time + timeout) > millis())
  {
    while (wifi.available())
    {
      // The esp has data so display its output to the serial window
      char c = wifi.read(); // read the next character.
      response += c;
    }
  }

  if (debug)
  {
    Serial.print(response);
  }

  return response;
}

void setup()
{
  Serial.begin(9600);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(Pin1, INPUT);
  pinMode(Pin2, INPUT);
  pinMode(Pin3, INPUT);
  pinMode(Pin4, INPUT);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, HIGH);

  delay(500);
}

void loop()
{

  if (DEBUG == true)
  {
    Serial.print("buffer: ");
    if (wifi.available())
    {
      String espBuf;
      long int time = millis();

      while ((time + 1000) > millis())
      {
        while (wifi.available())
        {
          // The esp has data so display its output to the serial window
          char c = wifi.read(); // read the next character.
          espBuf += c;
        }
      }
      Serial.print(espBuf);
    }
    Serial.println(" endbuffer");
  }

  Serial.print("Plant 1 - Moisture Level:");
  sensor1Value = analogRead(Pin1);
  Serial.println(sensor1Value);

  if (sensor1Value > 450)
  {
    digitalWrite(IN1, LOW);
  }
  else
  {
    digitalWrite(IN1, HIGH);
  }

  Serial.print("Plant 2 - Moisture Level:");
  sensor2Value = analogRead(Pin2);
  Serial.println(sensor2Value);

  if (sensor2Value > 450)
  {
    digitalWrite(IN2, LOW);
  }
  else
  {
    digitalWrite(IN2, HIGH);
  }

  Serial.print("Plant 3 - Moisture Level:");
  sensor3Value = analogRead(Pin3);
  Serial.println(sensor3Value);

  if (sensor3Value > 450)
  {
    digitalWrite(IN3, LOW);
  }
  else
  {
    digitalWrite(IN3, HIGH);
  }

  Serial.print("Plant 4 - Moisture Level:");
  sensor4Value = analogRead(Pin4);
  Serial.println(sensor4Value);

  if (sensor4Value > 450)
  {
    digitalWrite(IN4, LOW);
  }
  else
  {
    digitalWrite(IN4, HIGH);
  }

  String preparedData = prepareDataForWiFi(sensor1Value, sensor2Value, sensor3Value, sensor4Value);
  if (DEBUG == true)
  {
    Serial.println(preparedData);
  }
  sendDataToWiFiBoard(preparedData, 1000, DEBUG);

  delay(2000); // 2 seconds
}