/*
 * ================================================================================
 *  P1 Smart Meter Monitor - ESP8266 + OLED
 *  Villanyóra P1 portjának olvasása, adatok MQTT-re küldése és webes megjelenítése
 *
 *  Hardver: ESP8266 + SSD1306 OLED (128x64)
 *  Forráskód: https://github.com/GombyG/p1_meter_to_MQTT
 *
 *  Fő funkciók:
 *  - P1 telegram olvasása (115200 baud, invertált RX)
 *  - OBIS kódok feldolgozása és MQTT publikálás
 *  - OLED kijelző (fogyasztás + WiFi/MQTT státusz)
 *  - Webes felület kártyákkal + kedvencek (localStorage)
 *  - OTA frissítés (ArduinoOTA + GitHub automatikus frissítés)
 *  - Captive Portal + WiFi konfiguráció mentése SPIFFS-be
 *  - Menü a gombbal (IP, Firmware keresés, WiFi reset)
 * ================================================================================
 */

// ================================================================================
//  INCLUDE-OK
// ================================================================================
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <U8g2lib.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <DNSServer.h>          // Captive Portal-hoz
#include "latest_fw_version.h"  // verziószám
#include "secrets.h"            // Titkos adatok (MQTT, GitHub, OTA jelszó stb.)
#include "wifi_management.h"    // WiFi AP / konfiguráció kezelés


// ================================================================================
//  HARDVERES PIN DEFINÍCIÓK
// ================================================================================
#define OLED_SDA            14      // D5
#define OLED_SCL            12      // D6
#define OLED_RESET          U8X8_PIN_NONE
#define MODE_BTN_PIN        0       // D3 (GPIO0) - Reset / Menü gomb

// ================================================================================
//  MŰKÖDÉSI IDŐZÍTÉSEK
// ================================================================================
#define P1_READ_INTERVAL_MS     10000UL     // OLED frissítés 10 másodpercenként
#define FW_CHECK_INTERVAL_MS    600000UL    // Firmware ellenőrzés 10 percenként

// ================================================================================
//  MQTT ÉS OTA BEÁLLÍTÁSOK (secrets.h-ból)
// ================================================================================
#define MQTT_SERVER         SECRET_MQTT_SERVER
#define MQTT_PORT           SECRET_MQTT_PORT
#define MQTT_USER           SECRET_MQTT_USER
#define MQTT_PASSWORD       SECRET_MQTT_PASSWORD
#define MQTT_BASE_TOPIC     SECRET_MQTT_BASE_TOPIC

#define OTA_USER            SECRET_OTA_USER
#define OTA_PASSWORD        SECRET_OTA_PASSWORD

