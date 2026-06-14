//
// Created by lucas on 08/04/2026.
//

#ifndef SERVER_CONTROLLER_ESP32_MQTT_CALLBACK_H
#define SERVER_CONTROLLER_ESP32_MQTT_CALLBACK_H

void mqttCallback(const char* topic, byte* payload, unsigned int length);

#endif //SERVER_CONTROLLER_ESP32_MQTT_CALLBACK_H