//
// Created by lucas on 11/03/2026.
//

#include <Arduino.h>
#include <ArduinoJson.h>

#include "modules/control/connectivity_monitor.h"
#include "modules/network/network_task.h"
#include "modules/control/relay_action.h"
#include "modules/firebase/rtdb_manager.h"
#include "modules/control/server_controller.h"

#include "modules/network/wifi/wifi_manager.h"

#include "config.h"
#include "debug.h"


void setup()
{
    DEBUG_BEGIN(115200);
    DEBUG_PRINTLN("[SYSTEM] Inicializando...");

    pinMode(cfg::STATUS_SUPPLY_PIN, INPUT);
    pinMode(cfg::STATUS_MOBO_PIN, INPUT);

    pinMode(cfg::VENTILATION_PIN, OUTPUT);
    pinMode(cfg::STARTER_PIN, OUTPUT);
    pinMode(cfg::LED_PIN, OUTPUT);
    pinMode(cfg::RESET_PIN, OUTPUT);
    pinMode(cfg::COOLER_127_PIN, OUTPUT);

    wifi_recovery::ensureConnected();

    relay_action::setRelayState(cfg::STARTER_PIN, false);
    relay_action::setRelayState(cfg::VENTILATION_PIN, false);
    relay_action::setRelayState(cfg::RESET_PIN, false);
    relay_action::setRelayState(cfg::COOLER_127_PIN, false);

    rtdb_manager::begin();
    network_task::begin();
}

void loop()
{
    if (!connectivity_monitor::update())
        return;

    server_controller::update();
}