// ================================================================================
//  LOGO BITMAP (128x48 pixel)
// ================================================================================
static const unsigned char logo_bits[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
  0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xf0, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x07, 0x00, 0x00,
  0x00, 0x00, 0x02, 0x00, 0x00, 0xf0, 0xff, 0x01, 0x00, 0x00, 0x00, 0xff,
  0xff, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0x7f, 0x00, 0x00, 0xfc, 0xff, 0x07,
  0x00, 0x00, 0xc0, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x03,
  0x00, 0xff, 0xff, 0x1f, 0x00, 0x00, 0xf0, 0xff, 0xe0, 0xff, 0x01, 0x00,
  0x00, 0xff, 0xff, 0x07, 0x80, 0xff, 0xff, 0x1f, 0x00, 0x00, 0xfc, 0x3f,
  0x80, 0xff, 0x07, 0x00, 0x80, 0xff, 0xff, 0x0f, 0xc0, 0xff, 0xff, 0x0f,
  0x00, 0x00, 0xff, 0x0f, 0x00, 0xfe, 0x1f, 0x00, 0xc0, 0xff, 0xff, 0x07,
  0xe0, 0xff, 0xf3, 0x07, 0x00, 0xc0, 0xff, 0x03, 0x00, 0xf8, 0x7f, 0x00,
  0xc0, 0xff, 0xf8, 0x03, 0xe0, 0x7f, 0x00, 0x03, 0x00, 0xf0, 0xff, 0x00,
  0x00, 0xe0, 0xff, 0x01, 0xe0, 0x3f, 0xc0, 0x01, 0xf0, 0x1f, 0x00, 0x00,
  0x00, 0xfc, 0x3f, 0x00, 0x00, 0x80, 0xff, 0x07, 0xe0, 0x1f, 0x00, 0x00,
  0xf0, 0x0f, 0x00, 0x00, 0x00, 0xff, 0x0f, 0x00, 0x00, 0x00, 0xfe, 0x1f,
  0xe0, 0x1f, 0x00, 0x00, 0xf8, 0x0f, 0x00, 0x00, 0xc0, 0xff, 0x03, 0x00,
  0x00, 0x00, 0xf8, 0xff, 0xe0, 0x7f, 0x00, 0x00, 0xf8, 0x07, 0x00, 0x00,
  0xe0, 0xff, 0x01, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xc1, 0xff, 0x01, 0x00,
  0xfc, 0x07, 0x00, 0x00, 0xc0, 0xff, 0x01, 0x00, 0x00, 0x00, 0xe0, 0xff,
  0xc0, 0xff, 0x0f, 0x00, 0xfc, 0x07, 0xfc, 0x3f, 0xc0, 0xff, 0x01, 0x00,
  0x00, 0x00, 0xe0, 0xff, 0x80, 0xff, 0x7f, 0x00, 0xfc, 0x07, 0xfc, 0x3f,
  0x80, 0xf9, 0x01, 0xf8, 0xff, 0x00, 0xe0, 0x7f, 0x00, 0xff, 0xff, 0x01,
  0xfc, 0x07, 0xfc, 0x3f, 0x00, 0xf8, 0x01, 0xf8, 0xff, 0x03, 0xe0, 0x07,
  0x00, 0xfe, 0xff, 0x07, 0xfc, 0x07, 0xfc, 0x3f, 0x00, 0xf8, 0x01, 0xf8,
  0xff, 0x07, 0xe0, 0x07, 0x00, 0xf8, 0xff, 0x0f, 0xfc, 0x07, 0xfc, 0x3f,
  0x00, 0xf8, 0x01, 0xf8, 0xf3, 0x07, 0xe0, 0x07, 0x00, 0xc0, 0xff, 0x0f,
  0xf8, 0x07, 0xfc, 0x3f, 0x00, 0xf8, 0x01, 0xf8, 0xc1, 0x0f, 0xe0, 0x07,
  0x00, 0x00, 0xfe, 0x1f, 0xf8, 0x0f, 0x80, 0x3f, 0x00, 0xf8, 0x01, 0xf8,
  0xc1, 0x07, 0xe0, 0x07, 0x00, 0x00, 0xf8, 0x1f, 0xf8, 0x0f, 0x80, 0x3f,
  0x00, 0xf8, 0x01, 0xf8, 0xc1, 0x07, 0xe0, 0x07, 0x00, 0x00, 0xe0, 0x1f,
  0xf0, 0x1f, 0x80, 0x3f, 0x00, 0xf8, 0x01, 0xf8, 0xff, 0x03, 0xe0, 0x07,
  0x00, 0x01, 0xe0, 0x1f, 0xf0, 0x3f, 0x80, 0x3f, 0x00, 0xf8, 0x01, 0xf8,
  0xff, 0x01, 0xe0, 0x07, 0x80, 0x07, 0xe0, 0x1f, 0xe0, 0xff, 0x80, 0x3f,
  0x00, 0xf8, 0x01, 0xf8, 0xff, 0x03, 0xe0, 0x07, 0xc0, 0x1f, 0xf8, 0x1f,
  0xe0, 0xff, 0xff, 0x3f, 0x00, 0xf8, 0x01, 0xf8, 0xff, 0x07, 0xe0, 0x07,
  0xe0, 0xff, 0xff, 0x0f, 0xc0, 0xff, 0xff, 0x3f, 0x00, 0xf8, 0x01, 0xf8,
  0xc1, 0x0f, 0xe0, 0x07, 0xf0, 0xff, 0xff, 0x0f, 0x00, 0xff, 0xff, 0x3f,
  0x00, 0xf8, 0x01, 0xf8, 0x81, 0x1f, 0xe0, 0x07, 0xe0, 0xff, 0xff, 0x07,
  0x00, 0xfe, 0xff, 0x0f, 0x00, 0xf8, 0x01, 0xf8, 0x81, 0x1f, 0xe0, 0x07,
  0x80, 0xff, 0xff, 0x01, 0x00, 0xf8, 0xff, 0x03, 0x00, 0xf8, 0x01, 0xf8,
  0x81, 0x0f, 0xe0, 0x07, 0x00, 0xfe, 0xff, 0x00, 0x00, 0x80, 0x3f, 0x00,
  0x00, 0xf8, 0x01, 0xf8, 0xff, 0x0f, 0xe0, 0x07, 0x00, 0xe0, 0x0f, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x01, 0xf8, 0xff, 0x07, 0xe0, 0x07,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x01, 0xf8,
  0xff, 0x03, 0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xf8, 0x01, 0xf8, 0x7f, 0x00, 0xe0, 0x07, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x01, 0x00, 0x00, 0x00, 0xe0, 0x07,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00,
  0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0xe0, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00,
  0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ================================================================================
//  GLOBÁLIS OBJEKTUMOK
// ================================================================================
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RESET, OLED_SCL, OLED_SDA);

ESP8266WebServer server(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
extern DNSServer dnsServer;               // wifi_management.h-ból


// ================================================================================
//  GLOBÁLIS VÁLTOZÓK
// ================================================================================
const float CURRENT_VERSION = LATEST_VERSION;
const char* wifiConfigPath = "/wifi.txt";
const char* UPDATE_FLAG_FILE = "/update_pending.txt";
unsigned long lastP1ReadTime   = 0;
unsigned long lastFwCheckTime  = 0;
String lastPowerBE             = "0";     // Pillanatnyi import teljesítmény
String lastPowerKI             = "0";     // Pillanatnyi export teljesítmény
String chipId                  = "";
String mqttBaseTopic           = "";

String savedMqttServer         = "";
int    savedMqttPort           = MQTT_PORT;
String savedMqttUser           = "";
String savedMqttPass           = "";
String savedOtaUser            = "";
String savedOtaPass            = "";
bool inMenuMode                = false;
int  selectedMenuItem          = 1;
unsigned long lastMenuActivity = 0;
bool startupUpdateChecked      = false;

String tempObisData            = "";      // Ideiglenes puffer az aktuális telegram kártyáihoz
String allObisData             = "";      // A webes felületen megjelenő teljes HTML

// ================================================================================
//  ELŐRE DEKLARÁCIÓK
// ================================================================================
void addObisCard(const String& rawCode, String value);
void checkForUpdates();
void drawMyMenu();
void updateOledDisplay();
void handleP1Port();
void handleMQTTConnection();
void publishMetric(const char* subtopic, const String& value);
void showLogo(uint32_t displayTimeMs);
void handleResetButton();
void parseP1Telegram(String telegram);
String formatP1Time(String rawTime);
String getObisName(String code);
String formatWatt(const String& kwString);
String getFormattedTime();
void waitForNtpSync();
int getWifiQuality();
bool isAPMode = false;
// ================================================================================
//  SEGÉDFÜGGVÉNYEK – IDŐ ÉS FORMÁZÁS
// ================================================================================

/**
 * P1 időbélyeg formázása: YYMMDDhhmmss → 20YY.MM.DD hh:mm:ss
 */
String formatP1Time(String rawTime) {
  if (rawTime.length() < 12) return rawTime;

  String yy  = rawTime.substring(0, 2);
  String mm  = rawTime.substring(2, 4);
  String dd  = rawTime.substring(4, 6);
  String hh  = rawTime.substring(6, 8);
  String min = rawTime.substring(8, 10);
  String ss  = rawTime.substring(10, 12);

  return "20" + yy + "." + mm + "." + dd + " " + hh + ":" + min + ":" + ss;
}

/**
 * kW érték átalakítása Watt-ra (pl. "1.234" → "1234 W")
 */
String formatWatt(const String& kwString) {
  if (kwString.length() == 0 || kwString == "N/A") return "0 W";

  float kw = kwString.toFloat();
  int watt = round(kw * 1000.0);
  return String(watt) + " W";
}

/**
 * Aktuális idő lekérése NTP alapján
 */
String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  if (timeinfo->tm_year < (2020 - 1900)) {
    return "ONLINE (NTP hiányzik)";
  }

  char timeBuffer[25];
  strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(timeBuffer);
}

