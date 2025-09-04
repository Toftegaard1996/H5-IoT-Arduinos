//------------------------------------------------------------- KOMMENTAR ------------------------------------------------------------
/*
 # Projekt part: Publisher/måle client
 # Mål: Måle temperatur og lysstyrke og sende alarm hvis nogen af delene overskrider ønskede værdier
 #--------------------------------------------------------------------------------------------------------
 # Komponenter:
 # - Arduino Uno WiFi Rev2
 # - Grove Temperature and Humidity reader
 # - Grove Light sensor
 # - Grove RGB LCD Display
 # - Raspberry Pi 4 med:
 #    - Mosquitto (MQTT broker)
 #    - MariaDB (database med tabeller: users, keycards)
 #    - Python-service (MQTT <-> DB integration)
 #---------------------------------------------------------------------------------------------------------
 # Funktionalitet:
 # - Arduino måler værdierne efter givet interval
 # - Overskrider nogle af værdierne de ønskede fra databasen, sender Arduinoen en alarm
 #--------------------------------------------------------------------------------------------------------
 # Kode: Et eksempel på en publisher klient er taget fra arduino docs og er blevet tilpasset projektet
 # - Nogle forklarende kommentarer i koden kommer fra det oprindelige eksempel
 # Forfatter: Bob & Ditte
 # Dato: sep. 2025
 */
#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>
#include <ArduinoUniqueID.h>
#include "Arduino_SensorKit.h"
#include "arduino_secrets.h"
#include <Wire.h>
#include "rgb_lcd.h"

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
const char alarms[] = "Alarms";

// ------------------ Misc ------------------
//set interval for sending messages (milliseconds)
const long interval = 8000;
unsigned long previousMillis = 0;

char UID[21]; //Array to hold Unique ID from Arduino
rgb_lcd lcd; //Giving the lcd screen a name
#define LightSensor A0

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  Environment.begin(); //Start the temperature sensor in D3
  lcd.begin(16,2); //Begin LCD screen
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }


  UniqueIDdump(Serial);
	Serial.print("UniqueID: ");
	for (size_t i = 0; i < UniqueIDsize; i++)
	{
    sprintf(&UID[2*i], "%02X", UniqueID[i]);
	}
  UID[20] = '\0';
	Serial.println(UID);

  // attempt to connect to Wifi network:
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(5000);
    Serial.println(SECRET_SSID);
    Serial.println(SECRET_PASS);
  }

  Serial.println("You're connected to the network");
  Serial.println();

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  // Connect to the broker located on the raspberry pi
  mqttClient.setUsernamePassword(mqtt_user, mqtt_pass); //Set username and password for mosquitto
  if (!mqttClient.connect(broker, port)) { // Connect to the broker with the credentials
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();
}

void loop() {
  // call poll() regularly to allow the library to send MQTT keep alive which
  // avoids being disconnected by the broker
  mqttClient.poll();

  unsigned long currentMillis = millis();
  int value = analogRead(LightSensor);

  if (currentMillis - previousMillis >= interval) {
    // save the last time a message was sent
    previousMillis = currentMillis;

    Serial.print("Sending message to topic: ");
    Serial.println(topic);
    Serial.print("ID: ");
    Serial.println(UID);
    Serial.print("Temp: ");
    Serial.println(Environment.readTemperature());
    Serial.print("Light level: ");
    Serial.println(value);
    

    // send message, the Print interface can be used to set the message contents
    mqttClient.beginMessage(topic);
    mqttClient.println(UID);
    mqttClient.print(Environment.readTemperature());
    mqttClient.endMessage();

    if (value < 150)
    {
      Serial.println("Light too low! Sending alarm");
      mqttClient.beginMessage(alarms);
      mqttClient.println(UID);
      mqttClient.print("Light!");
      mqttClient.endMessage();
    }

    Serial.println();

    //update the screen with info
    lcd.setCursor(0, 0);
    lcd.print("ID:");
    lcd.print(UID);
    lcd.setCursor(0, 1);    // Set the Coordinates 
    lcd.print("Temp:");   
    lcd.print(Environment.readTemperature()); // Print the Values  

  }
}