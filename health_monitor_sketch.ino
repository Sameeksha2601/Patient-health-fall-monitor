/*
  IoT-Based Patient Health & Fall Monitoring System
  ----------------------------------------------------
  Final Year Mini Project - ECE Department

  Description:
  This system continuously monitors a patient's body temperature using a
  DS18B20 sensor and detects sudden falls using an MPU6050 accelerometer.
  On detecting a fall or abnormal temperature, a local buzzer sounds and
  the reading is flagged before being uploaded to ThingSpeak for remote
  monitoring by caregivers.

  Hardware:
    - ESP32 Dev Board
    - DS18B20 Digital Temperature Sensor (with 4.7k pull-up resistor)
    - MPU6050 Accelerometer/Gyroscope (I2C)
    - Buzzer (active)
    - 16x2 I2C LCD (optional local display)

  Wiring:
    - DS18B20 data pin -> GPIO 4  (with 4.7k ohm pull-up to 3.3V)
    - MPU6050 SDA -> GPIO 21, SCL -> GPIO 22 (default I2C pins)
    - Buzzer -> GPIO 25
    - LCD SDA -> GPIO 21, SCL -> GPIO 22 (shares I2C bus with MPU6050)

  Libraries required:
    - OneWire
    - DallasTemperature
    - Adafruit MPU6050
    - Adafruit Unified Sensor
    - LiquidCrystal_I2C
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <LiquidCrystal_I2C.h>

// ---------- USER CONFIG ----------
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// ThingSpeak channel settings (create a free channel at thingspeak.com)
const char* THINGSPEAK_API_KEY = "YOUR_WRITE_API_KEY";
const char* THINGSPEAK_SERVER  = "http://api.thingspeak.com/update";
// ----------------------------------

#define ONE_WIRE_BUS 4
#define BUZZER_PIN 25

// Fall detection: total acceleration magnitude thresholds (in g)
const float FREE_FALL_THRESHOLD = 0.4;   // near-zero g during free fall
const float IMPACT_THRESHOLD    = 1.8;   // sudden spike on hitting the ground
const float FEVER_THRESHOLD_C   = 38.0;  // fever cutoff
const float HYPOTHERMIA_THRESHOLD_C = 35.0;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);
Adafruit_MPU6050 mpu;
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long lastUpload = 0;
const unsigned long UPLOAD_INTERVAL_MS = 15000; // ThingSpeak free tier: min 15s between updates

bool fallDetectedPhase = false; // tracks whether we're mid-fall-sequence...
bool fallSinceLastUpload = false;   // <-- ADD THIS LINE

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

// Returns true if a fall event completes on this reading (free-fall followed by impact)
bool detectFall(float accelMagnitude) {
  if (accelMagnitude < FREE_FALL_THRESHOLD) {
    fallDetectedPhase = true;   // near weightlessness - possible start of a fall
    return false;
  }
  if (fallDetectedPhase && accelMagnitude > IMPACT_THRESHOLD) {
    fallDetectedPhase = false;  // impact after free-fall - confirmed fall
    return true;
  }
  return false;
}

void soundBuzzer(int beeps) {
  for (int i = 0; i < beeps; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(150);
  }
}

String urlEncode(String str) {
  str.replace(" ", "+");
  return str;
}

int statusToCode(String status) {
  if (status == "FALL DETECTED") return 1;
  if (status == "FEVER ALERT") return 2;
  if (status == "LOW TEMP ALERT") return 3;
  return 0;
}

void uploadToThingSpeak(float temperature, bool fallDetected, String status) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(THINGSPEAK_SERVER) + "?api_key=" + THINGSPEAK_API_KEY +
               "&field1=" + String(temperature, 1) +
               "&field2=" + String(fallDetected ? 1 : 0) +
               "&field3=" + String(statusToCode(status));

  http.begin(url);
  int httpCode = http.GET();
  Serial.println("ThingSpeak upload: " + String(httpCode));
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  tempSensor.begin();

  Wire.begin();
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found - check wiring!");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    Serial.println("MPU6050 initialized.");
  }

  lcd.init();
  lcd.backlight();
  lcd.print("Health Monitor");
  delay(1500);
  lcd.clear();

  connectWiFi();
}

void loop() {
  // --- Temperature reading ---
  tempSensor.requestTemperatures();
  float temperatureC = tempSensor.getTempCByIndex(0);

  // --- Accelerometer reading ---
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float ax = a.acceleration.x / 9.81; // convert to g
  float ay = a.acceleration.y / 9.81;
  float az = a.acceleration.z / 9.81;
  float accelMagnitude = sqrt(ax * ax + ay * ay + az * az);

  bool fall = detectFall(accelMagnitude);

if (fall) {
  fallSinceLastUpload = true;
}

  // --- Determine status ---
  String status = "OK";
  if (fall) {
    status = "FALL DETECTED";
    soundBuzzer(3);
  } else if (temperatureC > FEVER_THRESHOLD_C) {
    status = "FEVER ALERT";
    soundBuzzer(1);
  } else if (temperatureC < HYPOTHERMIA_THRESHOLD_C && temperatureC > 0) {
    status = "LOW TEMP ALERT";
    soundBuzzer(1);
  }

  // --- Local display ---
  lcd.setCursor(0, 0);
  lcd.print("Temp: " + String(temperatureC, 1) + "C   ");
  lcd.setCursor(0, 1);
  lcd.print("Status: " + status + "   ");

  // --- Serial debug ---
  Serial.println("Temp: " + String(temperatureC, 1) + "C | AccelMag: " +
                  String(accelMagnitude, 2) + "g | Status: " + status);

  // --- Cloud upload (rate-limited) ---
if (millis() - lastUpload > UPLOAD_INTERVAL_MS) {
  lastUpload = millis();
  uploadToThingSpeak(temperatureC, fallSinceLastUpload, status);   // changed "fall" to "fallSinceLastUpload"
  fallSinceLastUpload = false;   // <-- ADD THIS LINE
}

  delay(500);
}
