#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <RTClib.h>
#include <DHT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

const char* ssid = "nama wifi";
const char* password = "password";

#define API_KEY "api key"

#define USER_EMAIL "irsannurhidayat@gmail.com"
#define USER_PASSWORD "12345678"

#define DATABASE_URL "https://iot-jamur-tiram-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String databasePath;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200);

String uid;

String tempPath = "/suhu";
String humPath = "/kelembapan";
String datePath = "/tanggal";
String timePath = "/waktu";

String parentPath;

FirebaseJson json;

int lcdColumns = 20;
int lcdRows = 4;

LiquidCrystal_I2C lcd (0x27, lcdColumns, lcdRows);

RTC_DS3231 rtc;

#define DHTPIN 4
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

float suhu;
float kelembapan;
char tanggal[11];
char waktu[6];
String dataLogPath;
unsigned long timestamp;

void setup(void) {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Inisialisasi Sistem");
  Serial.println("Inisialisasi LCD Berhasil");

   delay(1000);

  Serial.println("Inisialisasi Koneksi WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Inisialisasi Koneksi WiFi Berhasil");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  delay(1000);

  timeClient.begin();
  timeClient.update();

  if (!rtc.begin()) {
    Serial.println("RTC belum terdeteksi");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.print("RTC Kehilangan Daya");
    rtc.adjust(DateTime(timeClient.getEpochTime()));
  }

  Serial.println("Inisialisasi RTC Berhasil");

  delay(1000);

  dht.begin();

  delay(1000);

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;
  Firebase.begin(&config, &auth);

  Serial.println("Mendapatkan UID");
  while ((auth.token.uid) == "") {
    Serial.print(".");
    delay(1000);
  }

  uid = auth.token.uid.c_str();
  Serial.print("UID User: ");
  Serial.println(uid);

  databasePath = "/iot-jamur-tiram-rtdb/UsersData/c1sNTPgEXCatwRQPjyhFFKeBjVr2/DataSensor";

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Inisialisasi Selesai");

  delay(3000);
}

void loop(void) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mengambil Data...");
  delay(3000);
  suhu = dht.readTemperature();
  kelembapan = dht.readHumidity();

  if (isnan(suhu) || isnan(kelembapan)) {
    Serial.println("Gagal Mendeteksi Suhu dan Kelembapan");
  } else {
    DateTime now = rtc.now();
    sprintf(tanggal, "%02d/%02d/%04d", now.day(), now.month(), now.year());
    sprintf(waktu, "%02d.%02d", now.hour(), now.minute());
    timestamp = now.unixtime();
    Serial.println("-----");
    Serial.print("Suhu: ");
    Serial.print(suhu);
    Serial.println(" C");
    Serial.print("Kelembapan: ");
    Serial.print(kelembapan);
    Serial.println(" %");
    Serial.print("Tanggal: ");
    Serial.println(tanggal);
    Serial.print("Waktu: ");
    Serial.print(waktu);
    Serial.println(" WIB");

    if (Firebase.ready()) {
      delay(1000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Mengirim Data...");
      Serial.println("Mencoba Mengirim Data");
      parentPath = databasePath + "/" + String(timestamp);
      json.set(tempPath.c_str(), suhu);
      json.set(humPath.c_str(), kelembapan);
      json.set(datePath.c_str(), tanggal);
      json.set(timePath.c_str(), waktu);
      if (Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json)) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Data Terkirim");
        Serial.println("Data berhasil terkirim");
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Data Gagal Terkirim");
        Serial.print("Gagal mengirim data: ");
        Serial.println(fbdo.errorReason());
      }
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Tidak Dapat Mengirim Data");
    }

    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(suhu);
    lcd.print(" C");
    lcd.setCursor(0, 1);
    lcd.print("Hum: ");
    lcd.print(kelembapan);
    lcd.print(" %");
    lcd.setCursor(0, 2);
    lcd.print("Date: ");
    lcd.print(tanggal);
    lcd.setCursor(0, 3);
    lcd.print("Time: ");
    lcd.print(waktu);
    lcd.print(" WIB");
  }
  delay(60000);
}