/**
 * NTP szinkron várása (max. 3 másodperc)
 */
void waitForNtpSync() {
  int timeout = 30;
  while (time(nullptr) < 1000000000 && timeout > 0) {
    delay(100);
    timeout--;
  }
}

/**
 * WiFi térerő 0-100% skálára átszámítva
 */
int getWifiQuality() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  long rssi = WiFi.RSSI();
  if (rssi <= -100) return 0;
  if (rssi >= -50)  return 100;
  return 2 * (rssi + 100);   // -100 dBm = 0%, -50 dBm = 100%
}

// ================================================================================
//  OBIS KÓD NEVEK ÉS KÁRTYA GENERÁLÁS
// ================================================================================

/**
 * OBIS kód → magyar megnevezés
 */
String getObisName(String code) {
  code.trim();

  #define IS_OBIS(c, target) (c == target || c == target ".255")

  // Feszültségek és áramok
  if (IS_OBIS(code, "32.7.0")) return "Pillanatnyi feszültség L1";
  if (IS_OBIS(code, "52.7.0")) return "Pillanatnyi feszültség L2";
  if (IS_OBIS(code, "72.7.0")) return "Pillanatnyi feszültség L3";
  if (IS_OBIS(code, "31.7.0")) return "Pillanatnyi áram L1";
  if (IS_OBIS(code, "51.7.0")) return "Pillanatnyi áram L2";
  if (IS_OBIS(code, "71.7.0")) return "Pillanatnyi áram L3";

  // Általános és állapot kódok
  if (IS_OBIS(code, "1.0.0"))   return "Idő";
  if (IS_OBIS(code, "42.0.0"))  return "COSEM logikai eszköznév";
  if (IS_OBIS(code, "96.1.0"))  return "Mérő gyári szám";
  if (IS_OBIS(code, "96.14.0")) return "Aktuális tarifa";
  if (IS_OBIS(code, "96.50.68") || IS_OBIS(code, "96.3.10")) return "Megszakító státusz";
  if (IS_OBIS(code, "17.0.0"))  return "Limiter határérték";
  if (IS_OBIS(code, "96.13.0")) return "Áramszolgáltatói üzenet";

  // Hatásos import energiák (+A)
  if (IS_OBIS(code, "1.8.0")) return "Hatásos import energia (+A) totál";
  if (IS_OBIS(code, "1.8.1")) return "Hatásos import energia T1";
  if (IS_OBIS(code, "1.8.2")) return "Hatásos import energia T2";
  if (IS_OBIS(code, "1.8.3")) return "Hatásos import energia T3";
  if (IS_OBIS(code, "1.8.4")) return "Hatásos import energia T4";

  // Hatásos export energiák (-A)
  if (IS_OBIS(code, "2.8.0")) return "Hatásos export energia (-A) totál";
  if (IS_OBIS(code, "2.8.1")) return "Hatásos export energia T1";
  if (IS_OBIS(code, "2.8.2")) return "Hatásos export energia T2";
  if (IS_OBIS(code, "2.8.3")) return "Hatásos export energia T3";
  if (IS_OBIS(code, "2.8.4")) return "Hatásos export energia T4";

  // Kombinált és meddő energiák
  if (IS_OBIS(code, "15.8.0")) return "Abszolút hatásos energia (|+A|+|-A|)";
  if (IS_OBIS(code, "3.8.0"))  return "Import meddő energia (+R)";
  if (IS_OBIS(code, "4.8.0"))  return "Export meddő energia (-R)";
  if (IS_OBIS(code, "5.8.0"))  return "Meddő energia QI (Import ind.)";
  if (IS_OBIS(code, "6.8.0"))  return "Meddő energia QII (Import kap.)";
  if (IS_OBIS(code, "7.8.0"))  return "Meddő energia QIII (Export ind.)";
  if (IS_OBIS(code, "8.8.0"))  return "Meddő energia QIV (Export kap.)";

  // Pillanatnyi teljesítmények
  if (IS_OBIS(code, "1.7.0"))  return "Pillanatnyi import teljesítmény (+A)";
  if (IS_OBIS(code, "2.7.0"))  return "Pillanatnyi export teljesítmény (-A)";
  if (IS_OBIS(code, "21.7.0")) return "Import teljesítmény (+A) L1";
  if (IS_OBIS(code, "41.7.0")) return "Import teljesítmény (+A) L2";
  if (IS_OBIS(code, "61.7.0")) return "Import teljesítmény (+A) L3";
  if (IS_OBIS(code, "22.7.0")) return "Export teljesítmény (-A) L1";
  if (IS_OBIS(code, "42.7.0")) return "Export teljesítmény (-A) L2";
  if (IS_OBIS(code, "62.7.0")) return "Export teljesítmény (-A) L3";

  // Pillanatnyi meddő teljesítmények
  if (IS_OBIS(code, "5.7.0")) return "Pillanatnyi meddő QI";
  if (IS_OBIS(code, "6.7.0")) return "Pillanatnyi meddő QII";
  if (IS_OBIS(code, "7.7.0")) return "Pillanatnyi meddő QIII";
  if (IS_OBIS(code, "8.7.0")) return "Pillanatnyi meddő QIV";

  // Áram korlátozás határértékek
  if (IS_OBIS(code, "31.4.0")) return "Áram korlátozás küszöb L1";
  if (IS_OBIS(code, "51.4.0")) return "Áram korlátozás küszöb L2";
  if (IS_OBIS(code, "71.4.0")) return "Áram korlátozás küszöb L3";

  // Teljesítménytényezők és frekvencia
  if (IS_OBIS(code, "13.7.0")) return "Teljesítmény tényező (CosPhi)";
  if (IS_OBIS(code, "33.7.0")) return "Teljesítmény tényező L1";
  if (IS_OBIS(code, "53.7.0")) return "Teljesítmény tényező L2";
  if (IS_OBIS(code, "73.7.0")) return "Teljesítmény tényező L3";
  if (IS_OBIS(code, "14.7.0")) return "Hálózati frekvencia";

  // Egyéb
  if (code == "WIFI_signal") return "Wi-Fi térerő";

  #undef IS_OBIS
  return code;   // Ha nincs ismert név, a kódot adjuk vissza
}

