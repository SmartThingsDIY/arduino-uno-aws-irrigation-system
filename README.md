
<h3>I thought this code would help someone start their first Arduino project. So here it is.</h3>

<h1>⚡️ COMPONENTS AND SUPPLIES</h1>

<img align="right" src="https://github.com/isbkch/arduino-uno-irrigation-system/blob/master/img/moisture.png?raw=true" style="max-width:100%;" height="350">

<ul>
    <li><a href="https://amzn.to/2EqybyM">Arduino Uno</a></li>
    <li><a href="https://amzn.to/2Ei40tP">Breadboard</a></li>
    <li><a href="https://amzn.to/2Ehh2ru">Jumper Wires</a></li>
    <li><a href="https://amzn.to/3ggJbMs">4 Channel Relay</a></li>
    <li><a href="https://amzn.to/3gn5FLN">Capacitive Soil Moisture Sensor</a></li>
    <li><a href="https://amzn.to/32hk9I1">Submersible Mini Water Pumps</a></li>
    <li><a href="https://amzn.to/2CPxNt8">2 AA Battery Holder with Switch</a></li>
</ul>
<br>

There is now an ensemble kit that includes most of the required hardware: <a href="https://amzn.to/3aN5qsj">WayinTop Automatic Irrigation DIY Kit</a>. But you still need to purchase the <a href="https://amzn.to/2CPxNt8">2 AA Battery Holder</a>, the <a href="https://amzn.to/2EqybyM">Arduino Uno</a> and the <a href="https://amzn.to/2Ehh2ru">Jumper Wires</a>
<br>

PS: This guide works for both options

<br>
<h1>🚀APPS</h1>
<ul>
    <li><a href="https://www.arduino.cc/en/main/software">Arduino IDE</a></li>
</ul>

<h1>ABOUT</h1>
<p>This is an extremely popular DIY project because it automatically waters your plants and flowers according to the level of moisture in the soil. The list of item above combines the development board (Arduino Uno), Mini pump, Tubing, Soil Moisture Sensors and a 4 Channel 5V Relay Module, which is all you need to build an automated irrigation system.</p>

<h1>BUY VS BUILD</h1>
<p>You can always go ahead and buy a ready to use solution like this <a href="https://amzn.to/3laBGds">Self Watering System</a> for roughly the same cost. But if you're looking to learn Arduino development while doing a fun & useful project in the meantime, then stick around</p>

<img align="right" src="https://github.com/isbkch/arduino-uno-irrigation-system/blob/master/img/moisture_sensor.png?raw=true" style="max-width:100%;" height="300">

<h1>What is moisture?</h1>
<p>Moisture is the presence of water, often in trace amounts. Small amounts of water may be found, for example, in the air (humidity), in foods, and in some commercial products. Moisture also refers to the amount of water vapor present in the air.</p>

<h1>CAPACITIVE SENSOR</h1>

<p>Inserted into the soil around the plants. This sensor can check whether your plant is thirsty by measuring the level of moisture in the soil</p>

<h1>THE WIRING</h1>

<p>There are a number of connections to be made. Lining up the display with the top of the breadboard helps to identify its pins without too much counting, especially if the breadboard has its rows numbered with row 1 as the top row of the board. Do not forget, the long yellow lead that links the slider of the pot to pin 3 of the display. The potentiometer is used to control the contrast of the display.</p>

<img src="https://github.com/isbkch/arduino-uno-irrigation-system/blob/master/img/wiring_diagram.png?raw=true"></a>

<h1>THE CODE</h1>
Open Arduino IDE and chose <strong>Arduino UNO</strong> from the board manager<br><br>
<img align="center" src="https://github.com/isbkch/arduino-uno-irrigation-system/blob/master/img/board.png?raw=true" style="max-width:100%;" height="600">

<br><br>
Paste the content of the file <a href="https://github.com/isbkch/arduino-uno-irrigation-system/blob/master/code.ino">code.ino</a> into a new Sketch. Then compile and upload the code to your board

<h3>Code Explanation</h3>

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

In the 'setup()' function, mainly using 'Serial.begin()' function to set the serial port baud rate, using the 'pinMode' function to set the port input and output function of arduino. 'OUTPUT' indicates output function and 'INPUT' indicates input function.

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

Finally, in the 'loop' function, cycle use the 'Serial.print' function to output the prompt information in the serial monitor, use the 'analogRead' function to read the sensor value. Then use the 'if' function to determine the sensor value, if the requirements are met, turn on the relay and using the 'digitalWrite' function to operate the pump, if not, then turn off the relay.
<br><br>
PS: <br>
There are total four lines of 'if(value4>550)' in the 'loop' function. This is the statement that controls the start of the pump. The values inside need to be reset according to the water needs of the plants and flowers.

<h1>DEMO VIDEO</h1>