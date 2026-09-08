#include "wifi_management.h"
#include <U8g2lib.h>
#include "secrets.h"  
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern ESP8266WebServer server;
extern const char* wifiConfigPath;

#define MQTT_SERVER         SECRET_MQTT_SERVER
#define MQTT_PORT           SECRET_MQTT_PORT
#define MQTT_USER           SECRET_MQTT_USER
#define MQTT_PASSWORD       SECRET_MQTT_PASSWORD
#define OTA_USER            SECRET_OTA_USER
#define OTA_PASSWORD        SECRET_OTA_PASSWORD

DNSServer dnsServer;
const byte DNS_PORT = 53;

void oled_write(byte sor, const String& szoveg) {
  const byte elsoSorY = 10;
  const byte sortav = 14;

  u8g2.drawUTF8(0, elsoSorY + (sor - 1) * sortav, szoveg.c_str());
}

void oled_clear() {
  u8g2.clearBuffer();
}

void oled_send() {
  u8g2.sendBuffer();
}

bool loadConfig(String& ssid, String& password, String& ip, String& gateway, String& subnet,
                String& mqttServer, String& mqttPort, String& mqttUser, String& mqttPass,
                String& otaUser, String& otaPass) {
    if (!SPIFFS.begin()) return false;
    if (!SPIFFS.exists(wifiConfigPath)) { SPIFFS.end(); return false; }
    
    File f = SPIFFS.open(wifiConfigPath, "r");
    if (!f) { SPIFFS.end(); return false; }
    
    ssid = f.readStringUntil('\n'); ssid.trim();
    password = f.readStringUntil('\n'); password.trim();
    ip = f.readStringUntil('\n'); ip.trim();
    gateway = f.readStringUntil('\n'); gateway.trim();
    subnet = f.readStringUntil('\n'); subnet.trim();
    
    mqttServer = f.readStringUntil('\n'); mqttServer.trim();
    mqttPort = f.readStringUntil('\n'); mqttPort.trim();
    mqttUser = f.readStringUntil('\n'); mqttUser.trim();
    mqttPass = f.readStringUntil('\n'); mqttPass.trim();

    otaUser = f.readStringUntil('\n'); otaUser.trim();
    otaPass = f.readStringUntil('\n'); otaPass.trim();
    
    f.close();
    SPIFFS.end();
    return ssid.length() > 0;
}

void saveConfig(const String& ssid, const String& password, const String& ip, const String& gateway, const String& subnet,
                const String& mqttServer, const String& mqttPort, const String& mqttUser, const String& mqttPass,
                const String& otaUser, const String& otaPass) {
    SPIFFS.begin();
    File f = SPIFFS.open(wifiConfigPath, "w");
    if (!f) return;
    
    f.println(ssid);
    f.println(password);
    f.println(ip);
    f.println(gateway);
    f.println(subnet);
    
    f.println(mqttServer);
    f.println(mqttPort);
    f.println(mqttUser);
    f.println(mqttPass);

    f.println(otaUser);
    f.println(otaPass);
    
    f.close();
    SPIFFS.end();
}

