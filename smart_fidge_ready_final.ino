#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include "DHT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WiFi setup
#define WIFI_SSID "Felix"  // Change to your Wi-Fi SSID
#define WIFI_PASSWORD "sayantan"  // Change to your Wi-Fi password

// DHT11 Sensor setup
#define DHTPIN D5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// MQ2 Gas Sensor setup
#define MQ2_PIN A0  // MQ2 sensor connected to analog pin A0

// Relay setup
#define RELAY_PIN D4  // Relay connected to digital pin D6

// Firebase setup
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Set the LCD address to 0x27 for a 16 chars and 2-line display

// Variables for LCD display update timing
unsigned long lastUpdate = 0;
int displayState = 0;  // 0: Temperature, 1: Humidity, 2: Gas

void setup() {
  Serial.begin(115200);
  
  // Initialize DHT sensor
  dht.begin(); 
  
  // Initialize relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Set relay to off by default
  
  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Connected to Wi-Fi. IP Address: ");
  Serial.println(WiFi.localIP());

  // Setup Firebase
  firebaseConfig.host = "iembotics-default-rtdb.firebaseio.com";  // Replace with your Firebase project URL
  firebaseConfig.signer.tokens.legacy_token = "jLUqSu7FodpwQBXMK7xaF8bcOZvkPKNZ2JigNoWa";  // Replace with Firebase database secret
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);

  // Initialize I2C LCD
  Wire.begin(D2, D1);  // SDA = D2 (GPIO4), SCL = D1 (GPIO5)
  lcd.begin(16, 2);  
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  // Show Wi-Fi connection success on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi connected");
  
  // Define degree symbol (°)
  byte degreeSymbol[8] = {
    0b00000,
    0b00110,
    0b00110,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000
  };
  lcd.createChar(0, degreeSymbol);  // Create the degree symbol as character 0
}

void loop() {
  // 1. Sensor Reading and Data Sending to Firebase

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int gasValue = analogRead(MQ2_PIN);

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Print sensor readings to Serial
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print("%  Temperature: ");
  Serial.print(temperature);
  Serial.print("°C  Gas Sensor Value: ");
  Serial.println(gasValue);

  // Send data to Firebase
  if (Firebase.setFloat(firebaseData, "/Temp", temperature)) {
    Serial.println("Temperature data sent successfully");
  } else {
    Serial.println("Failed to send temperature data");
    Serial.println(firebaseData.errorReason());
  }

  if (Firebase.setFloat(firebaseData, "/Humidity", humidity)) {
    Serial.println("Humidity data sent successfully");
  } else {
    Serial.println("Failed to send humidity data");
    Serial.println(firebaseData.errorReason());
  }

  if (Firebase.setInt(firebaseData, "/Gas", gasValue)) {
    Serial.println("Gas sensor data sent successfully");
  } else {
    Serial.println("Failed to send gas sensor data");
    Serial.println(firebaseData.errorReason());
  }

  // 2. Fetch Data from Firebase and Display on LCD

  unsigned long currentTime = millis();
  if (currentTime - lastUpdate >= 2000) {  // Update LCD every 2 seconds
    lastUpdate = currentTime;
    displayState = (displayState + 1) % 3;  // Cycle through 0: Temp, 1: Humidity, 2: Gas

    if (displayState == 0) {
      // Fetch and display temperature
      if (Firebase.get(firebaseData, "/FirebaseOK/LCD")) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Temperature:");
        lcd.setCursor(0, 1);
        String tempStr = firebaseData.stringData();  // Fetch temperature as a string
        lcd.print(tempStr); // Display string on LCD
        lcd.write((byte)0); // Display degree symbol
        lcd.print(" C");
        Serial.println("Temperature displayed on LCD.");

        // Convert string to integer
        int tempValue = tempStr.toInt();
        
        // Control the relay based on temperature value
        if (tempValue < 20) {
          digitalWrite(RELAY_PIN, HIGH);  // Turn on the relay
          Serial.println("Relay ON");
        } else {
          digitalWrite(RELAY_PIN, LOW);   // Turn off the relay
          Serial.println("Relay OFF");
        }
      } else {
        displayError();
      }
    } else if (displayState == 1) {
      // Fetch and display humidity
      if (Firebase.get(firebaseData, "/Humidity")) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Humidity:");
        lcd.setCursor(0, 1);
        lcd.print(firebaseData.stringData()); // Use stringData() for generic data type
        lcd.print(" %");
        Serial.println("Humidity displayed on LCD.");
      } else {
        displayError();
      }
    } else if (displayState == 2) {
      // Fetch and display gas level
      if (Firebase.get(firebaseData, "/Gas")) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Gas Level:");
        lcd.setCursor(0, 1);
        lcd.print(firebaseData.stringData()); // Use stringData() for generic data type
        Serial.println("Gas level displayed on LCD.");
      } else {
        displayError();
      }
    }
  }

  delay(3000);  // Delay before next sensor reading and data send
}

// Function to display Firebase fetch error
void displayError() {
  Serial.print("Failed to get data: ");
  Serial.println(firebaseData.errorReason());
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Firebase Error");
}
