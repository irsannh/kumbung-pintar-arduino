// pendefinisian library yang digunakan
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <DHT.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// pendefinisian ssid dan password untuk WiFi
const char* ssid = "nama wifi";
const char* password = "password";

// pendefinisian key, akun, serta database untuk Firebase
#define API_KEY "AIzaSyAMlLfPUpNv2RzYMbh_uDLGYZgf5DD3Yxw"

#define USER_EMAIL "irsannurhidayat@gmail.com"
#define USER_PASSWORD "12345678"

#define DATABASE_URL "https://iot-jamur-tiram-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson json;

String uid;
String dataLogPath = "/iot-jamur-tiram-rtdb/UsersData/c1sNTPgEXCatwRQPjyhFFKeBjVr2/DataSensor";
String parentPath;
String tempPath = "/suhu";
String humPath = "/kelembapan";
String datePath = "/tanggal";
String timePath = "/waktu";
String fanControlPath = "/iot-jamur-tiram-rtdb/UsersData/c1sNTPgEXCatwRQPjyhFFKeBjVr2/DataKendali/kendali_kipas";
String sensorControlPath = "/iot-jamur-tiram-rtdb/UsersData/c1sNTPgEXCatwRQPjyhFFKeBjVr2/DataKendali/kendali_sensor";
String fanPWMPath = "/iot-jamur-tiram-rtdb/UsersData/c1sNTPgEXCatwRQPjyhFFKeBjVr2/DataKendali/pwm_kipas";
String pumpControlPath = "/iot-jamur-tiram-rtdb/UsersData/c1sNTPgEXCatwRQPjyhFFKeBjVr2/DataKendali/kendali_pompa";
String heaterControlPath = "/iot-jamur-tiram-rtdb/UsersData/c1sNTPgEXCatwRQPjyhFFKeBjVr2/DataKendali/kendali_heater";
String fuzzyControlPath = "/iot-jamur-tiram-rtdb/UsersData/c1sNTPgEXCatwRQPjyhFFKeBjVr2/DataKendali/pengendalian_fuzzy";

// pendefinisian LCD yang akan digunakan
int lcdColumns = 20;
int lcdRows = 4;
LiquidCrystal_I2C lcd (0x27, lcdColumns, lcdRows);

// pendefinisian NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600);

// pendefinisian RTC yang akan digunakan
RTC_DS3231 rtc;

// pendefinisian Sensor DHT22 yang akan digunakan
#define DHTPIN 15
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

// pendefinisian pin untuk relay pompa dan heater
#define RELAY_PUMP_PIN 5
#define RELAY_HEATER_PIN 12

// pendefinisian pwm untuk kipas
#define ENA_FAN_1 18
#define ENA_FAN_2 19

const int pwmFrequency = 25000;
const int pwmResolution = 8;
const int pwmChannel1 = 0;
const int pwmChannel2 = 1;

// pendefinisian variabel yang akan digunakan
float suhu;
float kelembapan;
char tanggal[11];
char waktu[6];
unsigned long timestamp;
bool pengendalian_fuzzy = false;
DateTime now;
float kalibrasi_suhu = 0.70;
float kalibrasi_kelembapan = 3.27;
float suhu_terakhir;
float suhu_set = 27.00;
float kelembapan_set = 80.00;
float kelembapan_terakhir;
bool state_kipas = false;
bool state_pompa = false;
bool state_heater = false;
int state_pwm = 0;
bool state_sensor = false;

