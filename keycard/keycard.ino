//------------------------------------------------------------- KOMMENTAR ------------------------------------------------------------
///////////////////----------------------------- Database: iotdata  Tabeller: users, keycards ------------------------------////
/////////////---------- ADGANGSKONTROL MED RFID-KORT, PINKODER, LCD DISPLAY, MQTT OG DATABASE INTEGRATION -----------///////////////
/*
 # Projekt part: keycards læsning
 # Mål: Adgangskontrolsystem baseret på RFID-kort med validering imod en MariaDB-database via MQTT
 # Beskrivelse:
 #   Dette projekt gør det muligt for brugere at præsentere et RFID-kort. Kortets UID aflæses af
 #   Arduino Uno WiFi Rev2 med en Grove RFID2-læser via I2C. UID’et sendes via WiFi til en
 #   Raspberry Pi MQTT-broker.
 #   En Python-service på Raspberry Pi’en tjekker UID’et imod en MariaDB-database og returnerer
 #   enten ACCESS_GRANTED eller ACCESS_DENIED. Arduino’en viser resultatet på et RGB LCD-display.
 #--------------------------------------------------------------------------------------------------------
 # Komponenter:
 # - Arduino Uno WiFi Rev2
 # - Grove RFID2 (MFRC522 I2C version)
 # - Grove RGB LCD Display
 # - Raspberry Pi 4 med:
 #    - Mosquitto (MQTT broker)
 #    - MariaDB (database med tabeller: users, keycards)
 #    - Python-service (MQTT <-> DB integration)
 #---------------------------------------------------------------------------------------------------------
 # Forbindelser:
 # - RFID2 tilsluttes Grove Base Shield (I2C-port)
 # - RGB LCD Display tilsluttes via I2C (SDA, SCL)
 # - Arduino Uno WiFi Rev2 forbinder via WiFi til Raspberry Pi broker
 #
 # Funktionalitet:
 # - Brugeren lægger RFID-kortet på Grove RFID2-læseren
 # - Arduino aflæser UID og sender det til MQTT-topic `access/request_keycard`
 # - Python-servicen lytter på forespørgslen og slår op i MariaDB:
 #       - Hvis kortet findes og er aktivt -> publicér "ACCESS_GRANTED" til `access/response_keycard`
 #       - Ellers -> publicér "ACCESS_DENIED"
 # - Arduino lytter på "access/response_keycard":
 #       - LCD viser grønt "Adgang Givet" ved godkendelse
 #       - LCD viser rødt "Adgang Nægtet" ved afvisning
 #--------------------------------------------------------------------------------------------------------
 # Database:
 # - Tablen `user` gemmer brugeroplysninger (id, navn, pinkode)
 # - Tablen `keycards` kobler kortnummer til brugere med status aktiv/inaktiv
 #
 # Forfatter: Bob & Ditte
 # Dato: 02.09.2025
 */


//------------------------------------------- BIBLIOTEKER -----------------------------------------------------
#include <Arduino.h>      // Grundlæggende Arduino funktioner
#include <Wire.h>         // I2C kommunikation (til LCD og RFID)
#include <WiFiNINA.h>     // WiFi-funktioner til Arduino Uno WiFi Rev2
#include <PubSubClient.h> // MQTT klientbibliotek
#include "rgb_lcd.h"      // Grove RGB LCD display
#include "MFRC522_I2C.h"  // Grove RFID2 (MFRC522 over I2C)
#include "arduino_secrets.h" // Sensitive Data
#include <ArduinoUniqueID.h> // Get Serial ID


//------------------------------------------- LCD -------------------------------------------------------------
rgb_lcd lcd;                                    // Opretter et LCD-objekt
const int colorR = 0, colorG = 255, colorB = 0; // Standard baggrundsfarve (grøn)


//------------------------------------------- WiFi ------------------------------------------------------------
const char *ssid = SECRET_SSID;
const char *password = SECRET_PASS;


//------------------------------------------- MQTT ------------------------------------------------------------
const char *mqtt_server = "192.168.1.137"; // IP til Raspberry Pi MQTT broker
const int mqtt_port = 1883;               // Standard MQTT port
const char keycard[] = "Alarms/KeyCard";
const char noIssue[] = "Alarms/NoIssue";
const char *mqtt_user = SECRET_MQTT_USER; //User for mqtt setup
const char *mqtt_pass = SECRET_MQTT_PASS; //Password for mqtt setup


//------------------------------------------- RFID ------------------------------------------------------------
// MFRC522_I2C mfrc522(Wire, 0x28);   // Alternativ konstruktor (ikke brugt her)
// #define RST_PIN 5                  // Kun til SPI-versionen af MFRC522, ikke brugt med I2C
MFRC522_I2C mfrc522(0x28, 0xFF, &Wire); // Initialiser RFID læser (0x28 = I2C adresse, 0xFF = ingen reset)


//------------------------------------------- MQTT KLIENT -----------------------------------------------------
WiFiClient wifiClient;           // WiFi klient, bruges til at forbinde MQTT
PubSubClient client(wifiClient); // MQTT klient, baseret på WiFi-forbindelsen

