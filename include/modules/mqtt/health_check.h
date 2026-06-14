//
// Created by lucas on 06/04/2026.
//

#ifndef SERVER_CONTROLLER_ESP32_HEALTH_CHECK_H
#define SERVER_CONTROLLER_ESP32_HEALTH_CHECK_H

#pragma once
#include <ArduinoJson.h>

extern bool serverAlive;

namespace mqtt_health
{
    void resetHealthCheckState();
    void processServerHealthCheck();
    void sendHealthCheckRequest();
    void handleHealthResponse(JsonDocument& doc);
}


#endif //SERVER_CONTROLLER_ESP32_HEALTH_CHECK_H
