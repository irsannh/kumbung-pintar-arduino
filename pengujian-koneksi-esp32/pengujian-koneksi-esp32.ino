#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <LiquidCrystal_I2C.h>

const char* ssid = "nama wifi";
const char* password = "password";

int lcdColumns = 20;
int lcdRows = 4;

unsigned long waktuMulaiKoneksi;
unsigned long waktuBerhasilTerkoneksi;
unsigned long totalWaktuKoneksiAwal;
unsigned long waktuMulaiKoneksiUlang;
unsigned long waktuBerhasilTerkoneksiUlang;
unsigned long totalWaktuKoneksiUlang;
bool disconnectWifi = false;

LiquidCrystal_I2C lcd (0x27, lcdColumns, lcdRows);

WebServer server(80);

unsigned long ota_progress_millis = 0;

void onOTAStart() {
  Serial.println("OTA Update Started!");
}

void onOTAProgress(size_t current, size_t final) {
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current?: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA Update!");
  }
}

void setup(void) {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Mencoba Terhubung");
  lcd.print("Ke WiFi");
  waktuMulaiKoneksi = millis();
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  waktuBerhasilTerkoneksi = millis();
  totalWaktuKoneksiAwal = waktuBerhasilTerkoneksi - waktuMulaiKoneksi;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Berhasil Terhubung");
  lcd.setCursor(0, 1);
  lcd.print("waktu: ");
  lcd.print(totalWaktuKoneksiAwal);
  lcd.print(" ms");
  lcd.setCursor(0, 2);
  lcd.print("Status: Connect");

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", []() {
    server.send(200, "text/plain", "Hai! Ini adalah perangkat ESP32 untuk Sistem Pemantauan dan Pengendalian Pada Budidaya Jamur Tiram Berbasis Internet of Things Menggunakan Logika Fuzzy oleh Irsan Nur Hidayat (20190801171).");
  });

  ElegantOTA.begin(&server);
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  ElegantOTA.loop();
  if (WiFi.status() != WL_CONNECTED) {
    if (!disconnectWifi) {
      disconnectWifi = true;
      waktuMulaiKoneksiUlang = millis();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Mencoba Connect Lagi");
    }
  } else {
    if (disconnectWifi) {
      disconnectWifi = false;
      waktuBerhasilTerkoneksiUlang = millis();
      totalWaktuKoneksiUlang = waktuBerhasilTerkoneksiUlang - waktuMulaiKoneksiUlang;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Connect Lagi");
      lcd.setCursor(0, 1);
      lcd.print("waktu: ");
      lcd.print(totalWaktuKoneksiUlang);
      lcd.print(" ms");
      lcd.setCursor(0, 2);
      lcd.print("Status: Reconnect");
    }
  }
  delay(100);
}