//------------------------------------------- UNIQUE ID AND TRIES ---------------------------------------------
char UID[21]; //Array to hold Unique ID from Arduino
int count = 0; // Tæl hvor mange forsøg der er brugt

//------------------------------------------- CALLBACK --------------------------------------------------------
void callback(char *topic, byte *message, unsigned int length) // Kaldes når en besked modtages på et abonnement
{
    //     String payload;                  // Opretter en String til beskeden
    //     for (int i = 0; i < length; i++) // Samler beskeden byte for byte
    //         payload += (char)message[i];


    //     Serial.print("MQTT Message ["); // Debug: viser topic og payload i Serial Monitor
    //     Serial.print(topic);
    //     Serial.print("]: ");
    //     Serial.println(payload);


    String payload;
    for (int i = 0; i < length; i++)
        payload += (char)message[i];


    Serial.print("MQTT Message received on topic: ");
    Serial.print(topic);
    Serial.print(" | Length: ");
    Serial.print(length);
    Serial.print(" | Payload: ");
    Serial.println(payload);


    lcd.clear();                     // Ryd display
    if (payload == "ACCESS_GRANTED") // Hvis adgang givet
    {
        lcd.setRGB(0, 255, 0);       // Sæt display til grøn
        lcd.print("Access Granted"); // Vis tekst
        count = 0;
        client.publish(noIssue, "No issue");
    }
    else if (payload == "ACCESS_DENIED") // Hvis adgang nægtet
    {
        lcd.setRGB(255, 0, 0);      // Sæt display til rød
        lcd.print("Access Denied"); // Vis tekst
        count++;
        Serial.println(count);
        if (count >= 3) 
        {
            client.publish(keycard, UID); // Send alarm message
            Serial.print("Tried too many times! Sending alarm!");
        }
    }
}


//------------------------------------------- RECONNECT FUNKTION ----------------------------------------------
void reconnect() // Forsøger at forbinde igen til MQTT, hvis forbindelsen ryger
{
    while (!client.connected()) // Så længe vi ikke er forbundet
    {
        Serial.println("Attempting MQTT connection...");
        if (client.connect("ArduinoRFIDClient", mqtt_user, mqtt_pass)) // Forsøger at forbinde som "ArduinoRFIDClient"
        {
            Serial.println("Connected to broker");
            client.subscribe("access/response_keycard"); // Abonnerer på svar fra Python
        }
        else
        {
            Serial.print("Failed, rc="); // Viser fejlstatus
            Serial.print(client.state());
            delay(2000); // Vent lidt før nyt forsøg
        }
    }
}


//------------------------------------------- SETUP -----------------------------------------------------------
void setup()
{
    Serial.begin(9600); // Start seriemonitor
    Wire.begin();       // Start I2C bus

    // Unique Serial ID
    UniqueIDdump(Serial);
	Serial.print("UniqueID: ");
	for (size_t i = 0; i < UniqueIDsize; i++)
	{
        sprintf(&UID[2*i], "%02X", UniqueID[i]);
	}
    UID[20] = '\0';
	Serial.println(UID);

    // LCD setup
    lcd.begin(16, 2);                   // Initialiser LCD (16x2)
    lcd.setRGB(colorR, colorG, colorB); // Sæt standard farve (grøn)
    lcd.print("Connecting WiFi");       // Info til bruger


    // WiFi forbindelse
    WiFi.begin(ssid, password);           // Forbind til netværk
    while (WiFi.status() != WL_CONNECTED) // Vent til WiFi er forbundet
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected!");
    lcd.clear();
    lcd.print("Scan RFID card"); // Klar til scanning


    // MQTT setup
    client.setServer(mqtt_server, mqtt_port); // Indstil broker
    client.setCallback(callback);             // Sæt callback funktion


    // RFID setup
    mfrc522.PCD_Init(); // Initialiser RFID læser
    Serial.println("Place your RFID card...");


    // Abonner KUN én gang her
    // client.subscribe("access/response_keycard");
}


//------------------------------------------- LOOP ------------------------------------------------------------
void loop()
{
    if (!client.connected()) // Hvis MQTT er frakoblet
        reconnect();         // Forsøg at forbinde igen
    client.loop();           // Hold MQTT kørende


    if (!mfrc522.PICC_IsNewCardPresent()) // Hvis intet kort er til stede
        return;
    if (!mfrc522.PICC_ReadCardSerial()) // Hvis kort ikke kunne læses
        return;


    // Byg kortets UID som streng
    String cardID = "";
    for (byte i = 0; i < mfrc522.uid.size; i++)
    {
        if (mfrc522.uid.uidByte[i] < 0x10) // Tilføj et "0" foran hvis < 0x10
            cardID += "0";
        cardID += String(mfrc522.uid.uidByte[i], HEX); // Tilføj byte som HEX
    }
    cardID.toLowerCase(); // Konverter til små bogstaver


    Serial.print("Card detected: "); // Debug: vis kort-ID
    Serial.println(cardID);


    lcd.clear();         // Ryd display
    lcd.print("Card: "); // Vis kort
    lcd.setCursor(0, 1);
    lcd.print(cardID);


    // Send kort-ID til MQTT
    client.publish("access/request_keycard", cardID.c_str());


    delay(1000); // Vent 1 sekund før næste læsning
}
