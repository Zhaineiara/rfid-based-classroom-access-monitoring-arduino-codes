/*
REGISTRATION MODE

Connections:
RFID:
SDA -5
SCK - 18
MOSI - 23
MISO - 19
GND - GND(ESP)
RST - 22
3.3V - 3.3V(ESP)
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

 
const char* ssid = "Zhaine";
const char* password = "mapa12345";
const char* registerUrl = "http://192.168.83.42:3000/card_scan";

MFRC522 rfid(SS_PIN, RST_PIN);

hd44780_I2Cexp lcd;
HTTPClient registerhttp;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  Wire.begin(21, 17);  // SDA = GPIO 21, SCL = GPIO 17
  lcd.begin(16, 2);    // 16 columns, 2 rows
  lcd.clear();
  lcd.print("TUP System");
  lcd.setCursor(0, 1);
  lcd.print("Initialized!");
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

  String uid = "";

    lcd.clear();
    lcd.print("REGISTRATION MODE");
    lcd.setCursor(0, 1);
    lcd.print("MODE");
    delay(1000);

    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    Serial.println("Card UID: " + uid);
    registerMode(uid);
}


void wifiConnect(String wifiName, String wifiPassword) {
  WiFi.begin(wifiName, wifiPassword);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("internet...");
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

void registerMode(String cardUid) {
  lcd.clear();
  lcd.print("Getting Card");
  lcd.setCursor(0, 1);
  lcd.print("Information");
  delay(1000);

  if (WiFi.status() == WL_CONNECTED) {
    registerhttp.begin(registerUrl);
    registerhttp.addHeader("Content-Type", "application/json");
    registerhttp.addHeader("Authorization", "6dbe948bb56f1d6827fbbd8321c7ad14");

    String payload = String("{\"uid\":\"") + cardUid + String("\"}");

    int httpResponseCode = registerhttp.POST(payload);
    Serial.println("Payload sent: " + payload);

    if (httpResponseCode > 0) {
      String response = registerhttp.getString();
      Serial.println("HTTP Response Code: " + String(httpResponseCode));
      Serial.println("Server Response: " + response);

      if (response.indexOf("\"success\":true") != -1) {
        delay(1000);
        lcd.clear();
        lcd.print("Success");
        delay(1000);
      } else if (response.indexOf("\"success\":false") != -1) {
        lcd.clear();
        lcd.print("The card is");
        lcd.setCursor(0, 1);
        lcd.print("already in use");
        delay(1000);
      }
    } else {
      Serial.println("Error sending POST request");
      lcd.clear();
      lcd.print("POST Request Err");
    }
    registerhttp.end();
  }

  rfid.PICC_HaltA();
  delay(2000);
}