void setup() {
  Serial.begin(115200);

  // inisialisasi LCD
  Serial.println("Inisialisasi LCD...");
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Halo!");
  Serial.println("Inisialisasi LCD Berhasil Dilakukan");

  delay(1000);

  // inisialisasi koneksi WiFi
  Serial.println("Inisialisasi Koneksi WiFi...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Inisialisasi WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Inisialisasi Koneksi WiFi Berhasil Dilakukan");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Berhasil Terkoneksi");

  delay(1000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Inisialisasi Sistem");
  delay(2000);
  timeClient.begin();
  timeClient.update();
  // inisialisasi RTC
  Serial.println("Inisialisasi RTC...");

  if (!rtc.begin()) {
    Serial.println("RTC Tidak Terdeteksi");
    while (1);
  }

  rtc.adjust(DateTime(timeClient.getEpochTime()));
  Serial.println("RTC Tersinkronisasi dengan NTP Time!");

  now = rtc.now();
  if (now.year() != 2025) {
    ESP.restart();
  }
  
  delay(1000);

  // Inisialisasi Sensor DHT22
  Serial.println("Inisialisasi Sensor DHT22...");
  dht.begin();
  Serial.println("Inisialisasi Sensor DHT22 Berhasil Dilakukan");

  delay(1000);

  // Inisialisasi Relay untuk Pompa dan Heater
  Serial.println("Inisialisasi Pin Relay untuk Pompa dan Heater...");
  pinMode(RELAY_PUMP_PIN, OUTPUT);
  pinMode(RELAY_HEATER_PIN, OUTPUT);
  digitalWrite(RELAY_PUMP_PIN, LOW);
  digitalWrite(RELAY_HEATER_PIN, LOW);
  Serial.println("Inisialisasi Pin Relay untuk Pompa dan Heater Berhasil Dilakukan");

  delay(1000);

  // Inisialisasi PWM
  Serial.println("Inisialisasi Pin PWM untuk Kipas...");
  ledcAttachChannel(ENA_FAN_1, pwmFrequency, pwmResolution, pwmChannel1);
  ledcAttachChannel(ENA_FAN_2, pwmFrequency, pwmResolution, pwmChannel2);
  ledcWrite(ENA_FAN_1, 128);
  ledcWrite(ENA_FAN_2, 128);
  delay(5000);
  ledcWrite(ENA_FAN_1, 255);
  ledcWrite(ENA_FAN_2, 255);
  delay(5000);
  ledcWrite(ENA_FAN_1, 0);
  ledcWrite(ENA_FAN_2, 0);
  Serial.println("Inisialisasi Pin PWM untuk Kipas Berhasil Dilakukan");

  delay(1000);

  // Inisialisasi Firebase
  Serial.println("Inisialisasi Firebase...");
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;
  Firebase.begin(&config, &auth);

  Serial.println("Mencoba Mendapatkan UID...");
  while ((auth.token.uid) == "") {
    Serial.print(".");
    delay(500);
  }

  uid = auth.token.uid.c_str();
  Serial.print("UID User: ");
  Serial.println(uid);

  Serial.println("Inisialisasi Firebase Berhasil Dilakukan");

  delay(1000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Inisialisasi Selesai");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sistem Dimulai");

  delay(1000);

  pemantauan();
  delay(3000);
  if (suhu <= 24.00 or suhu >= 27.00 or kelembapan <= 80.00 or kelembapan >= 90.00) {
    pengendalian();
  } else {
    delay(60000);
    
    KirimKondisiTerkini();

    pengendalian_fuzzy = false;

    Firebase.RTDB.setBool(&fbdo, fuzzyControlPath, pengendalian_fuzzy);
    if (fbdo.errorReason() != "") {
      Serial.println("Gagal Mengubah Data pengendalian_fuzzy menjadi false: " + fbdo.errorReason());
    } else {
      Serial.println("Berhasil Mengubah Data pengendalian_fuzzy menjadi false");
    }

    fbdo.clear();
  }
}

void loop() {
  now = rtc.now();

  if (now.minute() == 0) {
    ESP.restart();
  } else if (now.minute() == 30) {
    pemantauan();
    delay(3000);
    if (suhu <= 24.00 or suhu >= 27.00 or kelembapan <= 80.00 or kelembapan >= 90.00) {
      pengendalian();
    } else {
      delay(60000);

      KirimKondisiTerkini();

      pengendalian_fuzzy = false;

      Firebase.RTDB.setBool(&fbdo, fuzzyControlPath, pengendalian_fuzzy);
      if (fbdo.errorReason() != "") {
        Serial.println("Gagal Mengubah Data pengendalian_fuzzy menjadi false: " + fbdo.errorReason());
      } else {
        Serial.println("Berhasil Mengubah Data pengendalian_fuzzy menjadi false");
      }

      fbdo.clear();
    } 
  } else {
    kendali_mobile();
    delay(2000);
    cekKondisiTerkini();
  }
  delay(2000);
}

void pemantauan() {
  pengendalian_fuzzy = true;

  // set LCD untuk memberikan tampilan yang menyatakan bahwa sistem tengah memantau kondisi
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Memantau Kondisi...");

  // mengubah key pengendalian_fuzzy menjadi true agar tidak ada pengendalian yang dilakukan dari mobile app
  Firebase.RTDB.setBool(&fbdo, fuzzyControlPath, pengendalian_fuzzy);
  if (fbdo.errorReason() != "") {
    Serial.println("Gagal Mengubah Data pengendalian_fuzzy menjadi true: " + fbdo.errorReason());
  } else {
    Serial.println("Berhasil Mengubah Data pengendalian_fuzzy menjadi true");
  }

  fbdo.clear();

  delay(1000);

  // mematikan semua aktuator yang ada jika masih aktif
  Serial.println("Mematikan Semua Aktuator...");
  ledcWrite(ENA_FAN_1, 0);
  ledcWrite(ENA_FAN_2, 0);
  digitalWrite(RELAY_PUMP_PIN, LOW);
  digitalWrite(RELAY_HEATER_PIN, LOW);
  Serial.println("Aktuator Berhasil Dimatikan");
  
  Firebase.RTDB.setBool(&fbdo, fanControlPath, false);
  Firebase.RTDB.setBool(&fbdo, pumpControlPath, false);
  Firebase.RTDB.setBool(&fbdo, heaterControlPath, false);
  Firebase.RTDB.setInt(&fbdo, fanPWMPath, 0);

  if (fbdo.errorReason() != "") {
    Serial.println("Gagal Mengubah Data seluruh kendali kipas, pompa, dan heater: " + fbdo.errorReason());
  } else {
    Serial.println("Berhasil Mengubah Data seluruh kendali kipas, pompa, dan heater");
  }

  fbdo.clear();

  delay(1000);

  // mengambil data suhu, kelembapan, waktu, dan tanggal pengukuran terkini
  KirimKondisiTerkini();
}

void pengendalian() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mengendalikan Kondisi...");
  // deklarasi variabel yang digunakan untuk logika fuzzy
  float fk_input_suhu_dingin, fk_input_suhu_ideal, fk_input_suhu_panas;
  float fk_input_kelembapan_rendah, fk_input_kelembapan_sedang, fk_input_kelembapan_ideal, fk_input_kelembapan_tinggi;
  float inferensi_aturan_1, inferensi_aturan_2, inferensi_aturan_3, inferensi_aturan_4, inferensi_aturan_5, inferensi_aturan_6; 
  float inferensi_aturan_7, inferensi_aturan_8, inferensi_aturan_9, inferensi_aturan_10, inferensi_aturan_11, inferensi_aturan_12;
  float output_kipas_1, output_kipas_2, output_kipas_3, output_kipas_4, output_kipas_5, output_kipas_6;
  float output_kipas_7, output_kipas_8, output_kipas_9, output_kipas_10, output_kipas_11, output_kipas_12;
  float output_pompa_1, output_pompa_2, output_pompa_3, output_pompa_4, output_pompa_5, output_pompa_6;
  float output_pompa_7, output_pompa_8, output_pompa_9, output_pompa_10, output_pompa_11, output_pompa_12;
  float output_heater_1, output_heater_2, output_heater_3, output_heater_4, output_heater_5, output_heater_6;
  float output_heater_7, output_heater_8, output_heater_9, output_heater_10, output_heater_11, output_heater_12;
  float output_pwm_kipas, output_durasi_pompa, output_durasi_heater;
  int pwm_kipas, durasi_pompa, durasi_heater;

  // assignment nilai fungsi keanggotaan dari masing-masing variabel input fuzzy (fuzzyfikasi)
  fk_input_suhu_dingin = fuzzyfikasi_suhu_dingin(suhu);
  fk_input_suhu_ideal = fuzzyfikasi_suhu_ideal(suhu);
  fk_input_suhu_panas = fuzzyfikasi_suhu_panas(suhu);
  fk_input_kelembapan_rendah = fuzzyfikasi_kelembapan_rendah(kelembapan);
  fk_input_kelembapan_sedang = fuzzyfikasi_kelembapan_sedang(kelembapan);
  fk_input_kelembapan_ideal = fuzzyfikasi_kelembapan_ideal(kelembapan);
  fk_input_kelembapan_tinggi = fuzzyfikasi_kelembapan_tinggi(kelembapan);

  // inferensi dengan menggunakan operasi logika AND (fungsi min)
  inferensi_aturan_1 = min(fk_input_suhu_dingin, fk_input_kelembapan_rendah);
  inferensi_aturan_2 = min(fk_input_suhu_dingin, fk_input_kelembapan_sedang);
  inferensi_aturan_3 = min(fk_input_suhu_dingin, fk_input_kelembapan_ideal);
  inferensi_aturan_4 = min(fk_input_suhu_dingin, fk_input_kelembapan_tinggi);
  inferensi_aturan_5 = min(fk_input_suhu_ideal, fk_input_kelembapan_rendah);
  inferensi_aturan_6 = min(fk_input_suhu_ideal, fk_input_kelembapan_sedang);
  inferensi_aturan_7 = min(fk_input_suhu_ideal, fk_input_kelembapan_ideal);
  inferensi_aturan_8 = min(fk_input_suhu_ideal, fk_input_kelembapan_tinggi);
  inferensi_aturan_9 = min(fk_input_suhu_panas, fk_input_kelembapan_rendah);
  inferensi_aturan_10 = min(fk_input_suhu_panas, fk_input_kelembapan_sedang);
  inferensi_aturan_11 = min(fk_input_suhu_panas, fk_input_kelembapan_ideal);
  inferensi_aturan_12 = min(fk_input_suhu_panas, fk_input_kelembapan_tinggi);

  // inferensi untuk mendapatkan nilai crisp pada masing-masing variabel output
  // crisp variabel output kecepatan kipas
  output_kipas_1 = crisp_kipas_pelan(inferensi_aturan_1);
  output_kipas_2 = crisp_kipas_pelan(inferensi_aturan_2);
  output_kipas_3 = crisp_kipas_pelan(inferensi_aturan_3);
  output_kipas_4 = crisp_kipas_pelan(inferensi_aturan_4);
  output_kipas_5 = crisp_kipas_agak_cepat(inferensi_aturan_5);
  output_kipas_6 = crisp_kipas_agak_cepat(inferensi_aturan_6);
  output_kipas_7 = crisp_kipas_agak_cepat(inferensi_aturan_7);
  output_kipas_8 = crisp_kipas_agak_cepat(inferensi_aturan_8);
  output_kipas_9 = crisp_kipas_cepat(inferensi_aturan_9);
  output_kipas_10 = crisp_kipas_cepat(inferensi_aturan_10);
  output_kipas_11 = crisp_kipas_cepat(inferensi_aturan_11);
  output_kipas_12 = crisp_kipas_cepat(inferensi_aturan_12);

  // crisp variabel output durasi pompa
  output_pompa_1 = crisp_pompa_lama(inferensi_aturan_1);
  output_pompa_2 = crisp_pompa_sebentar_sisi_kiri(inferensi_aturan_2);
  output_pompa_3 = crisp_pompa_agak_sebentar(inferensi_aturan_3);
  output_pompa_4 = crisp_pompa_sangat_sebentar(inferensi_aturan_4);
  output_pompa_5 = crisp_pompa_lama(inferensi_aturan_5);
  output_pompa_6 = crisp_pompa_agak_lama_sisi_kiri(inferensi_aturan_6);
  output_pompa_7 = crisp_pompa_sangat_sebentar(inferensi_aturan_7);
  output_pompa_8 = crisp_pompa_sangat_sebentar(inferensi_aturan_8);
  output_pompa_9 = crisp_pompa_lama(inferensi_aturan_9);
  output_pompa_10 = crisp_pompa_agak_lama_sisi_kanan(inferensi_aturan_10);
  output_pompa_11 = crisp_pompa_sebentar_sisi_kanan(inferensi_aturan_11);
  output_pompa_12 = crisp_pompa_sebentar_sisi_kiri(inferensi_aturan_12);

  // crisp variabel output durasi heater
  output_heater_1 = crisp_heater_lama(inferensi_aturan_1);
  output_heater_2 = crisp_heater_agak_lama(inferensi_aturan_2);
  output_heater_3 = crisp_heater_agak_lama(inferensi_aturan_3);
  output_heater_4 = crisp_heater_sebentar(inferensi_aturan_4);
  output_heater_5 = crisp_heater_mati(inferensi_aturan_5);
  output_heater_6 = crisp_heater_mati(inferensi_aturan_6);
  output_heater_7 = crisp_heater_mati(inferensi_aturan_7);
  output_heater_8 = crisp_heater_sangat_sebentar(inferensi_aturan_8);
  output_heater_9 = crisp_heater_mati(inferensi_aturan_9);
  output_heater_10 = crisp_heater_mati(inferensi_aturan_10);
  output_heater_11 = crisp_heater_mati(inferensi_aturan_11);
  output_heater_12 = crisp_heater_sangat_sebentar(inferensi_aturan_12);

  // defuzzyfikasi kipas
  output_pwm_kipas = (((inferensi_aturan_1 * output_kipas_1) + (inferensi_aturan_2 * output_kipas_2) + (inferensi_aturan_3 * output_kipas_3) + (inferensi_aturan_4 * output_kipas_4) + (inferensi_aturan_5 * output_kipas_5) + (inferensi_aturan_6 * output_kipas_6) + (inferensi_aturan_7 * output_kipas_7) + (inferensi_aturan_8 * output_kipas_8) + (inferensi_aturan_9 * output_kipas_9) + (inferensi_aturan_10 * output_kipas_10) + (inferensi_aturan_11 * output_kipas_11) + (inferensi_aturan_12 * output_kipas_12)) / (inferensi_aturan_1 + inferensi_aturan_2 + inferensi_aturan_3 + inferensi_aturan_4 + inferensi_aturan_5 + inferensi_aturan_6 + inferensi_aturan_7 + inferensi_aturan_8 + inferensi_aturan_9 + inferensi_aturan_10 + inferensi_aturan_11 + inferensi_aturan_12));
  
  // defuzzyfikasi pompa
  output_durasi_pompa = (((inferensi_aturan_1 * output_pompa_1) + (inferensi_aturan_2 * output_pompa_2) + (inferensi_aturan_3 * output_pompa_3) + (inferensi_aturan_4 * output_pompa_4) + (inferensi_aturan_5 * output_pompa_5) + (inferensi_aturan_6 * output_pompa_6) + (inferensi_aturan_7 * output_pompa_7) + (inferensi_aturan_8 * output_pompa_8) + (inferensi_aturan_9 * output_pompa_9) + (inferensi_aturan_10 * output_pompa_10) + (inferensi_aturan_11 * output_pompa_11) + (inferensi_aturan_12 * output_pompa_12)) / (inferensi_aturan_1 + inferensi_aturan_2 + inferensi_aturan_3 + inferensi_aturan_4 + inferensi_aturan_5 + inferensi_aturan_6 + inferensi_aturan_7 + inferensi_aturan_8 + inferensi_aturan_9 + inferensi_aturan_10 + inferensi_aturan_11 + inferensi_aturan_12));
  
  // defuzzyfikasi heater
  output_durasi_heater = (((inferensi_aturan_1 * output_heater_1) + (inferensi_aturan_2 * output_heater_2) + (inferensi_aturan_3 * output_heater_3) + (inferensi_aturan_4 * output_heater_4) + (inferensi_aturan_5 * output_heater_5) + (inferensi_aturan_6 * output_heater_6) + (inferensi_aturan_7 * output_heater_7) + (inferensi_aturan_8 * output_heater_8) + (inferensi_aturan_9 * output_heater_9) + (inferensi_aturan_10 * output_heater_10) + (inferensi_aturan_11 * output_heater_11) + (inferensi_aturan_12 * output_heater_12)) / (inferensi_aturan_1 + inferensi_aturan_2 + inferensi_aturan_3 + inferensi_aturan_4 + inferensi_aturan_5 + inferensi_aturan_6 + inferensi_aturan_7 + inferensi_aturan_8 + inferensi_aturan_9 + inferensi_aturan_10 + inferensi_aturan_11 + inferensi_aturan_12));

  delay(1000);

  Serial.println("Output PWM Kipas: " + String(output_pwm_kipas));
  Serial.println("Output Durasi Pompa: " + String(output_durasi_pompa));
  Serial.println("Output Durasi Heater: " + String(output_durasi_heater));

  // konversi nilai float ke int, karena delay dan pwm hanya menerima int
  pwm_kipas = int(round(output_pwm_kipas));
  durasi_pompa = int(round(output_durasi_pompa));
  durasi_heater = int(round(output_durasi_heater));

  // update data state/kondisi aktuator ke firebase rtdb
  Firebase.RTDB.setBool(&fbdo, fanControlPath, true);
  Firebase.RTDB.setBool(&fbdo, pumpControlPath, true);
  Firebase.RTDB.setBool(&fbdo, heaterControlPath, true);
  Firebase.RTDB.setInt(&fbdo, fanPWMPath, pwm_kipas);

  if (fbdo.errorReason() != "") {
    Serial.println("Gagal Mengubah Data seluruh kendali kipas, pompa, dan heater: " + fbdo.errorReason());
  } else {
    Serial.println("Berhasil Mengubah Data seluruh kendali kipas, pompa, dan heater");
  }

  fbdo.clear();

  delay(1000);

  // display nilai perhitungan logika fuzzy ke LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Output Fuzzy");
  lcd.setCursor(0, 1);
  lcd.print("Fan PWM: ");
  lcd.print(pwm_kipas);
  lcd.print(" PWM");
  lcd.setCursor(0, 2);
  lcd.print("Pump: ");
  lcd.print(durasi_pompa);
  lcd.print(" ms");
  lcd.setCursor(0, 3);
  lcd.print("Heater: ");
  lcd.print(durasi_heater);
  lcd.print(" ms");

  delay(5000);

  // eksekusi
  if (suhu <= 24.00 || kelembapan >= 90.00) {
    if (durasi_heater > 30000) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Heater Nyala");
      lcd.setCursor(0, 1);
      lcd.print(durasi_heater / 60000);
      lcd.print(" menit ");
      lcd.print((durasi_heater % 60000) / 1000);
      lcd.print(" detik");
      Serial.println("Heater Menyala");
      digitalWrite(RELAY_HEATER_PIN, HIGH);
      delay(durasi_heater);
      digitalWrite(RELAY_HEATER_PIN, LOW);
    }

    delay(5000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pompa Nyala");
    lcd.setCursor(0, 1);
    lcd.print(durasi_pompa / 60000);
    lcd.print(" menit ");
    lcd.print((durasi_pompa % 60000) / 1000);
    lcd.print(" detik");
    lcd.setCursor(0, 2);
    lcd.print("Kipas Nyala");
    lcd.setCursor(0, 3);
    lcd.print(pwm_kipas);
    lcd.print(" PWM");
    Serial.println("Pompa dan Kipas Menyala");
    digitalWrite(RELAY_PUMP_PIN, HIGH);
    Serial.println("Kipas Menyala");
    ledcWrite(ENA_FAN_1, pwm_kipas);
    ledcWrite(ENA_FAN_2, pwm_kipas);
    delay(durasi_pompa);
    digitalWrite(RELAY_PUMP_PIN, LOW);
    ledcWrite(ENA_FAN_1, 0);
    ledcWrite(ENA_FAN_2, 0);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pompa Nyala");
    lcd.setCursor(0, 1);
    lcd.print(durasi_pompa / 60000);
    lcd.print(" menit ");
    lcd.print((durasi_pompa % 60000) / 1000);
    lcd.print(" detik");
    lcd.setCursor(0, 2);
    lcd.print("Kipas Nyala");
    lcd.setCursor(0, 3);
    lcd.print(pwm_kipas);
    lcd.print(" PWM");
    Serial.println("Pompa dan Kipas Menyala");
    digitalWrite(RELAY_PUMP_PIN, HIGH);
    Serial.println("Kipas Menyala");
    ledcWrite(ENA_FAN_1, pwm_kipas);
    ledcWrite(ENA_FAN_2, pwm_kipas);
    delay(durasi_pompa);
    digitalWrite(RELAY_PUMP_PIN, LOW);  
    ledcWrite(ENA_FAN_1, 0);
    ledcWrite(ENA_FAN_2, 0);

    delay(5000);

    if (durasi_heater > 40000) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Heater Nyala");
      lcd.setCursor(0, 1);
      lcd.print(durasi_heater / 60000);
      lcd.print(" menit ");
      lcd.print((durasi_heater % 60000) / 1000);
      lcd.print(" detik");
      Serial.println("Heater Menyala");
      digitalWrite(RELAY_HEATER_PIN, HIGH);
      delay(durasi_heater);
      digitalWrite(RELAY_HEATER_PIN, LOW);
    }
  }
  
  delay(2000);

  Serial.println("Pengendalian Selesai");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pengendalian Selesai");

  // update kembali data state/kondisi aktuator ke firebase rtdb
  Firebase.RTDB.setBool(&fbdo, fanControlPath, false);
  Firebase.RTDB.setBool(&fbdo, pumpControlPath, false);
  Firebase.RTDB.setBool(&fbdo, heaterControlPath, false);
  Firebase.RTDB.setInt(&fbdo, fanPWMPath, 0);

  if (fbdo.errorReason() != "") {
    Serial.println("Gagal Mengubah Data seluruh kendali kipas, pompa, dan heater: " + fbdo.errorReason());
  } else {
    Serial.println("Berhasil Mengubah Data seluruh kendali kipas, pompa, dan heater");
  }

  fbdo.clear();

  delay(5000);
  
  // update data suhu dan kelembapan di firebase
  KirimKondisiTerkini();

  delay(2000);

  // update state pengendalian_fuzzy menjadi false agar pengendalian menggunakan mobile app bisa berjalan
  pengendalian_fuzzy = false;

  Firebase.RTDB.setBool(&fbdo, fuzzyControlPath, pengendalian_fuzzy);
  if (fbdo.errorReason() != "") {
    Serial.println("Gagal Mengubah Data pengendalian_fuzzy menjadi false: " + fbdo.errorReason());
  } else {
    Serial.println("Berhasil Mengubah Data pengendalian_fuzzy menjadi false");
  }

  fbdo.clear();
}

