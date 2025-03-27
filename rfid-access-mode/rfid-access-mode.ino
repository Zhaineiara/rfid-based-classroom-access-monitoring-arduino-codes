/*
ACCESS MODE CODE

Connections:
RFID:
SDA -5
SCK - 18
MOSI - 23
MISO - 19
GND - GND(ESP)
RST - 22
3.3V - 3.3V(ESP)

RELAY:
5V - 5V(BB)
GND - GND(BB)
S - 27
NO - - (SOLENOID)
COM - + (SOLENOID)
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#define RST_PIN 22
#define SS_PIN 5
#define RELAY_PIN 27

String room_status = "Lock";
String activeUser = "";  // Track the active user

const char* ssid = "Fake Wi";                      // WiFi SSID
const char* password = "Aa1231325213!";            // WiFi Password
const char* accessUrl = "http://192.168.68.235:3000/rfids";  // API endpoint

MFRC522 rfid(SS_PIN, RST_PIN);
hd44780_I2Cexp lcd;
HTTPClient accesshttp;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Ensure the relay starts in a locked state

  Wire.begin(21, 17);  // SDA = GPIO 21, SCL = GPIO 17
  lcd.begin(16, 2);    // 16 columns, 2 rows
  lcd.clear();
  lcd.print("TUP System");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();

  wifiConnect(ssid, password);

  lcd.clear();
  lcd.print("Please scan");
  lcd.setCursor(0, 1);
  lcd.print("your RFID Card!");
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return;

  // Read UID from the scanned card
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  Serial.println("Card UID: " + uid);

  lcd.clear();
  lcd.print("ACCESS MODE");
  accessMode(uid);  // Check access mode
}

void wifiConnect(String wifiName, String wifiPassword) {
  WiFi.begin(wifiName, wifiPassword);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("Wi-Fi...");
  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  lcd.clear();
  lcd.print("Wi-Fi Connected");
  Serial.println("\nConnected to Wi-Fi!");
  delay(2000);
}

int room_id = 1;

void accessMode(String cardUid) {
  lcd.clear();
  lcd.print("Getting Card");
  lcd.setCursor(0, 1);
  lcd.print("Information...");
  delay(1000);

  if (WiFi.status() == WL_CONNECTED) {
    accesshttp.begin(accessUrl);
    accesshttp.addHeader("Content-Type", "application/json");
    accesshttp.addHeader("Authorization", "Bearer 6dbe948bb56f1d6827fbbd8321c7ad14");  // Use Bearer format

    String payload = String("{\"uid\":\"") + cardUid + 
                     String("\",\"room_number\":\"") + room_id + 
                     String("\",\"room_status\":\"") + room_status + 
                     String("\",\"api_token\":\"") + "6dbe948bb56f1d6827fbbd8321c7ad14" + String("\"}");

    int httpResponseCode = accesshttp.POST(payload);
    Serial.println("Payload sent: " + payload);
    Serial.println("HTTP Response Code: " + String(httpResponseCode));

    if (httpResponseCode == 401) {
      // Unauthorized: Card not registered
      Serial.println("Unauthorized access! Card not registered.");
      
      lcd.clear();
      lcd.print("Not Registered!");
      lcd.setCursor(0, 1);
      lcd.print("Access Denied");
      delay(2000);

    } else if (httpResponseCode == 403) {
      // Forbidden: Card inactive
      Serial.println("Access denied! Card is inactive.");
      
      lcd.clear();
      lcd.print("Card Inactive!");
      lcd.setCursor(0, 1);
      lcd.print("Access Denied");
      delay(2000);

    } else if (httpResponseCode == 200) {
      // Success: Card is valid and active
      String response = accesshttp.getString();
      Serial.println("Server Response: " + response);

      if (response.indexOf("\"unlock\":true") != -1 && digitalRead(RELAY_PIN) == LOW) {
        // Unlock the door
        room_status = "Unlock";
        activeUser = cardUid;  
        digitalWrite(RELAY_PIN, HIGH);

        lcd.clear();
        lcd.print("Access Granted!");
        delay(1000);
        lcd.clear();
        lcd.print("Welcome!");
        lcd.setCursor(0, 1);
        lcd.print(activeUser);
        Serial.println("Access granted! Unlocking solenoid...");
        delay(2000);
        lcd.clear();
        lcd.print("Room " + String(room_id));
        lcd.setCursor(0, 1);
        lcd.print("Unlocked!");

      } else if (response.indexOf("\"lock\":true") != -1 && digitalRead(RELAY_PIN) == HIGH) {
        // Lock the door
        if (cardUid == activeUser) {
          room_status = "Lock";
          activeUser = "";
          digitalWrite(RELAY_PIN, LOW);

          lcd.clear();
          lcd.print("Goodbye!");
          lcd.setCursor(0, 1);
          lcd.print(cardUid);
          Serial.println("Access granted! Locking solenoid...");
          delay(2000);
          lcd.clear();
          lcd.print("Room " + String(room_id));
          lcd.setCursor(0, 1);
          lcd.print("Locked!");
        } else {
          Serial.println("Access denied! Only active user can time out.");
          lcd.clear();
          lcd.print("Access Denied!");
          lcd.setCursor(0, 1);
          lcd.print("Room occupied");
          delay(2000);
        }
      } else {
        Serial.println("Access denied! Invalid response.");
        lcd.clear();
        lcd.print("Access Denied!");
        lcd.setCursor(0, 1);
        lcd.print("Invalid Response");
      }

    } else {
      // Error with POST request
      Serial.println("Error sending POST request");
      lcd.clear();
      lcd.print("POST Failed!");
      lcd.setCursor(0, 1);
      lcd.print("Try Again");
      delay(2000);
    }

    accesshttp.end();
  }

  rfid.PICC_HaltA();
  delay(2000);
}