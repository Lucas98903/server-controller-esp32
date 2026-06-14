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

namespace
{
    constexpr unsigned long ATTEMPT_POWER_OFF_WAIT_MS = 10000;

    unsigned long s_lastAttemptPowerOffMs = 0;
    unsigned long s_serverOnSinceMs = 0;
    uint8_t s_failedAttemptPorwerOff = 0;
    bool s_lastServerOnState = false;
    bool s_awaitingShutdown = false;

    rtdb_manager::DeviceData s_deviceData;

    // -----------------------------------------------------------------

    void updateHardwareStatus()
    {
        const bool supplyIsOn = server_status::isSupplyOn();
        const bool moboIsOn = server_status::isMoboOn();
        const bool serverIsOn = server_status::isServerOn();

        // Se o servidor acabou de ligar, registra o tempo de início para usar na lógica de boot grace.
        if (serverIsOn && !s_lastServerOnState)
            s_serverOnSinceMs = millis();
        s_lastServerOnState = serverIsOn;

        // TODO: manter apenas uma unica fonte para informar que o servidor esta ligado
        rtdb_manager::enqueueSupplyStatusUpdate(supplyIsOn);
        rtdb_manager::enqueueMoboStatusUpdate(moboIsOn);

        digitalWrite(cfg::LED_PIN, serverIsOn ? HIGH : LOW);
    }

    void tryPowerOff()
    {
        // Se o servidor já estiver desligado, reseta os estados de tentativa de desligamento e retorna.
        if (!server_status::isServerOn())
        {
            s_lastAttemptPowerOffMs = 0;
            s_failedAttemptPorwerOff = 0;
            return;
        }

        // Se já estamos aguardando o desligamento, não faz nada.
        if (s_lastAttemptPowerOffMs == 0)
        {
            s_lastAttemptPowerOffMs = millis();

            // Faz tentativa de desligamento. Se já falhou várias vezes, faz um desligamento forçado.
            if (s_failedAttemptPorwerOff < cfg::MQTT_FAILED_BEFORE_POWER_ACTION)
                relay_action::pulsePowerButton();
            else
                relay_action::forcePowerButton();

            s_failedAttemptPorwerOff++;
        }

        if (millis() - s_lastAttemptPowerOffMs > ATTEMPT_POWER_OFF_WAIT_MS)
            s_lastAttemptPowerOffMs = 0;
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

            // Faz tentativas de reconexão apenas a cada MQTT_RECONNECT_WAIT_MS para evitar flood de mensagens e ações.
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
            tryPowerOff();
        }
        else
        {
            s_lastAttemptPowerOffMs = 0;
            s_serverOnSinceMs = 0;
            s_failedAttemptPorwerOff = 0;
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
            rtdb_manager::enqueuePowerOnCountUpdate(s_deviceData.powerOnCount + 1);
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
            relay_action::PulseResetButton();
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
}

namespace server_controller
{
    void update()
    {
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