/**
 * Egy OBIS érték HTML kártyává alakítása és a tempObisData pufferhez fűzése
 */
void addObisCard(const String& rawCode, String value) {
  String cleanCode = rawCode;
  cleanCode.trim();
  if (cleanCode.length() == 0 || value.length() == 0) return;

  String obisName = getObisName(cleanCode);
  if (obisName.length() == 0) obisName = cleanCode;

  // Mértékegységek és formázás
  if (value.indexOf('*') != -1) {
    value.replace("*", " ");
  } else {
    if (cleanCode == "1.0.0") value = formatP1Time(value);
    else if (cleanCode.startsWith("1.8.") || cleanCode.startsWith("2.8.") || cleanCode.startsWith("15.8.")) value += " kWh";
    else if (cleanCode.startsWith("3.8.") || cleanCode.startsWith("4.8.") || cleanCode.startsWith("5.8.") || cleanCode.startsWith("6.8.")) value += " kvarh";
    else if (cleanCode == "32.7.0" || cleanCode == "52.7.0" || cleanCode == "72.7.0") value += " V";
    else if (cleanCode == "31.7.0" || cleanCode == "51.7.0" || cleanCode == "71.7.0" ||
             cleanCode == "31.4.0" || cleanCode == "51.4.0" || cleanCode == "71.4.0") value += " A";
    else if (cleanCode == "1.7.0" || cleanCode == "2.7.0" || cleanCode == "21.7.0" ||
             cleanCode == "41.7.0" || cleanCode == "61.7.0" || cleanCode == "22.7.0" ||
             cleanCode == "42.7.0" || cleanCode == "62.7.0") value += " kW";
    else if (cleanCode == "14.7.0") value += " Hz";
    else if (cleanCode == "WIFI_signal") value += " dBm";
  }

  // HTML kártya generálása
  tempObisData += "<div class='card' data-code='" + cleanCode + "'>";
  tempObisData += "<div class='card-title'>" + obisName + "</div>";
  tempObisData += "<span class='star' onclick='tG(\"" + cleanCode + "\", this)'>★</span>";
  tempObisData += "<div class='card-value'>" + value + "</div>";
  tempObisData += "<div class='card-code'>" + cleanCode + "</div>";
  tempObisData += "</div>";
}

// ================================================================================
//  MQTT KEZELÉS
// ================================================================================

/**
 * Egy metrika publikálása MQTT-re + HTML kártya generálása
 */
void publishMetric(const char* subtopic, const String& value) {
  if (value.length() == 0) return;

  // 1. MQTT küldés
  if (mqttClient.connected()) {
    String fullTopic = mqttBaseTopic + "/" + String(subtopic);
    mqttClient.publish(fullTopic.c_str(), value.c_str(), true);   // retained
  }

  // 2. HTML kártya generálása
  addObisCard(String(subtopic), value);
}

/**
 * MQTT kapcsolat kezelése (újracsatlakozás + periodikus térerő/verzió küldés)
 */
