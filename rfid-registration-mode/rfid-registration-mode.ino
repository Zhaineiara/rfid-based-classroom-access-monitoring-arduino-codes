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

// Wi-Fi and Server Settings
const char* ssid = "Zhaine";
const char* password = "mapa12345";
const char* registerUrl = "http://192.168.68.235:3000/card_scan";  // Change to "https://" if using SSL
const char* authToken = "6dbe948bb56f1d6827fbbd8321c7ad14";

// Hardware instances
MFRC522 rfid(SS_PIN, RST_PIN);
hd44780_I2Cexp lcd;
HTTPClient httpClient;

// Variables for debouncing the card read
unsigned long lastCardTime = 0;
const unsigned long debounceDelay = 2000; // 2 seconds

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  // Initialize I2C and LCD (SDA = GPIO 21, SCL = GPIO 17)
  Wire.begin(21, 17);
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("TUP System");
  lcd.setCursor(0, 1);
  lcd.print("Initialized!");
  delay(2000);
  lcd.clear();

  // Connect to Wi-Fi
  connectToWiFi();

  lcd.clear();
  lcd.print("Please scan");
  lcd.setCursor(0, 1);
  lcd.print("your RFID Card!");
}

void loop() {
  // Check Wi-Fi connection and attempt reconnect if needed
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  // Check for a new card. If not present, exit the loop iteration.
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return;

  // Debounce: ensure at least 'debounceDelay' milliseconds between card reads
  if (millis() - lastCardTime < debounceDelay) {
    rfid.PICC_HaltA();
    return;
  }
  lastCardTime = millis();

  // Read card UID and convert to uppercase hexadecimal string
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0"; // Prepend a zero for single-digit values
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  // Pad with leading zeros until the UID string is at least 12 digits long
  while (uid.length() < 12) {
    uid = "0" + uid;
  }
  
  Serial.println("Card UID: " + uid);

  // Attempt to register the card
  registerCard(uid);

  // Halt the card and reset the reader for the next scan
  rfid.PICC_HaltA();
}

void connectToWiFi() {
  lcd.clear();
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("Wi-Fi...");
  Serial.print("Connecting to Wi-Fi");

  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  // Try to connect for up to 10 seconds
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear();
    lcd.print("Wi-Fi Connected");
    Serial.println("\nConnected to Wi-Fi!");
    delay(1000);
  } else {
    lcd.clear();
    lcd.print("Wi-Fi Failed");
    Serial.println("\nWi-Fi connection failed.");
    delay(2000);
  }
}

void registerCard(String cardUid) {
  lcd.clear();
  lcd.print("Getting Card Info");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");
  delay(500);

  if (WiFi.status() == WL_CONNECTED) {
    httpClient.begin(registerUrl);
    httpClient.addHeader("Content-Type", "application/json");
    httpClient.addHeader("Authorization", authToken);

    String payload = String("{\"uid\":\"") + cardUid + String("\"}");
    int httpResponseCode = httpClient.POST(payload);

    Serial.println("Payload sent: " + payload);

    if (httpResponseCode > 0) {
      String response = httpClient.getString();
      Serial.println("HTTP Response Code: " + String(httpResponseCode));
      Serial.println("Server Response: " + response);

      if (response.indexOf("\"success\":true") != -1) {
        lcd.clear();
        lcd.print("Registration");
        lcd.setCursor(0, 1);
        lcd.print("Successful!");
      } else if (response.indexOf("\"success\":false") != -1) {
        lcd.clear();
        lcd.print("Card Already");
        lcd.setCursor(0, 1);
        lcd.print("Registered!");
      } else {
        lcd.clear();
        lcd.print("Unknown Resp.");
      }
    } else {
      Serial.println("Error sending POST request. HTTP error code: " + String(httpResponseCode));
      lcd.clear();
      lcd.print("POST Request Err");
    }
    httpClient.end();
  } else {
    lcd.clear();
    lcd.print("No Wi-Fi Conn");
    Serial.println("Wi-Fi not connected, cannot register card.");
  }

  delay(2000);
  lcd.clear();
  lcd.print("Please scan");
  lcd.setCursor(0, 1);
  lcd.print("your RFID Card!");
}