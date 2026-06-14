//
// Created by lucas on 06/04/2026.
//

#ifndef SERVER_CONTROLLER_ESP32_VENTILATION_H
#define SERVER_CONTROLLER_ESP32_VENTILATION_H

#pragma once
#include <ArduinoJson.h>

extern bool ventilationOn;


namespace mqtt_ventilation
{
    void handleVentilationCommand(JsonDocument& doc);
}


#endif //SERVER_CONTROLLER_ESP32_VENTILATION_H