void handleMQTTConnection() {
  if (WiFi.status() != WL_CONNECTED) return;

  static unsigned long lastMqttRetry        = 0;
  static unsigned long lastWifiSignalPublish = 0;

  if (!mqttClient.connected()) {
    if (millis() - lastMqttRetry > 10000) {
      lastMqttRetry = millis();
      waitForNtpSync();

      String clientId = "ESP8266_P1_" + chipId;
      const char* user = (savedMqttUser.length() > 0) ? savedMqttUser.c_str() : nullptr;
      const char* pass = (savedMqttPass.length() > 0) ? savedMqttPass.c_str() : nullptr;

      if (mqttClient.connect(clientId.c_str(), user, pass)) {
        // Státusz és verzió publikálása
        String statusTopic = mqttBaseTopic + "/status";
        mqttClient.publish(statusTopic.c_str(), getFormattedTime().c_str(), true);

        String versionTopic = mqttBaseTopic + "/ESP_software_version";
        mqttClient.publish(versionTopic.c_str(), String(CURRENT_VERSION).c_str(), true);

        // === ÚJ: IP-cím publikálása MQTT-re (retained üzenetként) ===
        String ipTopic = mqttBaseTopic + "/ip_address";
        String currentIP = WiFi.localIP().toString();
        mqttClient.publish(ipTopic.c_str(), currentIP.c_str(), true);

        // Térerő azonnali küldése
        publishMetric("WIFI_signal", String(WiFi.RSSI()));
        lastWifiSignalPublish = millis();
      }
    }
  } else {
    mqttClient.loop();

    // Periodikus térerő és verzió küldés (60 mp-enként)
    if (millis() - lastWifiSignalPublish > 60000) {
      lastWifiSignalPublish = millis();
      publishMetric("WIFI_signal", String(WiFi.RSSI()));
      publishMetric("ip_address", WiFi.localIP().toString());

      String versionTopic = mqttBaseTopic + "/ESP_software_version";
      mqttClient.publish(versionTopic.c_str(), String(CURRENT_VERSION).c_str(), true);
    }
  }
}

// ================================================================================
//  P1 SOROS PORT KEZELÉS
// ================================================================================

/**
 * Egy teljes P1 telegram feldolgozása
 */
void parseP1Telegram(String telegram) {
  int startIdx = 0;
  bool inHistoricalBlock = false;

  while (startIdx < telegram.length()) {
    int endIdx = telegram.indexOf('\n', startIdx);
    if (endIdx == -1) endIdx = telegram.length();

    String line = telegram.substring(startIdx, endIdx);
    line.trim();

    // Történeti / elszámolási blokk kiszűrése (98.1.0)
    if (line.indexOf("98.1.0") != -1) {
      inHistoricalBlock = true;
    }
    if (inHistoricalBlock) {
      if (line.startsWith(")")) inHistoricalBlock = false;
      startIdx = endIdx + 1;
      continue;
    }

    // Érvénytelen sorok, fejlécek, CRC eldobása
    if (line.length() == 0 || line.startsWith("!") || line.startsWith("AUX")) {
      startIdx = endIdx + 1;
      continue;
    }

    // OBIS kód + érték szétválasztása
    int openParen = line.indexOf('(');
    if (openParen != -1) {
      String rawCode = line.substring(0, openParen);
      String rawVal  = line.substring(openParen + 1);

      if (rawVal.endsWith(")")) {
        rawVal = rawVal.substring(0, rawVal.length() - 1);
      }

      // Mértékegység levágása (* után)
      int starIdx = rawVal.indexOf('*');
      if (starIdx != -1) {
        rawVal = rawVal.substring(0, starIdx);
      }

      // Topic név tisztítása (1-0: és 0-0: előtagok eltávolítása)
      String topicName = rawCode;
      topicName.replace("1-0:", "");
      topicName.replace("0-0:", "");

      publishMetric(topicName.c_str(), rawVal);

      // Pillanatnyi teljesítmények mentése az OLED-hez
      if (rawCode.indexOf("1.7.0") != -1) lastPowerBE = rawVal;
      if (rawCode.indexOf("2.7.0") != -1) lastPowerKI = rawVal;
    }

    startIdx = endIdx + 1;
  }
}

/**
 * Nem-blokkoló P1 port olvasás
 */
void handleP1Port() {
  static String p1_telegram = "";
  static bool inTelegram = false;

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '/') {                 // Telegram kezdete
      inTelegram = true;
      p1_telegram = "";
      tempObisData = "";
    }

    if (inTelegram) {
      p1_telegram += c;

      if (c == '!') {               // Telegram vége
        addObisCard("WIFI_signal", String(WiFi.RSSI()));
        parseP1Telegram(p1_telegram);

        if (tempObisData.length() > 0) {
          allObisData = tempObisData;
          tempObisData = "";
        }

        inTelegram = false;
        break;
      }
    }
  }
}

// ================================================================================
//  OLED KEZELÉS
// ================================================================================

/**
 * Induló logo megjelenítése
 */
void showLogo(uint32_t displayTimeMs) {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.firstPage();
  do {
    u8g2.drawUTF8(0, 12, "GOMBY BUILDING SYSTEM");
    u8g2.drawXBMP(0, 18, 128, 48, logo_bits);
  } while (u8g2.nextPage());

  uint32_t startTime = millis();
  while (millis() - startTime < displayTimeMs) {
    yield();
  }
}

/**
 * Fő OLED képernyő frissítése
 */
void updateOledDisplay() {
  u8g2.clearBuffer();

  // Verzió
  u8g2.drawUTF8(0, 10, (String("P1 Monitor v.") + String(CURRENT_VERSION, 2)).c_str());

  // Kapcsolati státusz
  if (WiFi.status() == WL_CONNECTED) {
    // Van kapcsolat → normál megjelenítés
    String wifiStatus = String(getWifiQuality()) + "%";
    String mqttStatus = mqttClient.connected() ? "OK" : "??";
    u8g2.drawUTF8(0, 24, ("WiFi:" + wifiStatus + " MQTT:" + mqttStatus).c_str());
  } else {
    // Nincs kapcsolat → OFF-LINE MÓD
    u8g2.drawUTF8(0, 24, "OFF-LINE mód");
  }

  // Fogyasztási adatok (ezek offline-ban is kellenek)
  u8g2.drawUTF8(0, 40, ("Power BE: " + formatWatt(lastPowerBE)).c_str());
  u8g2.drawUTF8(0, 58, ("Power KI: " + formatWatt(lastPowerKI)).c_str());

  u8g2.sendBuffer();
}

