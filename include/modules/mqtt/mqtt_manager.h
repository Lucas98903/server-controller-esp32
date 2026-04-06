//
// Created by lucas on 06/04/2026.
//

#ifndef SERVER_CONTROLLER_ESP32_MQTT_MANAGER_H

#pragma once
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

extern WiFiClient wifiClient;
extern PubSubClient mqttClient;

namespace mqtt_manager
{
    void setupMqtt(MQTT_CALLBACK_SIGNATURE);
    void connectMqtt();
    bool publishJson(const char* topic, JsonDocument& doc);
}

#define SERVER_CONTROLLER_ESP32_MQTT_MANAGER_H

#endif //SERVER_CONTROLLER_ESP32_MQTT_MANAGER_H
