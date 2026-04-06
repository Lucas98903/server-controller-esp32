//
// Created by lucas on 11/03/2026.
//

#include "modules/control/server_controller.h"

#include <Arduino.h>

#include "modules/control/relay_action.h"
#include "modules/firebase/rtdb_manager.h"

#include "config.h"
#include "debug.h"
#include "modules/utils/is_server_on.h"

namespace
{
    rtdb_manager::DeviceData deviceData;

    bool statusInitialized = false;
    bool lastServerStatusSent = false;
    unsigned long serverStateDebounceStartMs = 0;
    bool lastMoboStatusSent = false;
    bool moboStatusInitialized = false;
    unsigned long moboStateDebounceStartMs = 0;
    bool isServerOn = false;
} // namespace

namespace server_controller
{
    void update()
    {
        const bool supplyIsOn = server_status::isSupplyOn();
        const bool moboIsOn = server_status::isMoboOn();

        rtdb_manager::enqueueSupplyStatusUpdate(supplyIsOn);
        rtdb_manager::enqueueMoboStatusUpdate(moboIsOn);

        digitalWrite(cfg::LED_PIN, server_status::isServerOn() ? HIGH : LOW);

        // -------------------------------------------------------------------------
        // DESPACHO DE COMANDOS DO RTDB
        // -------------------------------------------------------------------------
        rtdb_manager::DeviceData latest;
        if (rtdb_manager::consumeLatest(latest))
        {
            deviceData = latest;

            if (deviceData.turnServerOn)
            {
                DEBUG_PRINTLN("[ACTION] Comando recebido: ligar servidor.");
                relay_action::pulsePowerButton();
                rtdb_manager::enqueueClearTurnServerOn();
                rtdb_manager::enqueuePowerOnCountUpdate(deviceData.powerOnCount + 1);
            }

            if (deviceData.forcePowerOff)
            {
                DEBUG_PRINTLN("[ACTION] Comando recebido: forcar desligamento.");
                relay_action::forcePowerButton();
                rtdb_manager::enqueueClearForcePowerOff();
            }

            if (deviceData.resetServer)
            {
                DEBUG_PRINTLN("[ACTION] Comando recebido: resetar servidor.");
                relay_action::PulseResetButton();
                rtdb_manager::enqueueClearReset();
            }

            if (isServerOn)
            {
                relay_action::setRelayState(cfg::VENTILATION_PIN, deviceData.turnVentilationOn == false);
                relay_action::setRelayState(cfg::COOLER_127_PIN, deviceData.turnVentilation127On);
            }
        }

        // -------------------------------------------------------------------------
        // VENTILACAO: desligar quando servidor esta off
        // -------------------------------------------------------------------------
        if (!isServerOn)
        {
            relay_action::setRelayState(cfg::VENTILATION_PIN, false);
            relay_action::setRelayState(cfg::COOLER_127_PIN, false);
        }

        // -------------------------------------------------------------------------
        // HEARTBEAT
        // -------------------------------------------------------------------------
        if (!deviceData.itsAlive)
        {
            DEBUG_PRINTLN("[ACTION] itsAlive esta false. Enfileirando atualizacao...");
            rtdb_manager::enqueueUpdateItsAlive(true);
            deviceData.itsAlive = true;
        }
    }
} // namespace server_controller
