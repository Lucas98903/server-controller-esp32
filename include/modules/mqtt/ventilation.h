//
// Created by lucas on 06/04/2026.
//

#ifndef SERVER_CONTROLLER_ESP32_VENTILATION_H
#define SERVER_CONTROLLER_ESP32_VENTILATION_H

#pragma once
#include <ArduinoJson.h>

extern bool ventilationOn;

bool setVentilationState(bool turnOn);
void handleVentilationCommand(JsonDocument& doc);
void sendVentilationAck(int requestId, const char* action, bool success, const char* message);


#endif //SERVER_CONTROLLER_ESP32_VENTILATION_H
