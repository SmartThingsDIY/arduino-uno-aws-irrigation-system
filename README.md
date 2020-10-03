
⚡️ COMPONENTS AND SUPPLIES
==========================

![](https://github.com/isbkch/arduino-uno-aws-irrigation-system/blob/master/img/moisture.png?raw=true)

*   [Arduino Uno](https://amzn.to/2EqybyM)
*   [Breadboard](https://amzn.to/2Ei40tP)
*   [Jumper Wires](https://amzn.to/2Ehh2ru)
*   [4 Channel Relay](https://amzn.to/3ggJbMs)
*   [Capacitive Soil Moisture Sensor](https://amzn.to/3gn5FLN)
*   [Submersible Mini Water Pumps](https://amzn.to/32hk9I1)
*   [2 AA Battery Holder with Switch](https://amzn.to/2CPxNt8)

There is now an ensemble kit that includes most of the required hardware: [WayinTop Automatic Irrigation DIY Kit](https://amzn.to/3aN5qsj). But you still need to purchase the [2 AA Battery Holder](https://amzn.to/2CPxNt8), the [Arduino Uno](https://amzn.to/2EqybyM) and the [Jumper Wires](https://amzn.to/2Ehh2ru)
PS: This guide works for both options

🚀APPS
======

*   [Arduino IDE](https://www.arduino.cc/en/main/software)

ABOUT
=====

This is an extremely popular DIY project because it automatically waters your plants and flowers according to the level of moisture in the soil. The list of items above combines the development board (Arduino Uno), Mini pump, Tubing, Soil Moisture Sensors and a 4 Channel 5V Relay Module, which is all you need to build an automated irrigation system.

BUY VS BUILD
============

You can always go ahead and buy a ready to use a solution like this [Self Watering System](https://amzn.to/3laBGds) for roughly the same cost. But if you're looking to learn Arduino development while doing a fun & a useful project in the meantime, then stick around

![](https://github.com/isbkch/arduino-uno-aws-irrigation-system/blob/master/img/moisture_sensor.png?raw=true)

What is moisture?
=================

Moisture is the presence of water, often in trace amounts. Small amounts of water may be found, for example, in the air (humidity), in foods, and in some commercial products. Moisture also refers to the amount of water vapor present in the air.

CAPACITIVE SENSOR
=================

Inserted into the soil around the plants. This sensor can check whether your plant is thirsty by measuring the level of moisture in the soil

THE WIRING
==========

There are a number of connections to be made. Lining up the display with the top of the breadboard helps to identify its pins without too much counting, especially if the breadboard has its rows numbered with row 1 as the top row of the board. Do not forget, the long yellow lead that links the slider of the pot to pin 3 of the display. The potentiometer is used to control the contrast of the display.

![](https://github.com/isbkch/arduino-uno-aws-irrigation-system/blob/master/img/wiring_diagram.png?raw=true)

THE CODE
========

Open Arduino IDE and chose **Arduino UNO** from the board manager

![](https://github.com/isbkch/arduino-uno-aws-irrigation-system/blob/master/img/board.png?raw=true)

Paste the content of the file [code.ino](https://github.com/isbkch/arduino-uno-aws-irrigation-system/blob/master/code.ino) into a new Sketch. Then compile and upload the code to your board

### Code Explanation

In order to use Arduino to control the four-channel relay, we need to define four control pins of theArduino.
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