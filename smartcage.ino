#include <WiFi.h>
#include "time.h"
#include <DHT.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PubSubClient.h> // Tambahan Library MQTT

// ===== DEFINE PIN =====
#define PIN_DHT       4
#define PIN_BUZZER    26
#define PIN_SERVO     13
#define PIN_BTN_SERVO    33
#define PIN_RELAY     25
#define PIN_BTN_RELAY 5
#define LED_RED       14
#define LED_YELLOW    27
#define LED_GREEN     18

// ===== LCD =====
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== KONFIGURASI =====
#define DHTTYPE DHT22

const char* WIFI_SSID = "Kaum Rebahan";
const char* WIFI_PASS = "kepobgtdah";

// ===== KONFIGURASI MQTT =====
const char* mqtt_server = "broker.hivemq.com"; // Bisa diganti sesuai broker yang dipakai
const int mqtt_port = 1883;

const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET = 7 * 3600;
const int DAYLIGHT_OFFSET = 0;

// ===== OBJECT =====
DHT dht(PIN_DHT, DHTTYPE);
Servo myServo;
WiFiClient espClient;
PubSubClient client(espClient);

// ===== VARIABEL =====
String input = "";
float minTemp = 30;
float maxTemp = 34;

int kategori = 1;
bool lastRelayState = false;
bool lastRelayMode = false;

// Waktu
int lastJam = -1;
int lastMenit = -1;

// Button Servo
bool lastButtonState = HIGH;

// ===== RELAY CONTROL =====
bool relayModeManual = false;
bool relayState = false;

bool lastRelayBtnState = HIGH;
unsigned long pressStart = 0;

// Timer
unsigned long lastUpdate = 0;
unsigned long lastLCDSwitch = 0;
bool lcdMode = false;

unsigned long lastLEDUpdate = 0;
bool ledState = false;

// ===== MQTT TOPICS PUBLISH =====
const char* topic_pub_suhu = "monitoring/suhu";
const char* topic_pub_kelembapan = "monitoring/kelembapan";
const char* topic_pub_relay = "monitoring/relay";
const char* topic_pub_servo = "monitoring/servo";
const char* topic_pub_jam = "monitoring/jam";
const char* topic_pub_kategori = "monitoring/kategori";

// ===== MQTT TOPICS SUBSCRIBE =====
const char* topic_sub_kategori = "control/kategori";
const char* topic_sub_relay = "control/relay";
const char* topic_sub_servo = "control/servo";

// ===== SERVO =====
void gerakServo() {
  myServo.write(0);
  delay(1000);
  myServo.write(145);
}

void applyRelay() {
  digitalWrite(PIN_RELAY, relayState ? LOW : HIGH);
}

void updateTrafficLED(float suhu) {
  unsigned long currentMillis = millis();

  if (currentMillis - lastLEDUpdate >= 500) {
    lastLEDUpdate = currentMillis;
    ledState = !ledState;
  }

  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN, LOW);

  if (ledState) {
    if (suhu > maxTemp) {
      digitalWrite(LED_RED, HIGH);      // Panas = Merah
    }
    else if (suhu < minTemp) {
      digitalWrite(LED_YELLOW, HIGH);   // Dingin = Kuning
    }
    else {
      digitalWrite(LED_GREEN, HIGH);    // Ideal = Hijau
    }
  }
}

// ===== MQTT CALLBACK (MENERIMA PESAN) =====
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  
  Serial.print("Pesan MQTT masuk [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  // Kontrol Kategori
  if (String(topic) == topic_sub_kategori) {
    int kat = msg.toInt();
    if (kat == 1) { minTemp = 30; maxTemp = 34; kategori = 1; }
    else if (kat == 2) { minTemp = 28; maxTemp = 32; kategori = 2; }
    else if (kat == 3) { minTemp = 25; maxTemp = 29; kategori = 3; }
    else if (kat == 4) { minTemp = 21; maxTemp = 25; kategori = 4; }
    Serial.println("Kategori diubah via MQTT ke: " + String(kategori));
  }
  // Kontrol Relay (ON / OFF / AUTO)
  else if (String(topic) == topic_sub_relay) {
    if (msg == "AUTO") {
      relayModeManual = false;
      Serial.println("Relay Mode Auto Aktif via MQTT");
    } else if (msg == "ON") {
      relayModeManual = true;
      relayState = true;
      applyRelay();
      Serial.println("Relay ON (Manual) via MQTT");
    } else if (msg == "OFF") {
      relayModeManual = true;
      relayState = false;
      applyRelay();
      Serial.println("Relay OFF (Manual) via MQTT");
    }
  }
  // Kontrol Servo
  else if (String(topic) == topic_sub_servo) {
    if (msg == "FEED") {
      Serial.println("Memberi pakan via MQTT!");
      gerakServo();
    }
  }
}