// ================================================================================
//  MENÜ ÉS RESET GOMB
// ================================================================================

/**
 * Menü kirajzolása
 */
void drawMyMenu() {
  u8g2.clearBuffer();
  u8g2.drawUTF8(16, 10, "Menü");

  if (selectedMenuItem == 1) u8g2.drawUTF8(0, 25, "->");
  u8g2.drawUTF8(16, 25, "1. IP cím");

  if (selectedMenuItem == 2) u8g2.drawUTF8(0, 40, "->");
  u8g2.drawUTF8(16, 40, "2. WIFI adatok");
  
  if (selectedMenuItem == 3) u8g2.drawUTF8(0, 55, "->");
  u8g2.drawUTF8(16, 55, "3. WIFI reset");

  u8g2.sendBuffer();
}

/**
 * Reset / Menü gomb kezelése
 * - Rövid nyomás menüben: léptetés
 * - Hosszú nyomás (>1s): menübe lépés / funkció indítása
 */
void handleResetButton() {
  static unsigned long pressStartTime = 0;
  static bool longPressExecuted = false;
  static bool lastButtonState = HIGH;

  bool currentButtonState = digitalRead(MODE_BTN_PIN);

  // Menü timeout (10 másodperc)
  if (inMenuMode && (millis() - lastMenuActivity >= 10000)) {
    inMenuMode = false;
    updateOledDisplay();
  }

  // Gomb lenyomás kezdete
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    delay(50);   // Debounce
    if (digitalRead(MODE_BTN_PIN) == LOW) {
      pressStartTime = millis();
      longPressExecuted = false;
    }
  }

  // Gomb folyamatosan nyomva
  if (currentButtonState == LOW && pressStartTime != 0) {
  if (!longPressExecuted && (millis() - pressStartTime >= 1000)) {
    longPressExecuted = true;
    lastMenuActivity = millis();

    // === ÚJ: Ha nincs WiFi kapcsolat → AP infó megjelenítése ===
    if (WiFi.status() != WL_CONNECTED) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tf);
      u8g2.drawUTF8(0, 12, "WiFi Konfiguráció");
      u8g2.drawUTF8(0, 28, "SSID: P1-METER");
      u8g2.drawUTF8(0, 42, "PW: 12345678");
      u8g2.drawUTF8(0, 56, "IP: 192.168.4.1");
      u8g2.sendBuffer();

      // 10 másodpercig mutatja
      while (digitalRead(MODE_BTN_PIN) == LOW) { yield(); delay(10); } // elengedés megvárása
      delay(10000);

      updateOledDisplay();   // vissza az OFF-LINE MÓD képernyőre
      currentButtonState = HIGH;
      return;                // ne menjen tovább a normál menübe
    }

    // === Van WiFi → normál menü logika (ahogy eddig) ===
    if (!inMenuMode) {
      // Belépés a menübe
      inMenuMode = true;
      selectedMenuItem = 1;
      drawMyMenu();
    } else {
        // Funkció indítása
        u8g2.clearBuffer();

        if (selectedMenuItem == 1) {
          // IP cím + Chip ID megjelenítése
          u8g2.drawUTF8(0, 10, "IP cím:");
          u8g2.drawUTF8(0, 25, WiFi.localIP().toString().c_str());
          u8g2.drawUTF8(0, 40, "Chip ID:");
          u8g2.drawUTF8(0, 55, chipId.c_str());
          u8g2.sendBuffer();

          while (digitalRead(MODE_BTN_PIN) == LOW) { yield(); delay(10); }
          delay(10000);
        }
        else if (selectedMenuItem == 2) {
          // Firmware keresés
          
            String ssid = "";
  String password = "";

  if (!SPIFFS.begin()) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tf);
    u8g2.drawUTF8(0, 30, "SPIFFS hiba");
    u8g2.sendBuffer();
    delay(2000);
    return;
  }

  File f = SPIFFS.open(wifiConfigPath, "r");  // "/wifi.txt"
  if (!f) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tf);
    u8g2.drawUTF8(0, 30, "Nincs mentett WiFi");
    u8g2.sendBuffer();
    delay(2000);
    return;
  }

  // Feltételezzük, hogy a fájl formátuma a loadConfig-nak megfelelően
  // általában: SSID, jelszó, esetleg IP, gateway, subnet...
  if (f.available()) ssid = f.readStringUntil('\n');
  if (f.available()) password = f.readStringUntil('\n');

  ssid.trim();
  password.trim();
  f.close();

  // OLED megjelenítés
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tf);

  u8g2.drawUTF8(0, 10, "SSID:");
  u8g2.drawUTF8(0, 25, ssid.c_str());

  u8g2.drawUTF8(0, 44, "Jelszó:");
  u8g2.drawUTF8(0, 55, password.c_str());

  u8g2.sendBuffer();




  // 10 másodpercig mutatja, utána visszatér
  delay(10000);
        }
        else if (selectedMenuItem == 3) {
          // WiFi konfiguráció törlése
          u8g2.drawUTF8(0, 30, "WiFi törölve!");
          u8g2.sendBuffer();
          SPIFFS.begin();
          SPIFFS.remove(wifiConfigPath);
          SPIFFS.end();
          delay(2000);
          ESP.restart();
        }

        inMenuMode = false;
        updateOledDisplay();
        currentButtonState = HIGH;   // Állapot kényszerítése
      }
    }
  }

  // Gomb elengedése
  if (currentButtonState == HIGH && lastButtonState == LOW) {
    unsigned long holdTime = millis() - pressStartTime;
    pressStartTime = 0;

    // Rövid nyomás menüben → léptetés
    if (!longPressExecuted && holdTime < 3000 && inMenuMode) {
      lastMenuActivity = millis();
      selectedMenuItem++;
      if (selectedMenuItem > 3) selectedMenuItem = 1;
      drawMyMenu();
    }
  }

  lastButtonState = currentButtonState;
}

