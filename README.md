# Connected Smart Irrigation System with Arduino Uno board, Soil capacitive sensor, ESP8266 WiFi module and AWS IOT

This repo accompanies the "Connected Irrigation System" YouTube series. it contains the code, libraries, diagrams, and more information that I promised in the videos.

[**Video 1: Building an IoT Irrigation System with Arduino, and Soil sensor**](https://www.youtube.com/watch?v=JdvnfENodak)

⚡️ COMPONENTS AND SUPPLIES
==========================

<img align="right" src="https://github.com/iLyas/arduino-uno-aws-irrigation-system/blob/master/docs/moisture.png?raw=true" style="max-width:100%;" height="350">

* [Arduino Uno](https://amzn.to/2EqybyM)
* [Breadboard](https://amzn.to/2Ei40tP)
* [Jumper Wires](https://amzn.to/2Ehh2ru)
* [4 Channel Relay](https://amzn.to/3ggJbMs)
* [Capacitive Soil Moisture Sensor](https://amzn.to/3gn5FLN)
* [Submersible Mini Water Pumps](https://amzn.to/32hk9I1)
* [2 AA Battery Holder with Switch](https://amzn.to/2CPxNt8)
* [Hardware / Storage Cabinet Drawer](https://amzn.to/36ehDpB)
* [ESP8266 ESP-01 WiFi Module](https://amzn.to/30fUWNS)
* [ESP8266 ESP-01 programmable USB](https://amzn.to/345egi6)
* [ESP8266 ESP-01 Breadboard Adapter](https://amzn.to/3kSFVcP)

There is now an ensemble kit that includes most of the required hardware: [WayinTop Automatic Irrigation DIY Kit](https://amzn.to/3aN5qsj). But you still need to purchase the [2 AA Battery Holder](https://amzn.to/2CPxNt8), the [Arduino Uno](https://amzn.to/2EqybyM) and the [Jumper Wires](https://amzn.to/2Ehh2ru)
PS: This guide works for both options

AI/ML Stack
=====

* **LLMs**: GPT-3.5/4, Llama 3 for conversational AI
* **Frameworks**: LangChain for orchestration, TensorFlow Lite for edge ML
* **Vector DB**: Pinecone/Chroma for RAG implementation
* **ML Models**: Random Forest for predictions, LSTM for time-series

🖥 APPS
======

* [VSCode](https://code.visualstudio.com/)
* [Fritzing](https://fritzing.org/)
* [AWS CLI](https://docs.aws.amazon.com/cli/latest/userguide/cli-chap-install.html)
* [PlatformIO](https://platformio.org/)

📦 Libraries
---------

* [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
* [SoftwareSerial](https://www.arduino.cc/en/Reference.SoftwareSerial)

Cloud Infrastructure
=====

* **AWS IoT Core**: Device management and data ingestion
* **AWS Lambda**: Serverless compute for AI inference
* **API**: FastAPI for REST endpoints
* **Storage**: S3 for historical data, DynamoDB for real-time


ABOUT
=====

This is an extremely popular DIY project because it automatically waters your plants and flowers according to the level of moisture in the soil. The list of items above combines the development board (Arduino Uno), Mini pump, Tubing, Soil Moisture Sensors and a 4 Channel 5V Relay Module, which is all you need to build an automated irrigation system.

BUY VS BUILD
============

You can always go ahead and buy a ready to use a solution like this [Self Watering System](https://amzn.to/3laBGds) for roughly the same cost. But if you're looking to learn Arduino development while doing a fun and useful project in the meantime, then stick around

<img align="right" src="https://github.com/iLyas/arduino-uno-aws-irrigation-system/blob/master/docs/moisture_sensor.png?raw=true" style="max-width:100%;" height="350">

What is moisture?
=================

Moisture is the presence of water, often in trace amounts. Small amounts of water may be found, for example, in the air (humidity), in foods, and in some commercial products. Moisture also refers to the amount of water vapor present in the air.

CAPACITIVE SENSOR
=================

Inserted into the soil around the plants. This sensor can check whether your plant is thirsty by measuring the level of moisture in the soil

THE WIRING
==========

There are a number of connections to be made. Lining up the display with the top of the breadboard helps to identify its pins without too much counting, especially if the breadboard has its rows numbered with row 1 as the top row of the board. Do not forget, the long yellow lead that links the slider of the pot to pin 3 of the display. The potentiometer is used to control the contrast of the display.

<img align="center" src="https://github.com/iLyas/arduino-uno-aws-irrigation-system/blob/master/docs/wiring_diagram.png?raw=true" style="max-width:100%;" height="500">

THE CODE
========

### Code Explanation

In order to use Arduino to control the four-channel relay, we need to define four control pins of the Arduino.

```cpp
int IN1 = 2;
int IN2 = 3;
int IN3 = 4;
int IN4 = 5;
```

Since the value detected by the soil moisture sensor is an analog signal, so four analog ports are defined.

```cpp
int Pin1 = A0;
int Pin2 = A1;
int Pin3 = A2;
int Pin4 = A3;
```

We need to use a variable to store the value detected by the sensor. Since there are four sensors, we define four variables.

```cpp
float sensor1Value = 0;
float sensor2Value = 0;
float sensor3Value = 0;
float sensor4Value = 0;
```

In the `setup()` function, mainly using `Serial.begin()` function to set the serial port baud rate, using the `pinMode` function to set the port input and output function of arduino. `OUTPUT` indicates output function and `INPUT` indicates input function.

```cpp
void setup() {
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
```

Finally, in the `loop()` function, cycle use the `Serial.print()` function to output the prompt information in the serial monitor, use the `analogRead` function to read the sensor value. Then use the `if` function to determine the sensor value, if the requirements are met, turn on the relay and using the `digitalWrite` function to operate the pump, if not, then turn off the relay.

 ```cpp
void loop() {
    Serial.print("Plant 1 - Moisture Level:");
    sensor1Value = analogRead(Pin1);
    Serial.println(sensor1Value);

    if (sensor1Value > 450) {
        digitalWrite(IN1, LOW);
    } else {
        digitalWrite(IN1, HIGH);
    }
    ...
}
```

PS:
There are total four lines of `if(value4>550)` in the `loop()` function. This is the statement that controls the start of the pump. The values inside need to be reset according to the water needs of the plants and flowers.

Next Step
---------

For code that goes into the WiFi board (ESP8266 ESP01) and more explanation, please head out to this repo: <https://github.com/iLyas/esp8266-01-aws-mqtt>