void kendali_mobile() {
  // blok kode untuk mengambil data suhu dan kelembapan terkini apabila diperintahkan oleh mobile app
  if (Firebase.RTDB.get(&fbdo, sensorControlPath)) {
    if (fbdo.dataType() == "boolean") {
      bool valueSensorControl = fbdo.boolData();
      Serial.print("Value kendali_sensor: ");
      Serial.println(valueSensorControl);
        
      if (valueSensorControl) {
        KirimKondisiTerkini();

        Firebase.RTDB.setBool(&fbdo, sensorControlPath, false);

        if (fbdo.errorReason() != "") {
          Serial.println("Gagal Mengubah Data kendali_sensor menjadi false: " + fbdo.errorReason());
        } else {
          Serial.println("Berhasil Mengubah Data kendali_sensor menjadi false");
        }
      }
    } else {
      Serial.println("Data kendali_sensore yang diperoleh bukan boolean: " + fbdo.errorReason());
    }
  } else {
    Serial.println("Gagal Mendapatkan Data kendali_sensor: " + fbdo.errorReason());
  }
  
  delay(1000);

  // blok kode untuk melihat apakah ada perintah untuk menyalakan kipas
  if (Firebase.RTDB.get(&fbdo, fanControlPath)) {
    if (fbdo.dataType() == "boolean") {
      bool valueFanControl = fbdo.boolData();
      Serial.print("Value kendali_kipas: ");
      Serial.println(valueFanControl);

      if (valueFanControl) {
        if (Firebase.RTDB.get(&fbdo, fanPWMPath)) {
          if (fbdo.dataType() == "int") {
            int valueFanPWM = fbdo.intData();
            Serial.print("Value pwm_kipas: ");
            Serial.println(valueFanPWM);

            ledcWrite(ENA_FAN_1, valueFanPWM);
            ledcWrite(ENA_FAN_2, valueFanPWM);
            Serial.println("Kipas menyala dengan kecepatan " + valueFanPWM);
          } else {
            Serial.println("Data pwm_kipas yang diperoleh bukan int: " + fbdo.errorReason());
          }
        } else {
          Serial.println("Gagal Mendapatkan Data pwm_kipas: " + fbdo.errorReason());
        }
      } else {
        ledcWrite(ENA_FAN_1, 0);
        ledcWrite(ENA_FAN_2, 0);
      }
    } else {
      Serial.println("Data kendali_kipas yang diperoleh bukan boolean: " + fbdo.errorReason());
      ledcWrite(ENA_FAN_1, 0);
      ledcWrite(ENA_FAN_2, 0);
    }
  } else {
    Serial.println("Gagal Mendapatkan Data kendali_kipas: " + fbdo.errorReason());
    ledcWrite(ENA_FAN_1, 0);
    ledcWrite(ENA_FAN_2, 0);
  }

  delay(1000);

  // blok kode untuk melihat apakah ada perintah untuk menyalakan pompa
  if (Firebase.RTDB.get(&fbdo, pumpControlPath)) {
    if (fbdo.dataType() == "boolean") {
      bool valuePumpControl = fbdo.boolData();
      Serial.print("Value kendali_pompa: ");
      Serial.println(valuePumpControl);

      if (valuePumpControl) {
        digitalWrite(RELAY_PUMP_PIN, HIGH);
        Serial.println("Pompa menyala");
      } else {
        digitalWrite(RELAY_PUMP_PIN, LOW);
        Serial.println("Pompa dimatikan");
      }
    } else {
      Serial.println("Data kendali_pompa yang diperoleh bukan bool: " + fbdo.errorReason());
      digitalWrite(RELAY_PUMP_PIN, LOW);
    }
  } else {
    Serial.println("Gagal Mendapatkan Data kendali_pompa: " + fbdo.errorReason());
    digitalWrite(RELAY_PUMP_PIN, LOW);
  }

  delay(1000);

  // blok kode untuk melihat apakah ada perintah untuk menyalakan heater
  if (Firebase.RTDB.get(&fbdo, heaterControlPath)) {
    if (fbdo.dataType() == "boolean") {
      bool valueHeaterControl = fbdo.boolData();
      Serial.print("Value kendali_heater: ");
      Serial.println(valueHeaterControl);

      if (valueHeaterControl) {
        digitalWrite(RELAY_HEATER_PIN, HIGH);
        Serial.println("Heater menyala");
      } else {
        digitalWrite(RELAY_HEATER_PIN, LOW);
        Serial.println("Heater dimatikan");
      }
    } else {
      Serial.println("Data kendali_heater yang diperoleh bukan bool: " + fbdo.errorReason());
      digitalWrite(RELAY_HEATER_PIN, LOW);
    }
  } else {
    Serial.println("Gagal Mendapatkan Data kendali_heater: " + fbdo.errorReason());
    digitalWrite(RELAY_HEATER_PIN, LOW);
  }
    
  delay(1000);

  fbdo.clear();
}