void startAPMode() {
    WiFi.mode(WIFI_AP_STA);
    
    IPAddress local_ip(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP("P1-METER", "12345678");

    dnsServer.start(DNS_PORT, "*", local_ip);

    server.on("/", HTTP_GET, []() {
        String ssid, password, ip, gateway, subnet;
        String mqttServer, mqttPort, mqttUser, mqttPass;
        String otaUser, otaPass;
        
        loadConfig(ssid, password, ip, gateway, subnet, mqttServer, mqttPort, mqttUser, mqttPass, otaUser, otaPass);
        
        // Alapértelmezett értékek, ha még üresek lennének
        if (mqttServer.length() == 0) mqttServer = MQTT_SERVER;
        if (mqttPort.length() == 0)   mqttPort   = MQTT_PORT;
        if (mqttUser.length() == 0)   mqttUser   = MQTT_USER;
        if (mqttPass.length() == 0)   mqttPass   = MQTT_PASSWORD;
        if (otaUser.length() == 0)    otaUser    = OTA_USER;
        if (otaPass.length() == 0)    otaPass    = OTA_PASSWORD;

        int n = WiFi.scanNetworks();
        String ssidOptions = "";
        bool foundSavedSsid = false;

        if (n > 0) {
            for (int i = 0; i < n; ++i) {
                String networkName = WiFi.SSID(i);
                int rssi = WiFi.RSSI(i);
                bool isSelected = (networkName == ssid);
                if (isSelected) foundSavedSsid = true;

                String selected = isSelected ? " selected" : "";
                ssidOptions += "<option value='" + networkName + "'" + selected + ">" 
                            + networkName + " (" + String(rssi) + " dBm)</option>";
            }
        }

        // Opció a kézi beírásra
        String manualSelected = (!foundSavedSsid && ssid.length() > 0) ? " selected" : "";
        ssidOptions += "<option value='__MANUAL__' " + manualSelected + ">-- Kézi megadás / Rejtett SSID --</option>";

        String html = R"====(<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name="viewport" content="width=device-width, initial-scale=1"><title>Eszköz Konfiguráció</title><style>body{font-family:Arial,sans-serif;background-color:#f7f7f7;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;padding:20px 0;box-sizing:border-box}.container{background-color:#4CAF50;color:white;padding:25px 30px;border-radius:12px;box-shadow:0 0 15px rgba(0,0,0,0.2);text-align:center;width:90%;max-width:420px}h1{margin-bottom:15px;font-size:20px}h2{font-size:16px;margin:15px 0 5px 0;text-align:left;border-bottom:1px solid rgba(255,255,255,0.4);padding-bottom:3px}form{display:flex;flex-direction:column;gap:10px}label{font-size:13px;text-align:left}input[type="text"],input[type="password"],input[type="number"],select{font-size:14px;padding:8px;width:100%;box-sizing:border-box;border-radius:5px;border:none}input[type="submit"]{font-size:16px;padding:12px;margin-top:15px;cursor:pointer;background-color:white;color:#4CAF50;border:none;border-radius:5px;font-weight:bold}.form-group{display:flex;flex-direction:column;align-items:flex-start}#manual_group{display:none;margin-top:5px;width:100%;}</style><script>function toggleManualSsid(){var sel=document.getElementById('ssid_select');var man=document.getElementById('manual_group');if(sel.value==='__MANUAL__'){man.style.display='block';}else{man.style.display='none';}}window.onload=function(){toggleManualSsid();};</script></head><body><div class="container"><h1>P1 Mérő Beállítások</h1><form method='POST' action='/save'><h2>WiFi Beállítások</h2><div class="form-group"><label>WiFi Hálózat:</label><select id='ssid_select' name='ssid_select' onchange='toggleManualSsid()'>{{ssid_options}}</select><div id='manual_group'><label style='margin-top:5px;'>Kézi SSID / Hálózat neve:</label><input name='ssid_manual' type='text' value='{{ssid_manual_value}}' placeholder='Írd be a hálózat nevét'></div></div><div class="form-group"><label>WiFi Jelszó:</label><input name='password' type='password' value='{{password}}'></div><div class="form-group"><label>Fix IP (opcionális):</label><input name='ip' type="text" value='{{ip}}'></div><div class="form-group"><label>Gateway (opcionális):</label><input name='gateway' type="text" value='{{gateway}}'></div><div class="form-group"><label>Subnet (opcionális):</label><input name='subnet' type="text" value='{{subnet}}'></div><h2>MQTT Beállítások</h2><div class="form-group"><label>MQTT Szerver IP / Host:</label><input name='mqtt_server' type="text" value='{{mqtt_server}}'></div><div class="form-group"><label>MQTT Port:</label><input name='mqtt_port' type="number" value='{{mqtt_port}}'></div><div class="form-group"><label>MQTT Felhasználónév:</label><input name='mqtt_user' type="text" value='{{mqtt_user}}'></div><div class="form-group"><label>MQTT Jelszó:</label><input name='mqtt_pass' type="password" value='{{mqtt_pass}}'></div><h2>OTA Beállítások</h2><div class="form-group"><label>OTA Felhasználónév:</label><input name='ota_user' type="text" value='{{ota_user}}'></div><div class="form-group"><label>OTA Jelszó:</label><input name='ota_pass' type="password" value='{{ota_pass}}'></div><input type='submit' value='Mentés és újraindítás'></form></div></body></html>)====";

        html.replace("{{ssid_options}}", ssidOptions);
        html.replace("{{ssid_manual_value}}", ssid);
        html.replace("{{password}}", password); 
        html.replace("{{ip}}", ip); 
        html.replace("{{gateway}}", gateway); 
        html.replace("{{subnet}}", subnet);
        
        html.replace("{{mqtt_server}}", mqttServer);
        html.replace("{{mqtt_port}}", mqttPort);
        html.replace("{{mqtt_user}}", mqttUser);
        html.replace("{{mqtt_pass}}", mqttPass);

        html.replace("{{ota_user}}", otaUser);
        html.replace("{{ota_pass}}", otaPass);
        
        server.send(200, "text/html; charset=UTF-8", html);
    });

    server.on("/save", HTTP_POST, []() {
        String selectedSsid = server.arg("ssid_select");
        String finalSsid = (selectedSsid == "__MANUAL__") ? server.arg("ssid_manual") : selectedSsid;

        saveConfig(
            finalSsid, server.arg("password"), server.arg("ip"), server.arg("gateway"), server.arg("subnet"),
            server.arg("mqtt_server"), server.arg("mqtt_port"), server.arg("mqtt_user"), server.arg("mqtt_pass"),
            server.arg("ota_user"), server.arg("ota_pass")
        );
        server.send(200, "text/html; charset=UTF-8", "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>Beállítások elmentve. Az eszköz újraindul...</h1></body></html>");
        delay(1000);
        ESP.restart();
    });

    server.onNotFound([]() {
        server.sendHeader("Location", String("http://192.168.4.1/"), true);
        server.send(302, "text/plain", "");
    });

    server.begin();

    oled_clear();
    oled_write(1, "WiFi Konfiguráció");
    oled_write(2, "SSID:P1-METER");
    oled_write(3, "PW:12345678");
    oled_write(4, "IP: 192.168.4.1");
    oled_send();
    delay(5000);
}