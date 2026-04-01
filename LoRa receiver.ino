// ===== RECEIVER: ESP8266 + LoRa + Buzzer + Blynk IoT =====

#define BLYNK_TEMPLATE_ID   "TMPL3kx8G5Wi9"
#define BLYNK_TEMPLATE_NAME "LoRa and IoT Enabled Real Time Safety Monitoring"
#define BLYNK_AUTH_TOKEN    "wnv2kdPkKrgijACneG9OnFf0qssPhZEt"

#include <SPI.h>
#include <LoRa.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// ---------------- WiFi ----------------
char ssid[] = "lora1234";
char pass[] = "lora4321";

// ---------------- LoRa config ----------------
const long LORA_FREQ = 433E6;

#define LORA_SS    D1
#define LORA_RST   -1
#define LORA_DIO0  D2

// ESP8266 Hardware SPI pins (fixed)
// SCK  -> D5
// MISO -> D6
// MOSI -> D7

// ---------------- Buzzer ----------------
#define BUZZ_PIN   D0

// ---------------- Blynk Virtual Pins ----------------
#define VPIN_BUTTON   V0
#define VPIN_HR       V1
#define VPIN_SPO2     V2
#define VPIN_LAT      V3
#define VPIN_LON      V4

uint32_t lastPacketTime = 0;

String getTagValue(const String& s, const String& tag) {
  int i = s.indexOf(tag);
  if (i < 0) return "";

  int start = i + tag.length();
  int comma = s.indexOf(',', start);

  if (comma > 0) return s.substring(start, comma);
  return s.substring(start);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(BUZZ_PIN, OUTPUT);
  digitalWrite(BUZZ_PIN, LOW);

  Serial.println("Connecting to WiFi + Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("LoRa init failed");
    while (1) {}
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();

  Serial.println("RX ready");
}

void loop() {
  Blynk.run();

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String msg = "";
    while (LoRa.available()) {
      msg += (char)LoRa.read();
    }

    Serial.print("RX -> ");
    Serial.println(msg);

    String buttonStr = getTagValue(msg, "B,");
    String hrStr     = getTagValue(msg, "HR,");
    String spo2Str   = getTagValue(msg, "SPO2,");
    String latStr    = getTagValue(msg, "LAT,");
    String lonStr    = getTagValue(msg, "LON,");

    int button = buttonStr.toInt();

    if (button == 1) {
      digitalWrite(BUZZ_PIN, HIGH);
      Serial.println("BUZZER ON");
    } else {
      digitalWrite(BUZZ_PIN, LOW);
      Serial.println("BUZZER OFF");
    }

    Blynk.virtualWrite(VPIN_BUTTON, button);

    if (hrStr.length() > 0 && hrStr != "NA") {
      Blynk.virtualWrite(VPIN_HR, hrStr.toFloat());
    }

    if (spo2Str.length() > 0 && spo2Str != "NA") {
      Blynk.virtualWrite(VPIN_SPO2, spo2Str.toFloat());
    }

    if (latStr.length() > 0 && lonStr.length() > 0) {
      Blynk.virtualWrite(VPIN_LAT, latStr.toFloat());
      Blynk.virtualWrite(VPIN_LON, lonStr.toFloat());
    }

    Serial.print("Button = "); Serial.println(button);
    Serial.print("HR = "); Serial.println(hrStr);
    Serial.print("SpO2 = "); Serial.println(spo2Str);
    Serial.print("LAT = "); Serial.println(latStr);
    Serial.print("LON = "); Serial.println(lonStr);

    lastPacketTime = millis();
  }

  // Optional safety: if no packet for 5 sec, buzzer OFF
  if (millis() - lastPacketTime > 5000) {
    digitalWrite(BUZZ_PIN, LOW);
  }
}