// fungsi untuk fuzzyfikasi input suhu (himpunan dingin)
float fuzzyfikasi_suhu_dingin(float suhu) {
  if (suhu >= 24.00) {
    return 0.00;
  } else if (suhu <= 22.00) {
    return 1.00;
  } else if (suhu > 22 && suhu < 24) {
    return ((24 - suhu) / (24 - 22));
  }
  return 0.00;
}

// fungsi untuk fuzzyfikasi input suhu (himpunan ideal)
float fuzzyfikasi_suhu_ideal(float suhu) {
  if (suhu <= 22 || suhu >= 29) {
    return 0.00;
  } else if (suhu >= 24 && suhu <= 27) {
    return 1.00;
  } else if (suhu > 22 && suhu < 24) {
    return ((suhu - 22) / (24 - 22));
  } else if (suhu > 27 && suhu < 29) {
    return ((29 - suhu) / (29 - 27));
  }
  return 0.00;
}

// fungsi untuk fuzzyfikasi input suhu (himpunan panas)
float fuzzyfikasi_suhu_panas(float suhu) {
  if (suhu <= 27) {
    return 0.00;
  } else if (suhu >= 29) {
    return 1.00;
  } else if (suhu > 27 && suhu < 29) {
    return ((29 - suhu) / (29 - 27));
  }
  return 0.00;
}

