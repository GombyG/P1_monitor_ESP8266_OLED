// --- EGYEDI adtok beállítása ---


// Cseréld le a saját adataiddal a your_ kezdetű adatokat
// ha nem az alap portot használod azt is változtasd meg.
// MQTT szerver adatai:
#define SECRET_MQTT_SERVER         "your_server"   // pld. IP cím vagy domain  (192.168.1.1 /  valamidomain.hu)
#define SECRET_MQTT_PORT           1883          // nem kell idézőjelbe tenni pld. alap 1883 vagy egyedi pld. 41883 
#define SECRET_MQTT_USER           "your_user"     // MQTT Felhasználónév 
#define SECRET_MQTT_PASSWORD       "your_pw" // MQTT Jelszó
#define SECRET_MQTT_BASE_TOPIC     "power/smartmeter"   //  Ebbe a topicba lesz az adat a chipID val kiegészítve pld. power/smartmeter/23d22

// WIFI OTA frissítés esetén ezt a jelszót kell megadni a feltöltés elött
#define SECRET_OTA_USER            "admin"
#define SECRET_OTA_PASSWORD        "your_ota_pw"	
