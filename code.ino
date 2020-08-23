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

void setup() {
  Serial.begin(9600);
  // for (int i = 1; i < 5; i++) {
  //   pinMode(IN + i, OUTPUT);
  // }

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

void loop() {

  Serial.print("Plant 1 - Moisture Level:");
  sensor1Value = analogRead(Pin1);
  Serial.println(sensor1Value);

  if (sensor1Value > 450) {
    digitalWrite(IN1, LOW);
  } else {
    digitalWrite(IN1, HIGH);
  }

  Serial.print("Plant 2 - Moisture Level:");
  sensor2Value = analogRead(Pin2);
  Serial.println(sensor2Value);

  if (sensor2Value > 450) {
    digitalWrite(IN2, LOW);
  } else {
    digitalWrite(IN2, HIGH);
  }

  Serial.print("Plant 3 - Moisture Level:");
  sensor3Value = analogRead(Pin3);
  Serial.println(sensor3Value);

  if (sensor3Value > 450) {
    digitalWrite(IN3, LOW);
  } else {
    digitalWrite(IN3, HIGH);
  }

  Serial.print("Plant 4 - Moisture Level:");
  sensor4Value = analogRead(Pin4);
  Serial.println(sensor4Value);

  if (sensor4Value > 450) {
    digitalWrite(IN4, LOW);
  } else {
    digitalWrite(IN4, HIGH);
  }

  Serial.println();
  delay(1000);
}