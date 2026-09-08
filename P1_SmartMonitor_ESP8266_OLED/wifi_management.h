#ifndef WIFI_MANAGEMENT_H
#define WIFI_MANAGEMENT_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>

bool loadConfig(String& ssid, String& password, String& ip, String& gateway, String& subnet,
                String& mqttServer, String& mqttPort, String& mqttUser, String& mqttPass,
                String& otaUser, String& otaPass);

void saveConfig(const String& ssid, const String& password, const String& ip, const String& gateway, const String& subnet,
                const String& mqttServer, const String& mqttPort, const String& mqttUser, const String& mqttPass,
                const String& otaUser, const String& otaPass);

void startAPMode();

#endif