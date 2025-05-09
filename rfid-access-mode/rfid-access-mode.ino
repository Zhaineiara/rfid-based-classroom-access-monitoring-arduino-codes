/*
ACCESS MODE CODE - WITH POWER OUTAGE RESILIENCE AND 12 DIGITS CARD REFERENCES NUMBER

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

no pin of relay == open circuit
when energized == it closes the circuit. on solenoid.
room status is unlock = high relay, open solenoid

when deenergized == it open the circuit. off solenoid.
room status is lock = low relay, off solenoid

on relay = nakalabas solenoid
off relay = nakapasok solenoid

Additional feature:
  When there's a power outage, the system stores its previous state 
  (relay state, room status, and active user) in non-volatile storage.
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <Preferences.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

/*Start of changeable variables*/
/*Make sure it is on the same network, change the 4th octet only*/
IPAddress local_IP(192, 168, 0, 101);
int room_id = 1;
/*End of changeable variables*/

IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
const char* ssid = "RFID_system";
const char* password = "rfidsystem";
const char* accessUrl = "http://192.168.0.100:3000/rfids";

#define RST_PIN 22
#define SS_PIN 5
#define RELAY_PIN 27
#define BUTTON_PIN 14

String room_status;
String activeUser;
bool relayState;
bool lastButtonState = HIGH;

MFRC522 rfid(SS_PIN, RST_PIN);
hd44780_I2Cexp lcd;
HTTPClient accesshttp;
Preferences preferences;
AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  
  // Initialize preferences
  preferences.begin("access-system", false); // false = read/write mode

  // Uncomment this is there's an issue about data stored in preference
  preferences.clear(); // ← This wipes all saved preferences
  preferences.end();

  // Load saved state
  loadSystemState();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, relayState);  // Restore relay state

  // Initialize button input with internal pullup 
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(21, 17);
  lcd.begin(16, 2);
  lcd.clear();
  delay(50);
  lcd.print("RFID ROOM SYSTEM");
  lcd.setCursor(0, 1);
  lcd.print("2024-2025");
  Serial.print(relayState ? "Unlocked" : "Locked");
  delay(2000);
  lcd.clear();
  delay(50);

  wifiConnect(ssid, password);

  // Setup web server endpoint for relay control
  setupWebServer();

  lcd.clear();
  delay(50);
  lcd.print("Please scan");
  lcd.setCursor(0, 1);
  lcd.print("your RFID Card!");
}

void loop() {
  static int failedReads = 0;

  // Check manual override button using edge detection
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH) {  // Falling edge detected
    // Toggle the relay output only
    if (digitalRead(RELAY_PIN) == HIGH) {
      digitalWrite(RELAY_PIN, LOW);
      lcd.clear();
      delay(50);
      lcd.print("Button Pressed");
      lcd.setCursor(0, 1);
      lcd.print("Room Locked!");
    } else {
      digitalWrite(RELAY_PIN, HIGH);
      lcd.clear();
      delay(50);
      lcd.print("Button Pressed");
      lcd.setCursor(0, 1);
      lcd.print("Room Unlocked!");
    }
    delay(1000);  // Debounce delay for the button press
  }
  lastButtonState = currentButtonState;

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
    delay(50);
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
  delay(50);
  lcd.print("Wi-Fi Connecting....");
  Serial.print("Connecting Wi-Fi");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear();
    delay(50);
    lcd.print("Wi-Fi Connected");
    Serial.println("\nConnected to Wi-Fi!");
  } else {
    lcd.clear();
    delay(50);
    lcd.print("Wi-Fi Failed!");
    Serial.println("\nWi-Fi connection failed!");
  }
  delay(2000);
}