// fungsi untuk fuzzyfikasi input kelembapan (himpunan rendah)
float fuzzyfikasi_kelembapan_rendah(float kelembapan) {
  if (kelembapan >= 60) {
    return 0.00;
  } else if (kelembapan <= 55) {
    return 1.00;
  } else if (kelembapan > 55 && kelembapan < 60) {
    return ((60 - kelembapan) / (60 - 55));
  }
  return 0.00;
}

// fungsi untuk fuzzyfikasi input kelembapan (himpunan sedang)
float fuzzyfikasi_kelembapan_sedang(float kelembapan) {
  if (kelembapan <= 55 || kelembapan >= 80) {
    return 0.00;
  } else if (kelembapan >= 60 && kelembapan <= 75) {
    return 1.00;
  } else if (kelembapan > 55 && kelembapan < 60) {
    return ((kelembapan - 55) / (60 - 55));
  } else if (kelembapan > 75 && kelembapan < 80) {
    return ((80 - kelembapan) / (80 - 75));
  }
  return 0.00;
}

// fungsi untuk fuzzyfikasi input kelembapan (himpunan ideal)
float fuzzyfikasi_kelembapan_ideal(float kelembapan) {
  if (kelembapan <= 75 || kelembapan >= 95) {
    return 0.00;
  } else if (kelembapan >= 80 && kelembapan <= 90) {
    return 1.00;
  } else if (kelembapan > 75 && kelembapan < 80) {
    return ((kelembapan - 75) / (80 - 75));
  } else if (kelembapan > 90 && kelembapan < 95) {
    return ((95 - kelembapan) / (95 - 90));
  }
  return 0.00;
}

