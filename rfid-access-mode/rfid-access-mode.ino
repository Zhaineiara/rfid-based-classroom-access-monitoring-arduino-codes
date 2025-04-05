/*
ACCESS MODE CODE - WITH POWER OUTAGE RESILIENCE

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

LCD CONNECTION:
VCC - 5V (BB)
GND - (BB)
SDA - 21 (ESP)
SCL - 17 (ESP)
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <Preferences.h>

/*Start of changeable variables*/
/*Make sure it is on the same network, change the 4th octet only*/
IPAddress local_IP(192, 168, 68, 101);
int room_id = 2;
/*End of changeable variables*/

IPAddress gateway(192, 168, 68, 1);
IPAddress subnet(255, 255, 255, 0);
const char* ssid = "Fake Wi";
const char* password = "Aa1231325213!";
const char* accessUrl = "http://192.168.68.235:3000/rfids";

#define RST_PIN 22
#define SS_PIN 5
#define RELAY_PIN 27

String room_status;
String activeUser;
bool relayState;

MFRC522 rfid(SS_PIN, RST_PIN);
hd44780_I2Cexp lcd;
HTTPClient accesshttp;
Preferences preferences;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  
  // Initialize preferences
  preferences.begin("access-system", false); // false = read/write mode
  
  // Load saved state
  loadSystemState();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, relayState);  // Restore relay state

  Wire.begin(21, 17);
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("TUP System");
  lcd.setCursor(0, 1);
  lcd.print(relayState ? "Unlocked" : "Locked");
  delay(2000);
  lcd.clear();

  wifiConnect(ssid, password);

  lcd.clear();
  lcd.print("Please scan");
  lcd.setCursor(0, 1);
  lcd.print("your RFID Card!");
}

void loop() {
  static int failedReads = 0;

  if (!rfid.PICC_IsNewCardPresent()) {
    failedReads++;
    Serial.println("No new card detected.");
  } else if (!rfid.PICC_ReadCardSerial()) {
    failedReads++;
    Serial.println("Failed to read card serial.");
  } else {
    failedReads = 0;

    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) {
        uid += "0";
      }
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();

    // Pad the UID to 12 digits if needed
    while(uid.length() < 12) {
      uid = "0" + uid;
    }

    Serial.println("Card UID: " + uid);

    lcd.clear();
    lcd.print("ACCESS MODE");
    accessMode(uid);
    return;
  }

  if (failedReads > 3) {
    Serial.println("RFID reinitializing after repeated failures...");
    rfid.PCD_Init();
    failedReads = 0;
  }

  delay(500);
}

void wifiConnect(String wifiName, String wifiPassword) {
  WiFi.begin(wifiName.c_str(), wifiPassword.c_str());
  lcd.clear();
  lcd.print("Connecting to Wi-Fi");
  Serial.print("Connecting to Wi-Fi");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear();
    lcd.print("Wi-Fi Connected");
    Serial.println("\nConnected to Wi-Fi!");
  } else {
    lcd.clear();
    lcd.print("Wi-Fi Failed!");
    Serial.println("\nWi-Fi connection failed!");
  }
  delay(2000);
}

void accessMode(String cardUid) {
  HTTPClient http;

  lcd.clear();
  lcd.print("Getting Card");
  lcd.setCursor(0, 1);
  lcd.print("Information...");
  delay(1000);

  String formattedUid = cardUid;
  while(formattedUid.length() < 12) {
    formattedUid = "0" + formattedUid;
  }
  formattedUid.toUpperCase();

  Serial.println("Card UID: " + formattedUid);

  if (WiFi.status() == WL_CONNECTED) {
    accesshttp.begin(accessUrl);
    accesshttp.addHeader("Content-Type", "application/json");
    accesshttp.addHeader("Authorization", "Bearer 6dbe948bb56f1d6827fbbd8321c7ad14");

    String payload = String("{\"uid\":\"") + cardUid + 
                     String("\",\"room_number\":\"") + room_id + 
                     String("\",\"room_status\":\"") + room_status + 
                     String("\",\"api_token\":\"") + "6dbe948bb56f1d6827fbbd8321c7ad14" + String("\"}");

    int httpResponseCode = accesshttp.POST(payload);
    Serial.println("Payload sent: " + payload);
    Serial.println("HTTP Response Code: " + String(httpResponseCode));

    if (httpResponseCode == 401) {
      lcd.clear();
      lcd.print("Not Registered!");
      lcd.setCursor(0, 1);
      lcd.print("Access Denied");
      delay(2000);

    } else if (httpResponseCode == 403) {
      lcd.clear();
      lcd.print("Card Inactive!");
      lcd.setCursor(0, 1);
      lcd.print("Access Denied");
      delay(2000);

    } else if (httpResponseCode == 200) {
      String response = accesshttp.getString();
      Serial.println("Server Response: " + response);

      if (response.indexOf("\"unlock\":true") != -1 && !relayState) {
        room_status = "Unlock";
        activeUser = cardUid;
        relayState = true;
        saveSystemState();
        digitalWrite(RELAY_PIN, HIGH);

        lcd.clear();
        lcd.print("Access Granted!");
        delay(1000);
        lcd.clear();
        lcd.print("Welcome!");
        lcd.setCursor(0, 1);
        lcd.print(activeUser);
        delay(2000);
        lcd.clear();
        lcd.print("Room " + String(room_id));
        lcd.setCursor(0, 1);
        lcd.print("Unlocked!");

      } else if (response.indexOf("\"lock\":true") != -1 && relayState) {
        if (cardUid == activeUser) {
          room_status = "Lock";
          activeUser = "";
          relayState = false;
          saveSystemState();
          digitalWrite(RELAY_PIN, LOW);

          lcd.clear();
          lcd.print("Goodbye!");
          lcd.setCursor(0, 1);
          lcd.print(cardUid);
          delay(2000);
          lcd.clear();
          lcd.print("Room " + String(room_id));
          lcd.setCursor(0, 1);
          lcd.print("Locked!");
        } else {
          lcd.clear();
          lcd.print("Access Denied!");
          lcd.setCursor(0, 1);
          lcd.print("Room occupied");
          delay(2000);
        }
      }
    }
    accesshttp.end();
  }

  rfid.PICC_HaltA();
  delay(2000);
}

void loadSystemState() {
  room_status = preferences.getString("room_status", "Lock");
  activeUser = preferences.getString("active_user", "");
  relayState = preferences.getBool("relay_state", false);

  // ✅ Safety check
  if (relayState && activeUser == "") {
    // Relay says "open" but no active user? Force reset to locked.
    Serial.println("Invalid state detected — forcing Lock.");
    relayState = false;
    room_status = "Lock";
    preferences.putBool("relay_state", relayState);
    preferences.putString("room_status", room_status);
  }
}

void saveSystemState() {
  preferences.putString("room_status", room_status);
  preferences.putString("active_user", activeUser);
  preferences.putBool("relay_state", relayState);
  
  Serial.println("Saved system state:");
  Serial.println("Room Status: " + room_status);
  Serial.println("Active User: " + activeUser);
  Serial.println("Relay State: " + String(relayState ? "HIGH" : "LOW"));
}