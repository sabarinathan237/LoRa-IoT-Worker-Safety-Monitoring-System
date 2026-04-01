// ===== TRANSMITTER: ESP8266 + LoRa (Ra-02) + Button + MAX30100 + GPS6MV2 =====

#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include "MAX30100_PulseOximeter.h"
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

// ---------------- LoRa config ----------------
#define LORA_SS    D8      // CS
#define LORA_RST   -1      // RST tied to 3.3V
#define LORA_DIO0  D0      // DIO0
const long LORA_FREQ = 433E6;

// ESP8266 Hardware SPI pins (fixed)
// SCK  -> D5
// MISO -> D6
// MOSI -> D7

// ---------------- Button ----------------
#define BTN_PIN    D3      // Active LOW

// ---------------- MAX30100 ----------------
PulseOximeter pox;
float heartRate = 0.0;
float spo2 = 0.0;
bool hrValid = false;
bool spo2Valid = false;
uint32_t lastPoxUpdate = 0;

// ---------------- GPS ----------------
// GPS TX -> D4
SoftwareSerial gpsSerial(D4, -1);   // RX=D4, TX unused
TinyGPSPlus gps;

double gpsLat = 0.0;
double gpsLon = 0.0;
bool gpsHasFix = false;

// ---------------- Timers ----------------
uint32_t lastSend = 0;

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(BTN_PIN, INPUT_PULLUP);

  // I2C for MAX30100
  Wire.begin(D2, D1);   // SDA=D2, SCL=D1
  Serial.print("Init MAX30100... ");
  if (!pox.begin()) {
    Serial.println("FAILED");
  } else {
    Serial.println("OK");
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
  }

  // GPS serial
  gpsSerial.begin(9600);
  Serial.println("GPS serial started @ 9600");

  // LoRa init
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("LoRa init failed");
    while (1) {}
  }

  LoRa.setTxPower(17, PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();

  Serial.println("TX ready");
}

void updateGPS() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isValid() && gps.location.age() < 2000) {
    gpsLat = gps.location.lat();
    gpsLon = gps.location.lng();
    gpsHasFix = true;
  } else {
    gpsHasFix = false;
  }
}

void updateMAX30100() {
  pox.update();

  if (millis() - lastPoxUpdate >= 1000) {
    lastPoxUpdate = millis();

    float hr = pox.getHeartRate();
    float ox = pox.getSpO2();

    if (hr >= 40.0 && hr <= 180.0) {
      heartRate = hr;
      hrValid = true;
    } else {
      hrValid = false;
    }

    if (ox >= 70.0 && ox <= 100.0) {
      spo2 = ox;
      spo2Valid = true;
    } else {
      spo2Valid = false;
    }

    Serial.print("HR = ");
    if (hrValid) Serial.print(heartRate, 1);
    else Serial.print("NA");

    Serial.print("  SpO2 = ");
    if (spo2Valid) Serial.println(spo2, 1);
    else Serial.println("NA");
  }
}

void sendPacket(bool pressed) {
  // GPS must be valid. No default values used.
  if (!gpsHasFix) {
    Serial.println("GPS invalid -> packet not sent");
    return;
  }

  String payload = "";

  payload += "B,";
  payload += String(pressed ? 1 : 0);

  payload += ",HR,";
  if (hrValid) payload += String(heartRate, 1);
  else payload += "NA";

  payload += ",SPO2,";
  if (spo2Valid) payload += String(spo2, 1);
  else payload += "NA";

  payload += ",LAT,";
  payload += String(gpsLat, 6);

  payload += ",LON,";
  payload += String(gpsLon, 6);

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();

  Serial.print("TX -> ");
  Serial.println(payload);
}

void loop() {
  updateMAX30100();
  updateGPS();

  bool pressed = (digitalRead(BTN_PIN) == LOW);

  if (millis() - lastSend >= 1000) {
    lastSend = millis();
    sendPacket(pressed);
  }
}