// fungsi untuk fuzzyfikasi input kelembapan (himpunan tinggi)
float fuzzyfikasi_kelembapan_tinggi(float kelembapan) {
  if (kelembapan <= 90) {
    return 0.00;
  } else if (kelembapan >= 95) {
    return 1.00;
  } else if (kelembapan > 90 && kelembapan < 95) {
    return ((95 - kelembapan) / (95 - 90));
  }
  return 0.00;
}

// fungsi untuk memperoleh output crisp kipas (himpunan pelan)
float crisp_kipas_pelan(float inferensi) {
  if (inferensi == 0.00) {
    return 140.00;
  } else if (inferensi == 1.00) {
    return 60.00;
  } else {
    return (140.00 - (inferensi * 80.00));
  }
}

// fungsi untuk memperoleh output crisp kipas (himpunan agak cepat)
float crisp_kipas_agak_cepat(float inferensi) {
  if (inferensi == 0.00) {
    return 60.00;
  } else if (inferensi == 1.00) {
    return 140.00;
  } else {
    return ((inferensi * 80.00) + 60.00);
  }
}

// fungsi untuk memperoleh output crisp kipas (himpunan cepat)
float crisp_kipas_cepat(float inferensi) {
  if (inferensi == 0.00) {
    return 140.00;
  } else if (inferensi == 1.00) {
    return 220.00;
  } else {
    return ((inferensi * 80.00) + 140.00);
  }
}