// ================================================================================
//  WEBES FELÜLET
// ================================================================================

/**
 * Nyers adatok megjelenítése (debug)
 */
void handleRaw() {
  String out = "=== P1 WEBES ADATOK HOSSZA ===\n";
  out += String(allObisData.length()) + " karakter\n\n";
  out += "=== GENERÁLT HTML KÁRTYÁK MINTÁJA ===\n";
  out += allObisData;
  server.send(200, "text/plain", out);
}

/**
 * Fő webes oldal (kártyák + kedvencek szűrés)
 */
void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  // HTML fejléc + CSS
  String page = "<!DOCTYPE html><html lang='hu'><head><meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  page += "<meta http-equiv='refresh' content='10'>";
  page += "<title>P1 Smart Meter</title><style>";
  page += "body{font-family:sans-serif;background:#121212;color:#e0e0e0;margin:0;padding:10px}";
  page += ".container{max-width:1100px;margin:0 auto}";
  page += "h1{text-align:center;color:#00e676;font-size:18px}";
  page += ".controls{display:flex;justify-content:center;gap:10px;margin-bottom:15px}";
  page += ".btn{background:#1e1e1e;border:1px solid #444;color:#aaa;padding:8px 16px;border-radius:15px;font-size:13px;cursor:pointer;-webkit-tap-highlight-color:transparent}";
  page += ".btn.act{background:#00e676;color:#121212;font-weight:bold}";
  page += ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:8px}";
  page += ".card{background:#1e1e1e;border-radius:8px;padding:10px;border:1px solid #2c2c2c;position:relative;display:flex;flex-direction:column;justify-content:space-between}";
  page += ".card-title{font-size:10px;color:#90caf9;text-transform:uppercase;margin-bottom:5px;padding-right:20px}";
  page += ".star{position:absolute;top:6px;right:6px;font-size:18px;color:#444;cursor:pointer;padding:4px;user-select:none;-webkit-tap-highlight-color:transparent}";
  page += ".card.is-fav .star{color:#ffd700}";
  page += ".card-value{font-size:15px;font-weight:bold;color:#00e676}";
  page += ".card-code{font-size:8px;color:#ffb74d;font-family:monospace;margin-top:5px}";
  page += ".fav-mode .card:not(.is-fav){display:none !important}";
  page += "@media(max-width:480px){.grid{grid-template-columns:1fr}}";
  page += "</style></head><body><div class='container'>";
  page += "<h1>P1 SmartMeter</h1>";
  page += "<div class='controls'>";
  page += "<button id='bA' class='btn act' onclick='setFilter(0)'>Összes</button>";
  page += "<button id='bF' class='btn' onclick='setFilter(1)'>★ Kedvencek</button>";
  page += "</div><div id='g' class='grid'>";

  server.sendContent(page);

  // Kártyák
  if (allObisData.length() > 0) {
    server.sendContent(allObisData);
  } else {
    server.sendContent("<div class='card'><div class='card-title'>Állapot</div><div class='card-value'>Várakozás adatra...</div></div>");
  }

  // JavaScript (kedvencek + szűrés localStorage-ben)
  String js = "</div></div><script>";
  js += "let favs = JSON.parse(localStorage.getItem('p1_favs') || '[]');";
  js += "let showFavOnly = localStorage.getItem('p1_show_fav') === '1';";
  js += "let grid = document.getElementById('g');";
  js += "function syncUI(){";
  js += "  document.querySelectorAll('.card').forEach(c => {";
  js += "    let code = c.getAttribute('data-code');";
  js += "    if(code && favs.includes(code)){ c.classList.add('is-fav'); } else { c.classList.remove('is-fav'); }";
  js += "  });";
  js += "  if(showFavOnly){ grid.classList.add('fav-mode'); } else { grid.classList.remove('fav-mode'); }";
  js += "  document.getElementById('bA').className = 'btn' + (!showFavOnly ? ' act' : '');";
  js += "  document.getElementById('bF').className = 'btn' + (showFavOnly ? ' act' : '');";
  js += "}";
  js += "window.tG = function(code, el){";
  js += "  let idx = favs.indexOf(code);";
  js += "  if(idx > -1){ favs.splice(idx, 1); } else { favs.push(code); }";
  js += "  localStorage.setItem('p1_favs', JSON.stringify(favs));";
  js += "  syncUI();";
  js += "};";
  js += "window.setFilter = function(val){";
  js += "  showFavOnly = (val === 1);";
  js += "  localStorage.setItem('p1_show_fav', showFavOnly ? '1' : '0');";
  js += "  syncUI();";
  js += "};";
  js += "syncUI();";
  js += "</script></body></html>";

  server.sendContent(js);
  server.sendContent("");
}