void accessMode(String cardUid) {
  HTTPClient http;

  lcd.clear();
  delay(50);
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

    if (httpResponseCode == 403) {
      lcd.clear();
      delay(50);
      lcd.print("Room Occupied");
      lcd.setCursor(0, 1);
      lcd.print("Access Denied");
      delay(2000);

    } else if (httpResponseCode == 401) {
      lcd.clear();
      delay(50);
      lcd.print("Not Registered!");
      lcd.setCursor(0, 1);
      lcd.print("Access Denied");
      delay(2000);

    } else if (httpResponseCode == 409) {
      lcd.clear();
      delay(50);
      lcd.print("Inactive Card!");
      lcd.setCursor(0, 1);
      lcd.print("Access Denied");
      delay(2000);

    } else if (httpResponseCode == 200) {
      String response = accesshttp.getString();
      Serial.println("Server Response: " + response);

      // — parse the user name —
      const String userKey = "\"user\":\"";
      int uPos = response.indexOf(userKey);
      String ownerName = "";
      if (uPos != -1) {
        int uStart = uPos + userKey.length();
        int uEnd   = response.indexOf("\"", uStart);
        ownerName  = response.substring(uStart, uEnd);
      }

      // — parse the room_num field —
      const String roomKey = "\"room_num\":\"";
      int rPos = response.indexOf(roomKey);
      String roomText = "";
      if (rPos != -1) {
        int rStart   = rPos + roomKey.length();
        int rEnd     = response.indexOf("\"", rStart);
        roomText     = response.substring(rStart, rEnd);
      }

      if (response.indexOf("Recovered session") != -1) {
        lcd.clear();
        delay(50);
        lcd.print("Session Fixed!");
        lcd.setCursor(0, 1);
        lcd.print("Now Locked");
        delay(2000);
      }


      if (response.indexOf("\"unlock\":true") != -1 && !relayState) {
        room_status = "Unlock";
        activeUser = cardUid;
        relayState = true;
        saveSystemState();
        digitalWrite(RELAY_PIN, HIGH);

        lcd.clear();
        delay(50);
        lcd.print("Access Granted!");
        delay(1000);
        lcd.clear();
        delay(50);
        lcd.print("Welcome! Prof.");
        lcd.setCursor(0, 1);
        lcd.print(ownerName);
        delay(2000);
        lcd.clear();
        delay(50);
        lcd.print(roomText);
        lcd.setCursor(0, 1);
        lcd.print("Unlocked!");

        // *** NEW: Auto-lock after 10 seconds with a countdown display ***
        for (int i = 10; i > 0; i--) {
          lcd.clear();
          delay(50);
          lcd.print("Auto-lock in:");
          lcd.setCursor(0, 1);
          lcd.print(i);  // Shows the countdown number
          delay(1000);   // Wait 1 second
        }

        room_status = "Unlock";
        activeUser = cardUid;
        relayState = false;
        saveSystemState();
        digitalWrite(RELAY_PIN, LOW);  // Disengage relay (lock room)
        lcd.clear();
        delay(50);
        lcd.print(roomText + " Locked!");
        lcd.setCursor(0, 1);
        lcd.print("Room Occupied");

        delay(500);

      } else if (response.indexOf("\"lock\":true") != -1 && !relayState) {
        if (cardUid == activeUser) {
          room_status = "Lock";
          activeUser = "";
          relayState = false;
          saveSystemState();
          digitalWrite(RELAY_PIN, LOW);

          lcd.clear();
          delay(50);
          lcd.print("Goodbye! Prof.");
          lcd.setCursor(0, 1);
          lcd.print(ownerName);
          delay(2000);
          lcd.clear();
          delay(50);
          lcd.print(roomText + " Locked!");
          lcd.setCursor(0, 1);
          lcd.print("Room Available");
        } else {
          lcd.clear();
          delay(50);
          lcd.print("Room occupied");
          lcd.setCursor(0, 1);
          lcd.print("Access Denied!");
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

  // Safety: ensure only Lock/Unlock values are used
  if (room_status != "Lock" && room_status != "Unlock") {
    Serial.println("Corrupt room_status detected — resetting to Lock.");
    room_status = "Lock";
    relayState = false;
    activeUser = "";
    saveSystemState();
  }

  // ✅ Safety check
  if (relayState && activeUser == "") {
    // Relay says "open" but no active user? Force reset to locked.
    Serial.println("Invalid state detected — forcing Lock.");
    relayState = false;
    room_status = "Lock";
    preferences.putBool("relay_state", relayState);
    preferences.putString("room_status", room_status);
  }

  Serial.println("✅ Loaded system state:");
  Serial.println("Room Status: " + room_status);
  Serial.println("Active User: " + activeUser);
  Serial.println("Relay State: " + String(relayState ? "HIGH" : "LOW"));
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

void setupWebServer() {
  server.on("/relay", HTTP_POST, [](AsyncWebServerRequest *request) {
    if(!request->hasHeader("Authorization")) {
      request->send(401);
      return;
    }

    String auth = request->header("Authorization");
    if(auth != "Bearer 6dbe948bb56f1d6827fbbd8321c7ad14") {
      request->send(401);
      return;
    }

    AsyncWebParameter* p = request->getParam("state", true);
    if(p) {
      bool newState = (p->value() == "true");
      digitalWrite(RELAY_PIN, newState ? HIGH : LOW);
      relayState = newState;
      saveSystemState();
      
      request->send(200, "application/json", "{\"success\":true,\"state\":" + String(newState ? "true" : "false") + "}");
    } else {
      request->send(400);
    }
  });

  server.begin();
}