// fungsi untuk memperoleh output crisp pompa (himpunan sangat sebentar)
float crisp_pompa_sangat_sebentar(float inferensi) {
  if (inferensi == 0.00) {
    return 180000.00;
  } else if (inferensi == 1.00) {
    return 90000.00;
  } else {
    return (180000.00 - (inferensi * 90000.00));
  }
}

// fungsi untuk memperoleh output crisp pompa (himpunan agak sebentar)
float crisp_pompa_agak_sebentar(float inferensi) {
  if (inferensi == 0.00) {
    return 90000.00;
  } else if (inferensi == 1.00) {
    return 180000.00;
  } else {
    return ((inferensi * 90000.00) + 90000.00);
  }
}

// fungsi untuk memperoleh output crisp pompa (himpunan sebentar (sisi kiri kurva))
float crisp_pompa_sebentar_sisi_kiri(float inferensi) {
  if (inferensi == 0.00) {
    return 180000.00;
  } else if (inferensi == 1.00) {
    return 300000.00;
  } else {
    return ((inferensi * 120000.00) + 180000.00);
  }
}

// fungsi untuk memperoleh output crisp pompa (himpunan sebentar (sisi kanan kurva))
float crisp_pompa_sebentar_sisi_kanan(float inferensi) {
  if (inferensi == 0.00) {
    return 180000.00;
  } else if (inferensi == 1.00) {
    return 300000.00;
  } else {
    return ((inferensi * 120000.00) + 300000.00);
  }
}

// fungsi untuk memperoleh output crisp pompa (himpunan agak lama (sisi kiri kurva fuzzy))
float crisp_pompa_agak_lama_sisi_kiri(float inferensi) {
  if (inferensi == 0.00) {
    return 300000.00;
  } else if (inferensi == 1.00) {
    return 420000.00;
  } else {
    return ((inferensi * 120000.00) + 300000.00);
  }
}