// ===== MQTT RECONNECT =====
void reconnect() {
  while (!client.connected()) {
    Serial.print("Mencoba koneksi MQTT...");
    String clientId = "ESP32Client-SmartCage-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("Terhubung ke MQTT Broker");
      // Subscribe topik kontrol
      client.subscribe(topic_sub_kategori);
      client.subscribe(topic_sub_relay);
      client.subscribe(topic_sub_servo);
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 5 detik...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  dht.begin();

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_BTN_SERVO, INPUT_PULLUP);
  pinMode(PIN_BTN_RELAY, INPUT_PULLUP);

  pinMode(PIN_RELAY, OUTPUT);
  relayState = false;   // kondisi awal OFF
  applyRelay();

  myServo.attach(PIN_SERVO);
  myServo.write(145);

  // Setup LED Traffic
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN, LOW);  

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.print("Connecting WiFi");

  Serial.println("WiFi Connecting...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
  
  lcd.clear();
  lcd.print("WiFi Connected");

  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER);
  
  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  delay(1000);
}

void loop() {

  // Jaga koneksi MQTT tetap hidup
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // ===== INPUT SERIAL =====
  if (Serial.available()) {
    input = Serial.readStringUntil('\n');
    input.trim();

    if (input == "kategori 1") {
      minTemp = 30; maxTemp = 34; kategori = 1;
    } else if (input == "kategori 2") {
      minTemp = 28; maxTemp = 32; kategori = 2;
    } else if (input == "kategori 3") {
      minTemp = 25; maxTemp = 29; kategori = 3;
    } else if (input == "kategori 4") {
      minTemp = 21; maxTemp = 25; kategori = 4;
    }
  }

  // ===== BUTTON SERVO =====
  bool currentButtonState = digitalRead(PIN_BTN_SERVO);
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    gerakServo();
  }
  lastButtonState = currentButtonState;

  // ===== BUTTON RELAY =====
  bool currentRelayBtn = digitalRead(PIN_BTN_RELAY);

  if (currentRelayBtn == LOW && lastRelayBtnState == HIGH) {
    pressStart = millis();
  }

  if (currentRelayBtn == HIGH && lastRelayBtnState == LOW) {
    unsigned long pressDuration = millis() - pressStart;

    if (pressDuration >= 2000) {
      // tahan 2 detik menjadi AUTO
      relayModeManual = false;
      Serial.println("Mode Auto Aktif");
    } else {
      // klik biasa
      if (!relayModeManual) {
        relayModeManual = true;
        Serial.println("Masuk Mode Manual");
      } else {
        relayState = !relayState;
        Serial.println(relayState ? "Relay ON" : "Relay OFF");
      }
    }

    applyRelay();
  }

  lastRelayBtnState = currentRelayBtn;

  // ===== UPDATE SENSOR & PUBLISH MQTT SETIAP DETIK =====
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();

    float suhu = dht.readTemperature();
    float kelembapan = dht.readHumidity();
    struct tm timeinfo;

    if (isnan(suhu) || isnan(kelembapan)) return;
    if (!getLocalTime(&timeinfo)) return;

    int jam = timeinfo.tm_hour;
    int menit = timeinfo.tm_min;
    int detik = timeinfo.tm_sec;

    // ===== BUZZER =====
    if (suhu < minTemp || suhu > maxTemp) {
      digitalWrite(PIN_BUZZER, HIGH);
      delay(200);
      digitalWrite(PIN_BUZZER, LOW);
    }

    // ===== RELAY AUTO =====
    if (!relayModeManual) {
      if (suhu > maxTemp) {
        relayState = true;
      } else {
        relayState = false;
      }
      applyRelay();
    }

    // ===== SERVO JADWAL =====
    bool triggerWaktu =
      (jam == 7  && menit == 0) ||
      (jam == 12 && menit == 0) ||
      (jam == 17 && menit == 0);

    if (triggerWaktu && !(lastJam == jam && lastMenit == menit)) {
      gerakServo();
    }

    lastJam = jam;
    lastMenit = menit;

    updateTrafficLED(suhu);

    // =============================================
    // == LCD DISPLAY - BERGANTIAN SETIAP 2 DETIK ==
    // =============================================
    if (millis() - lastLCDSwitch >= 2000) {
      lastLCDSwitch = millis();
      lcdMode = !lcdMode;
    }

    // ----- Baris 1 -----
    lcd.setCursor(0, 0);
    lcd.print("                ");

    lcd.setCursor(0, 0);

    if (!lcdMode) {
      // Tampilkan Suhu & Kelembapan
      lcd.print("T:");
      lcd.print((int)suhu);
      lcd.print((char)223);
      lcd.print("C ");

      lcd.print("H:");
      lcd.print((int)kelembapan);
      lcd.print("%   ");
    } 
    else {
      // Tampilkan Jam Realtime
      lcd.print("Jam: ");
      if (jam < 10) lcd.print("0");
      lcd.print(jam);
      lcd.print(":");
      if (menit < 10) lcd.print("0");
      lcd.print(menit);
      lcd.print(":");
      if (detik < 10) lcd.print("0");
      lcd.print(detik);
      lcd.print("      ");
    }

    // ----- Baris 2 -----
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);

    if (!lcdMode) {
      // Mode Suhu: Status suhu + Mode relay
      if (suhu < minTemp) {
        lcd.print("Dingin ");
      } else if (suhu > maxTemp) {
        lcd.print("Panas  ");
      } else {
        lcd.print("Ideal  ");
      }
      lcd.print(relayModeManual ? "Man" : "Auto");
    } 
    else {
      // Mode Jam: Jadwal feeding berikutnya
      String jadwal;
      if (jam < 7) jadwal = "07:00";
      else if (jam < 12) jadwal = "12:00";
      else if (jam < 17) jadwal = "17:00";
      else jadwal = "07:00";

      lcd.print("Next Feed: ");
      lcd.print(jadwal);
      lcd.print("      ");
    }
    
    // ===== STATUS SUHU =====
    String statusSuhu;
    if (suhu < minTemp) statusSuhu = "Dingin";
    else if (suhu > maxTemp) statusSuhu = "Panas";
    else statusSuhu = "Ideal";

    // ===== JADWAL =====
    String jadwalSerial;
    if (jam < 7) jadwalSerial = "07:00";
    else if (jam < 12) jadwalSerial = "12:00";
    else if (jam < 17) jadwalSerial = "17:00";
    else jadwalSerial = "07:00";

    // ===== PUBLISH DATA KE MQTT =====
    client.publish(topic_pub_suhu, String(suhu).c_str());
    client.publish(topic_pub_kelembapan, String(kelembapan).c_str());
    
    // Mengirim status relay lengkap dengan mode-nya
    String statusRelay = relayModeManual ? (relayState ? "ON (Manual)" : "OFF (Manual)") : (relayState ? "ON (Auto)" : "OFF (Auto)");
    client.publish(topic_pub_relay, statusRelay.c_str());
    
    client.publish(topic_pub_servo, jadwalSerial.c_str());
    
    char waktuStr[9];
    sprintf(waktuStr, "%02d:%02d:%02d", jam, menit, detik);
    client.publish(topic_pub_jam, waktuStr);
    
    client.publish(topic_pub_kategori, String(kategori).c_str());

    // ===== SERIAL OUTPUT =====
    Serial.println("=================================");
    Serial.print("Kategori: ");
    Serial.println(kategori);

    Serial.print("Next Feed: ");
    Serial.println(jadwalSerial);

    Serial.print("Waktu: ");
    Serial.println(waktuStr);

    Serial.print(" | Temp: ");
    Serial.print((int)suhu);
    Serial.print("°C | Hum: ");
    Serial.print((int)kelembapan);
    Serial.print("% (");
    Serial.print(statusSuhu);
    Serial.println(")");

    // ===== RELAY (SAAT BERUBAH) =====
    if (relayState != lastRelayState || relayModeManual != lastRelayMode) {
      Serial.print("Relay: ");
      Serial.print(relayModeManual ? "Manual" : "Auto");
      Serial.print(" | ");
      Serial.println(relayState ? "ON" : "OFF");

      lastRelayState = relayState;
      lastRelayMode = relayModeManual;
    }
  }
}
