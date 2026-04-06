//
// Created by lucas on 06/04/2026.
//

#ifndef SERVER_CONTROLLER_ESP32_IS_SERVER_ON_H
#define SERVER_CONTROLLER_ESP32_IS_SERVER_ON_H

#include <Arduino.h>
#include "config.h"

namespace server_status
{
    bool isMoboOn();
    bool isSupplyOn();
    bool isServerOn();
}

#endif //SERVER_CONTROLLER_ESP32_IS_SERVER_ON_H