// fungsi untuk memperoleh output crisp pompa (himpunan agak lama (sisi kanan kurva fuzzy))
float crisp_pompa_agak_lama_sisi_kanan(float inferensi) {
  if (inferensi == 0.00) {
    return 300000.00;
  } else if (inferensi == 1.00) {
    return 420000.00;
  } else {
    return ((inferensi * 120000.00) + 420000.00);
  }
}

// fungsi untuk memperoleh output crisp pompa (himpunan lama)
float crisp_pompa_lama(float inferensi) {
  if (inferensi == 0.00) {
    return 420000.00;
  } else if (inferensi == 1.00) {
    return 540000.00;
  } else {
    return ((inferensi * 120000.00) + 420000.00);
  }
}

// fungsi untuk memperoleh output crisp heater (himpunan pelan)
float crisp_heater_mati(float inferensi) {
  if (inferensi == 0.00) {
    return 40000.00;
  } else if (inferensi == 1.00) {
    return 30000.00;
  } else {
    return (40000.00 - (inferensi * 10000.00));
  }
}

// fungsi untuk memperoleh output crisp heater (himpunan sangat sebentar)
float crisp_heater_sangat_sebentar(float inferensi) {
  if (inferensi == 0.00) {
    return 30000.00;
  } else if (inferensi == 1.00) {
    return 65000.00;
  } else {
    return ((inferensi * 35000.00) + 30000.00);
  }
}

// fungsi untuk memperoleh output crisp heater (himpunan sebentar)
float crisp_heater_sebentar(float inferensi) {
  if (inferensi == 0.00) {
    return 90000.00;
  } else if (inferensi == 1.00) {
    return 135000.00;
  } else {
    return ((inferensi * 45000.00) + 135000.00);
  }
}

// fungsi untuk memperoleh output crisp heater (himpunan agak lama)
float crisp_heater_agak_lama(float inferensi) {
  if (inferensi == 0.00) {
    return 170000.00;
  } else if (inferensi == 1.00) {
    return 220000.00;
  } else {
    return ((inferensi * 50000.00) + 220000.00);
  }
}

// fungsi untuk memperoleh output crisp heater (himpunan lama)
float crisp_heater_lama(float inferensi) {
  if (inferensi == 0.00) {
    return 260000.00;
  } else if (inferensi == 1.00) {
    return 270000.00;
  } else {
    return ((inferensi * 10000.00) + 260000.00);
  }
}

 // fungsi untuk memantau kondisi terkini dan menampilkannya pada LCD agar tetap bisa terupdate
void cekKondisiTerkini() {
  suhu = dht.readTemperature();
  kelembapan = dht.readHumidity();

  if (isnan(suhu) || isnan(kelembapan)) {
    Serial.println("Gagal Mendeteksi Suhu dan Kelembapan");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gagal Memantau Kondisi");
    now = rtc.now();
    sprintf(tanggal, "%02d/%02d/%04d", now.day(), now.month(), now.year());
    sprintf(waktu, "%02d.%02d", now.hour(), now.minute());
    timestamp = now.unixtime();
  } else {
    suhu = suhu - kalibrasi_suhu;
    kelembapan = round(kelembapan + kalibrasi_kelembapan);
    now = rtc.now();
    sprintf(tanggal, "%02d/%02d/%04d", now.day(), now.month(), now.year());
    sprintf(waktu, "%02d.%02d", now.hour(), now.minute());
    timestamp = now.unixtime();
  }

  Serial.println("----- KONDISI TERKINI -----");
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

  delay(1000);

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

 // fungsi untuk mengambil data kondisi setelah pengendalian
void KirimKondisiTerkini() {
  suhu = dht.readTemperature();
  kelembapan = dht.readHumidity();

  if (isnan(suhu) || isnan(kelembapan)) {
    Serial.println("Gagal Mendeteksi Suhu dan Kelembapan");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gagal Memantau Kondisi");
    suhu = suhu_terakhir;
    kelembapan = kelembapan_terakhir;
    now = rtc.now();
    sprintf(tanggal, "%02d/%02d/%04d", now.day(), now.month(), now.year());
    sprintf(waktu, "%02d.%02d", now.hour(), now.minute());
    timestamp = now.unixtime();
  } else {
    suhu = suhu - kalibrasi_suhu;
    kelembapan = round(kelembapan + kalibrasi_kelembapan);
    suhu_terakhir = suhu;
    kelembapan_terakhir = kelembapan;
    now = rtc.now();
    sprintf(tanggal, "%02d/%02d/%04d", now.day(), now.month(), now.year());
    sprintf(waktu, "%02d.%02d", now.hour(), now.minute());
    timestamp = now.unixtime();
  }

  Serial.println("----- KONDISI TERKINI -----");
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

  delay(1000);

  // mengirimkan data log terbaru ke Database
  parentPath = dataLogPath + "/" + String(timestamp);
  json.set(tempPath.c_str(), suhu);
  json.set(humPath.c_str(), kelembapan);
  json.set(datePath.c_str(), tanggal);
  json.set(timePath.c_str(), waktu);

  Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json);

  if (fbdo.errorReason() != "") {
    Serial.println("Gagal Mengirim Data Terbaru Suhu dan Kelembapan: " + fbdo.errorReason());
  } else {
    Serial.println("Berhasil Mengirim Data Terbaru Suhu dan Kelembapan");
  }

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

