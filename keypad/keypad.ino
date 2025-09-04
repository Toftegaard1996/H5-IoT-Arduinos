#include <Arduino.h>
#include <Wire.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <LiquidCrystal.h>
#include <Keypad.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include "arduino_secrets.h"
#include <ArduinoUniqueID.h> // Get Serial ID


// Create LCD object
rgb_lcd lcd;


// You can also set RGB colors if you want
const int colorR = 0;
const int colorG = 255;
const int colorB = 0;


// ----------------- WiFi -----------------
const char *ssid = SECRET_SSID;
const char *password = SECRET_PASS;


// ----------------- MQTT -----------------
const char *mqtt_server = "192.168.1.137"; // Raspberry Pi IP
const int mqtt_port = 1883;
const char topic[] = "access/response";
const char keypadAlarm[] = "Alarms/KeyPad";
const char noIssue[] = "Alarms/NoIssue";
const char *mqtt_user = SECRET_MQTT_USER;
const char *mqtt_pass = SECRET_MQTT_PASS;


// ----------------- Keypad ----------------
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
// byte rowPins[ROWS] = {9, 8, 10, 11};   // adjust pins
// byte colPins[COLS] = {12, A0, A1, A2}; // adjust pins


// Seeed Grove Base Shield which doesn’t expose D9/D10/D11 directly.
// Digital Grove ports: D2, D3, D4, D5, D6, D7, D8
// Analog Grove ports: A0, A1, A2, A3
byte rowPins[ROWS] = {2, 3, 4, 5};  // rows → Grove D2, D3, D4, D5 //
byte colPins[COLS] = {6, 7, 8, A0}; // cols → Grove D6, D7, D8, A0


Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


// ----------------- Alarm -----------------
// const int ledPin = 13;
const int redLedPin = A1;   // Grove A1 for external red LED
const int buzzerPin = A2;   // Grove A2 for buzzer
const int greenLedPin = A3; // Grove A3 for green LED

//------------------------------------------- UNIQUE ID AND TRIES ---------------------------------------------
char UID[21]; //Array to hold Unique ID from Arduino

// ----------------- MQTT Client -----------
WiFiClient wifiClient;
PubSubClient client(wifiClient);


String enteredPin = "";
int wrongAttempts = 0;


// ----------------- Callback -----------------
void callback(char *topic, byte *message, unsigned int length)
{
    String payload;
    for (int i = 0; i < length; i++)
        payload += (char)message[i];


    Serial.print("MQTT Message received [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(payload);


    lcd.clear();
    if (payload == "ACCESS_GRANTED")
    {
        lcd.clear();
        lcd.setRGB(0, 255, 0); // green for granted
        lcd.print("Door Open");
        Serial.println("Access granted → Door Open");
        digitalWrite(greenLedPin, HIGH); // turn ON green LED
        delay(2000);
        digitalWrite(greenLedPin, LOW); // turn OFF after 2 sec
        // digitalWrite(ledPin, LOW);
        digitalWrite(redLedPin, LOW); // off by default
        digitalWrite(buzzerPin, LOW); // off by default
        client.publish(noIssue, "No issue");

        wrongAttempts = 0;
    }
    else if (payload == "ACCESS_DENIED")
    {
        lcd.clear();
        lcd.setRGB(251, 198, 207); // Lilac for denied
        lcd.print("Wrong PIN");
        Serial.println("Access denied → Wrong PIN");
        wrongAttempts++;
        if (wrongAttempts >= 3)
        {
            lcd.clear();
            lcd.setRGB(255, 0, 0); // red for alarm
            lcd.print("ALARM!");
            Serial.println("!!! Alarm triggered after 3 wrong tries !!!");
            // digitalWrite(ledPin, HIGH);    // onboard LED
            digitalWrite(redLedPin, HIGH); // turn on external red LED
            digitalWrite(buzzerPin, HIGH); // turn on buzzer
            client.publish(keypadAlarm, UID); // Send alarm message
        }
    }
}


void reconnect()
{
    while (!client.connected())
    {
        Serial.println("Attempting MQTT connection...");
        if (client.connect("ArduinoClient", mqtt_user, mqtt_pass))
        {
            Serial.println("Connected to MQTT broker");
            client.subscribe(topic);
            Serial.print("Subscribed to topic: ");
            Serial.println(topic);
        }
        else
        {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" → retrying in 2 seconds");
            delay(2000);
        }
    }
}


void setup()
{
    // pinMode(ledPin, OUTPUT);
    pinMode(redLedPin, OUTPUT);
    pinMode(greenLedPin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);


    digitalWrite(greenLedPin, LOW); // off by default
    digitalWrite(redLedPin, LOW);   // off by default
    digitalWrite(buzzerPin, LOW);   // off by default


    Serial.begin(9600); // Start serial monitor
    lcd.begin(16, 2);
    lcd.setRGB(colorR, colorG, colorB); // set backlight to green
    lcd.print("Connecting WiFi");
    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(ssid);

    // Unique Serial ID
    UniqueIDdump(Serial);
	Serial.print("UniqueID: ");
	for (size_t i = 0; i < UniqueIDsize; i++)
	{
        sprintf(&UID[2*i], "%02X", UniqueID[i]);
	}
    UID[20] = '\0';
	Serial.println(UID);


    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("WiFi connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());


    lcd.clear();
    lcd.print("WiFi Connected");


    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
}


void loop()
{
    if (!client.connected())
        reconnect();
    client.loop();


    char key = keypad.getKey();
    if (key)
    {
        Serial.print("Key pressed: ");
        Serial.println(key);


        if (key == '#')
        {
            Serial.print("Publishing PIN: ");
            Serial.println(enteredPin);
            client.publish("access/request", enteredPin.c_str());
            enteredPin = "";
        }
        else
        {
            enteredPin += key;
            // Mask the PIN with '#'
            String masked = "";
            for (int i = 0; i < enteredPin.length(); i++)
                masked += "#";

            lcd.clear();
            lcd.print("PIN: " + masked);

            Serial.print("Current PIN buffer (hidden on LCD): ");
            Serial.println(enteredPin);  // still visible in Serial Monitor for debugging
            //Serial.println(masked);  // Or hiding it in  Serial Monitor

        }
    }
}




