//------------------------------------------------------------- KOMMENTAR ------------------------------------------------------------
/*
 # Projekt part: Subscribe client
 # Mål: Subscribe til forskellige topics og reagere ud fra hvilket topic der kommer ind
 #--------------------------------------------------------------------------------------------------------
 # Komponenter:
 # - Arduino Uno WiFi Rev2
 # - Grove LED
 # - Grove Buzzer
 # - Grove RGB LCD Display
 # - Raspberry Pi 4 med:
 #    - Mosquitto (MQTT broker)
 #    - MariaDB (database med tabeller: users, keycards)
 #    - Python-service (MQTT <-> DB integration)
 #---------------------------------------------------------------------------------------------------------
 # Funktionalitet:
 # - Arduino forbliver passiv indtil en alarm går i gang
 # - Når alarm topic bliver triggered, reagere Arduino med hvilket board der har slået alarm og hvad det drejer sig om
 #--------------------------------------------------------------------------------------------------------
 # Kode: Et eksempel på en subscribe klient er taget fra arduino docs og er blevet tilpasset projektet
 # - Nogle forklarende kommentarer i koden kommer fra det oprindelige eksempel
 # Forfatter: Bob & Ditte
 # Dato: sep. 2025
 */

#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>
#include "rgb_lcd.h"
#include "arduino_secrets.h"

// --------- Sensitive information from the secret tab ---------
char ssid[] = SECRET_SSID; // network SSID
char pass[] = SECRET_PASS; // network password
char mqtt_user[] = SECRET_MQTT_USER; // Mosquitto username
char mqtt_pass[] = SECRET_MQTT_PASS; // Mosquitto password

// ------------------ WIFI ------------------
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// ------------------ MQTT ------------------
const char broker[] = "192.168.1.137";
int        port     = 1883;
const char topic[]  = "Topic:OpsætningMQTT";
const char alarms[] = "Alarms/#";
const char keycard[] = "Alarms/KeyCard";
const char keypad[] = "Alarms/KeyPad";
const char noIssue[] = "Alarms/NoIssue";

// ------------------ Alarm ------------------
const int redLedPin = A1;   // Grove A1 for external red LED
const int buzzerPin = A2;   // Grove A2 for buzzer

// ------------------ Misc ------------------
rgb_lcd lcd; //Giving the lcd screen a name

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  lcd.begin(16,2); //Begin LCD screen
  lcd.setRGB(0, 255, 0); // set backlight to green
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  // attempt to connect to Wifi network:
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(5000);
  }

  Serial.println("You're connected to the network");
  Serial.println();

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  // Connect to the broker located on the raspberry pi
  mqttClient.setUsernamePassword(mqtt_user, mqtt_pass); //Set username and password for mosquitto
  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);

  Serial.println("Subscribing to topics: ");
  Serial.println(topic);
  Serial.println(alarms);
  Serial.println();

  lcd.setRGB(0, 255, 0);
  lcd.print("Connected to");
  lcd.setCursor(0,1);
  lcd.print("topics");

  // subscribe to a topic
  mqttClient.subscribe(topic);
  mqttClient.subscribe(alarms);

  // topics can be unsubscribed using:
  // mqttClient.unsubscribe(topic);

  Serial.println("Topics: ");
  Serial.println(topic);
  Serial.println(alarms);
  Serial.println();

  //Initialize alarm LED
  pinMode(redLedPin, OUTPUT);
  digitalWrite(redLedPin, LOW); // off by default
  digitalWrite(buzzerPin, LOW); // off by default

  // Screen ready
  lcd.clear();
  lcd.setRGB(0, 255, 0);
  lcd.print("Awaiting topic");
}

void loop() {
  // call poll() regularly to allow the library to receive MQTT messages and
  // send MQTT keep alive which avoids being disconnected by the broker
  mqttClient.poll();
}

void onMqttMessage(int messageSize) {
  // we received a message, print out the topic and contents
  Serial.print("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  // Prepare topic for switch case
  int itopic = 0;
  if (mqttClient.messageTopic() == topic)
    itopic = 1;
  else if (mqttClient.messageTopic() == keycard)
    itopic = 2;
  else if (mqttClient.messageTopic() == keypad)
    itopic = 3;
  else if (mqttClient.messageTopic() == noIssue)
    itopic = 4;

  // Handle initial version of message and split UID from the rest of the message
    char buffer[30]; // Temporary variable to hold the UID and Temperature
    byte count = 0;
    // use the Stream interface to print the contents
    while (mqttClient.available()) {
      // Put the received message in a placeholder
      buffer[count] = mqttClient.read();
      count++;
      buffer[count] = '\0';
    }
    // Split the UID from the rest of the message
    char UID[21];
    for(int i = 0; i < 20; i++)
    {
      UID[i] = buffer[i];
    }
    UID[20] = '\0';

  //------------------------------------------- Temp ----------------------------------------------
  // Following handles if the rest of the message is temperature
  if (mqttClient.messageTopic() == 1)
  {
    char Temp[5];  
    count = 0;
    for(int i = 20; i < 24; i++)
    {
      Temp[count] = buffer[i];
      count++;
    }
    Temp[4] = '\0';
    int value = atoi(Temp);
    Serial.println(UID);
    Serial.println(value);
    
    // Handle reaction if received value exceeds chosen max value
    if (value >= 27)
    {
      Serial.println("Temp too high!");
      digitalWrite(redLedPin, HIGH); // turn on external red LED
    }
  }

  //------------------------------------------- Alarm ----------------------------------------------
  // Following triggers anytime an alarm is set off
  if (itopic > 1)
  {
    digitalWrite(redLedPin, HIGH); // turn on external red LED
    digitalWrite(buzzerPin, HIGH); // turn on buzzer
    lcd.setRGB(255, 0, 0); // Red
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ID:");
    lcd.print(UID);
    switch (itopic) {
      case 2:
        lcd.setCursor(0, 1);
        lcd.print("Issue: Card");
        break;
      case 3:
        lcd.setCursor(0, 1);
        lcd.print("Issue: Pincode");
        break;
      case 4:
        // Reset screen/alarm
        lcd.clear();
        lcd.setRGB(0, 255, 0);
        lcd.print("Awaiting topic");
        pinMode(redLedPin, OUTPUT);
        digitalWrite(redLedPin, LOW);
        digitalWrite(buzzerPin, LOW);
        break;
    }
  } 

  

  Serial.println();
  Serial.println();
}