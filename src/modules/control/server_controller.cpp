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
#include "modules/network/wifi/wifi_connection.h"

#include "config.h"
#include "debug.h"
#include "modules/utils/is_server_on.h"
#include "modules/utils/mqtt_callback.h"

namespace
{
    constexpr unsigned long ATTEMPT_POWER_OFF_WAIT_MS = 10000;

    unsigned long s_lastAttemptPowerOffMs = 0;
    unsigned long s_serverOnSinceMs = 0;
    uint8_t s_failedAttemptPowerOff = 0;
    bool s_powerOffPending = false;

    rtdb_manager::DeviceData s_deviceData;

    // -----------------------------------------------------------------

    void updateHardwareStatus()
    {
        const bool serverIsOn = server_status::isServerOn();
        rtdb_manager::enqueueIsServerOn(serverIsOn);
    }

    // Não-bloqueante: retorna true enquanto o desligamento está em andamento.
    // Deve ser chamada a cada tick de update() até retornar false.
    bool tickPowerOff()
    {
        if (!server_status::isServerOn())
        {
            s_powerOffPending = false;
            s_lastAttemptPowerOffMs = 0;
            s_failedAttemptPowerOff = 0;
            return false;
        }

        const unsigned long now = millis();

        if (!s_powerOffPending)
        {
            s_lastAttemptPowerOffMs = now;
            s_powerOffPending = true;
            return true;
        }

        if (now - s_lastAttemptPowerOffMs <= ATTEMPT_POWER_OFF_WAIT_MS)
            return true;

        s_lastAttemptPowerOffMs = now;
        s_failedAttemptPowerOff++;

        if (s_failedAttemptPowerOff < cfg::MQTT_FAILED_BEFORE_POWER_ACTION)
            relay_action::pulsePowerButton();
        else
            relay_action::forcePowerButton();

        return true;
    }

    void checkMqttConnect()
    {
        if (server_status::isServerOn())
        {
            if (s_serverOnSinceMs == 0)
                s_serverOnSinceMs = millis();

            // Verifica se o MQTT está conectado.
            if (mqttClient.connected())
                return;

            mqtt_manager::setupMqtt(mqttCallback);
            if (mqtt_manager::connectMqtt())
                return;

            // Verifica se o tempo de incializacao passou.
            const bool inBootGrace = (s_serverOnSinceMs != 0) && (millis() - s_serverOnSinceMs < cfg::MQTT_BOOT_GRACE_MS);
            if (inBootGrace)
                return;

            // Se chegou aqui, o MQTT não está conectado e o tempo de boot grace já passou.
            // Tenta desligar o servidor para evitar ficar com o servidor ligado sem controle.
            // TODO: desenvolver uma maneira de enviar informacao que o MQTT falhou e o ESP32 desligou o servidor.
            tickPowerOff();
        }
        else
        {
            s_powerOffPending = false;
            s_lastAttemptPowerOffMs = 0;
            s_serverOnSinceMs = 0;
            s_failedAttemptPowerOff = 0;
        }
    }

    void dispatchRtdbCommands()
    {
        rtdb_manager::DeviceData latest;
        // Tenta consumir os dados mais recentes do RTDB. Se não tiver dados frescos, retorna.
        if (!rtdb_manager::consumeLatest(latest))
            return;

        s_deviceData = latest;

        if (s_deviceData.turnServerOn)
        {
            DEBUG_PRINTLN("[ACTION] Comando recebido: ligar servidor.");
            relay_action::pulsePowerButton();
            rtdb_manager::enqueueClearTurnServerOn();
        }

        if (s_deviceData.forcePowerOff)
        {
            DEBUG_PRINTLN("[ACTION] Comando recebido: forcar desligamento.");
            relay_action::forcePowerButton();
            rtdb_manager::enqueueClearForcePowerOff();
        }

        if (s_deviceData.resetServer)
        {
            DEBUG_PRINTLN("[ACTION] Comando recebido: resetar servidor.");
            relay_action::pulseResetButton();
            rtdb_manager::enqueueClearReset();
        }
    }

    void updateVentilation()
    {
        if (!server_status::isServerOn())
        {
            DEBUG_PRINTLN("[ACTION] Servidor desligado. Desligando ventilacao.");
            relay_action::setRelayState(cfg::VENTILATION_PIN, false);
            relay_action::setRelayState(cfg::COOLER_127_PIN, false);
        }
    }

    void updateItsAlive()
    {
        if (!s_deviceData.itsAlive)
        {
            DEBUG_PRINTLN("[ACTION] itsAlive esta false. Enfileirando atualizacao...");
            rtdb_manager::enqueueUpdateItsAlive(true);
            s_deviceData.itsAlive = true;
        }
    }

    bool shouldRestoreServerAfterReconnect = false;

    bool isRtdbOperational()
    {
        const unsigned long now = millis();
        const unsigned long lastOk = rtdb_manager::getLastSuccessfulCommunicationMs();

        if (rtdb_manager::isStreamConnected())
            return true;

        if (lastOk != 0 && (now - lastOk) < cfg::RTDB_COMMUNICATION_TIMEOUT_MS)
            return true;

        return false;
    }

    bool isSystemOperational()
    {
        return wifi_connection::isConnected() && isRtdbOperational();
    }

    bool lostConnection()
    {
        const bool suspendSystem = !isSystemOperational() && server_status::isServerOn() && !shouldRestoreServerAfterReconnect;

        if (suspendSystem)
        {
            DEBUG_PRINTLN("[NET] Falha de conectividade util detectada. Aguardando confirmacao...");
            shouldRestoreServerAfterReconnect = true;
        }

        if (shouldRestoreServerAfterReconnect && server_status::isServerOn())
        {
            tickPowerOff();
        }

        const bool restoreSystem = isSystemOperational() && !server_status::isServerOn() && shouldRestoreServerAfterReconnect;

        if (restoreSystem)
        {
            DEBUG_PRINTLN("[NET] Conectividade util restabelecida.");
            shouldRestoreServerAfterReconnect = false;
            relay_action::pulsePowerButton();
        }
        return shouldRestoreServerAfterReconnect;
    }
}

namespace server_controller
{
    void update()
    {
        const bool serverIsOn = server_status::isServerOn();
        digitalWrite(cfg::LED_PIN, serverIsOn ? HIGH : LOW);

        if (lostConnection())
            return;

        updateHardwareStatus();
        checkMqttConnect();
        dispatchRtdbCommands();
        updateVentilation();
        updateItsAlive();

        if (server_status::isServerOn() && mqttClient.connected())
            mqtt_health::processServerHealthCheck();

        mqttClient.loop();
    }
} // namespace server_controller