/**
 * Csak a kártyák HTML-jét adja vissza
 */
void handleData() {
  server.send(200, "text/plain", allObisData);
}

// ================================================================================
//  OTA (ArduinoOTA)
// ================================================================================

void setupOTA() {
  String otaHost = "P1-Smartmeter_" + chipId;
  MDNS.begin(otaHost.c_str());
  ArduinoOTA.setHostname(otaHost.c_str());
  ArduinoOTA.setPassword(savedOtaPass.c_str());

  ArduinoOTA.onStart([]() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tf);
    u8g2.drawUTF8(0, 20, "OTA Frissités...");
    u8g2.sendBuffer();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tf);
    u8g2.drawUTF8(0, 10, "OTA Frissités...");
    String pct = String(progress / (total / 100)) + "%";
    u8g2.drawUTF8(0, 40, pct.c_str());
    u8g2.sendBuffer();
  });

  ArduinoOTA.onEnd([]() {
    u8g2.clearBuffer();
    u8g2.drawUTF8(0, 30, "Frissités Kész!");
    u8g2.sendBuffer();
  });

  ArduinoOTA.onError([](ota_error_t error) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 30, "OTA Hiba!");
    u8g2.sendBuffer();
  });

  ArduinoOTA.begin();
}



  

// ================================================================================
//  SETUP
// ================================================================================
void setup() {
  // NTP beállítás (közép-európai idő + nyári időszámítás)
  configTime("CET-1CEST,M3.5.0,M10.5.0/3", "hu.pool.ntp.org", "pool.ntp.org");

  // Gomb bemenet
  pinMode(MODE_BTN_PIN, INPUT_PULLUP);

  // MQTT buffer méret növelése
  mqttClient.setBufferSize(2048);

  // Egyedi chip azonosító
  chipId = String(ESP.getChipId(), HEX);
  chipId.toLowerCase();
  mqttBaseTopic = String(MQTT_BASE_TOPIC) + "/" + chipId;

  // Soros port (P1) – 115200 baud, RX invertálva
  Serial.begin(115200);
  Serial.setRxBufferSize(1024);
  USC0(UART0) = USC0(UART0) | BIT(UCRXI);

  // OLED inicializálás + logo
  u8g2.begin();
  showLogo(5000);
  u8g2.setFont(u8g2_font_ncenB08_tf);

  handleResetButton();   // Ha a gomb nyomva van indításkor

  // WiFi konfiguráció betöltése
  String ssid, password, ipstr, gatewaystr, subnetstr;
  String mqttServer, mqttPort, mqttUser, mqttPass, otaUser, otaPass;

  if (!loadConfig(ssid, password, ipstr, gatewaystr, subnetstr,
                  mqttServer, mqttPort, mqttUser, mqttPass,otaUser,otaPass)) {
    startAPMode();       // Nincs mentett konfig → AP mód
    isAPMode = true;
    return;
  }

  // Mentett MQTT adatok
  savedMqttServer = (mqttServer.length() > 0) ? mqttServer : MQTT_SERVER;
  savedMqttPort   = (mqttPort.length() > 0)   ? mqttPort.toInt() : MQTT_PORT;
  savedMqttUser   = (mqttUser.length() > 0)   ? mqttUser : MQTT_USER;
  savedMqttPass   = (mqttPass.length() > 0)   ? mqttPass : MQTT_PASSWORD;
  savedOtaUser   = (otaUser.length() > 0)   ? otaUser : OTA_USER;
  savedOtaPass   = (otaPass.length() > 0)   ? otaPass : OTA_PASSWORD;
  
  // ... WiFi csatlakozás ...
 
 
  // WiFi csatlakozás
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tf);
  u8g2.drawUTF8(0, 30, "WiFi csatlakozás...");
  u8g2.sendBuffer();
SPIFFS.begin();
  bool fixip = ipstr.length() > 0 && gatewaystr.length() > 0 && subnetstr.length() > 0;
  if (fixip) {
    IPAddress ip, gateway, subnet;
    IPAddress dns1(8, 8, 8, 8);
    IPAddress dns2(8, 8, 4, 4);
    if (ip.fromString(ipstr) && gateway.fromString(gatewaystr) && subnet.fromString(subnetstr)) {
      WiFi.config(ip, gateway, subnet, dns1, dns2);
    }
  }

  String hostName = "P1-SmartMeter_" + chipId;
  WiFi.hostname(hostName);
  WiFi.begin(ssid.c_str(), password.c_str());

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    setupOTA();
    mqttClient.setServer(savedMqttServer.c_str(), savedMqttPort);
  } else {
    startAPMode();
  }

  // Webszerver végpontok
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/raw", handleRaw);
  setupWebOTA();
  server.begin();

  updateOledDisplay();
}

// ================================================================================
//  LOOP
// ================================================================================
void loop() {
  // Captive Portal DNS (kizárólag AP módban)
  if (isAPMode) {
    dnsServer.processNextRequest();
  } 

  // Webszerver kezelése
  server.handleClient();

  // Hálózati szolgáltatások (csak csatlakozott WiFi esetén)
  if (WiFi.status() == WL_CONNECTED) {
    MDNS.update();
    ArduinoOTA.handle();
    handleMQTTConnection();
  }

  // Gomb kezelése
  handleResetButton();

  // P1 port olvasása (nem-blokkoló)
  handleP1Port();

  // OLED frissítés 10 másodpercenként (menüben nem)
  if (millis() - lastP1ReadTime > P1_READ_INTERVAL_MS) {
    lastP1ReadTime = millis();
    if (!inMenuMode) {
      updateOledDisplay();
    }
  }
}