//
// Created by lucas on 11/03/2026.
//

#include "modules/control/server_controller.h"

#include <Arduino.h>

#include "modules/control/relay_action.h"
#include "modules/firebase/rtdb_manager.h"

#include "modules/mqtt/mqtt_manager.h"
#include "modules/mqtt/health_check.h"
#include "modules/mqtt/ventilation.h"

#include "config.h"
#include "debug.h"
#include "modules/utils/is_server_on.h"
#include "modules/utils/mqtt_callback.h"

rtdb_manager::DeviceData deviceData;

namespace
{
    constexpr unsigned long MQTT_RECONNECT_WAIT_MS = 10000;
    constexpr unsigned long MQTT_POST_RESTART_CHECK_MS = 30000;

    unsigned long s_mqttDisconnectedSinceMs = 0;
    unsigned long s_serverOnSinceMs = 0;
    unsigned long s_restartCheckSinceMs = 0;
    uint8_t s_failedReconnectWindows = 0;
    bool s_lastServerOnState = false;
    bool s_waitingRestartCheck = false;
}

namespace server_controller
{
    void update()
    {
        const bool supplyIsOn = server_status::isSupplyOn();
        const bool moboIsOn = server_status::isMoboOn();
        const bool serverIsOn = supplyIsOn || moboIsOn;

        if (serverIsOn && !s_lastServerOnState)
            s_serverOnSinceMs = millis();
        s_lastServerOnState = serverIsOn;

        rtdb_manager::enqueueSupplyStatusUpdate(supplyIsOn);
        rtdb_manager::enqueueMoboStatusUpdate(moboIsOn);

        digitalWrite(cfg::LED_PIN, serverIsOn ? HIGH : LOW);

        // -------------------------------------------------------------------------
        // MQTT: publicar status do servidor
        // -------------------------------------------------------------------------

        if (!mqttClient.connected() && serverIsOn)
        {
            const unsigned long now = millis();
            const bool inBootGrace = (s_serverOnSinceMs != 0) && (now - s_serverOnSinceMs < cfg::MQTT_BOOT_GRACE_MS);

            if (s_waitingRestartCheck)
            {
                if (now - s_restartCheckSinceMs >= MQTT_POST_RESTART_CHECK_MS)
                {
                    if (server_status::isServerOn())
                    {
                        // Se o servidor ainda estiver ligado, forca desligamento
                        relay_action::forcePowerButton();
                    }
                    s_waitingRestartCheck = false;
                    s_mqttDisconnectedSinceMs = now;
                    s_failedReconnectWindows = 0;
                }
            }
            else
            {
                if (s_mqttDisconnectedSinceMs == 0)
                    s_mqttDisconnectedSinceMs = now;

                if (now - s_mqttDisconnectedSinceMs >= MQTT_RECONNECT_WAIT_MS)
                {
                    DEBUG_PRINTLN("[MQTT] Desconectado, tentando reconectar");
                    mqtt_manager::setupMqtt(mqttCallback);

                    if (!mqtt_manager::connectMqtt())
                    {
                        s_mqttDisconnectedSinceMs = now;

                        if (inBootGrace)
                        {
                            DEBUG_PRINTLN("[MQTT] Broker indisponivel no boot grace. Sem acao de energia.");
                        }
                        else
                        {
                            s_failedReconnectWindows++;

                            if (s_failedReconnectWindows >= cfg::MQTT_FAILED_WINDOWS_BEFORE_POWER_ACTION)
                            {
                                DEBUG_PRINTLN("[MQTT] Falhas repetidas. Acionando tentativa de recuperacao por energia.");
                                relay_action::pulsePowerButton();
                                s_waitingRestartCheck = true;
                                s_restartCheckSinceMs = now;
                                s_failedReconnectWindows = 0;
                            }
                        }
                    }
                    else
                    {
                        s_mqttDisconnectedSinceMs = 0;
                        s_failedReconnectWindows = 0;
                    }
                }
            }
        }
        else
        {
            s_mqttDisconnectedSinceMs = 0;
            s_restartCheckSinceMs = 0;
            s_failedReconnectWindows = 0;
            s_waitingRestartCheck = false;
        }

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
        }

        // -------------------------------------------------------------------------
        // VENTILACAO: desligar quando servidor esta off
        // -------------------------------------------------------------------------
        if (!server_status::isServerOn())
        {
            relay_action::setRelayState(cfg::VENTILATION_PIN, false);
            relay_action::setRelayState(cfg::COOLER_127_PIN, false);
        }

        // -------------------------------------------------------------------------
        // itsAlive: garantir que o ESP32 esta funcionando.
        // -------------------------------------------------------------------------
        if (!deviceData.itsAlive)
        {
            DEBUG_PRINTLN("[ACTION] itsAlive esta false. Enfileirando atualizacao...");
            rtdb_manager::enqueueUpdateItsAlive(true);
            deviceData.itsAlive = true;
        }

        mqtt_health::processServerHealthCheck();
        mqttClient.loop();
    }
} // namespace server_controller
