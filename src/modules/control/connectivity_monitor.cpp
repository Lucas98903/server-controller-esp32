//
// Created by lucas on 11/03/2026.
//

#include "modules/control/connectivity_monitor.h"

#include <Arduino.h>

#include "modules/network/wifi/wifi_connection.h"
#include "modules/control/relay_action.h"
#include "modules/firebase/rtdb_manager.h"
#include "modules/utils/is_server_on.h"

#include "config.h"
#include "debug.h"

namespace
{
    // Assume que o sistema era operacional antes do boot para que a primeira
    // perda de conectividade (inclusive na inicializacao) dispare o timer.
    bool systemWasOperational = true;
    bool shouldRestoreServerAfterReconnect = false;
    unsigned long operationalFailureSinceMs = 0;

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
} // namespace

namespace connectivity_monitor
{
    bool update()
    {
        const unsigned long now = millis();
        const bool systemOperational = isSystemOperational();

        // -------------------------------------------------------------------------
        // PERDA DE CONECTIVIDADE UTIL (Wi-Fi + RTDB)
        // -------------------------------------------------------------------------
        if (!systemOperational)
        {
            if (systemWasOperational)
            {
                systemWasOperational = false;
                operationalFailureSinceMs = now;
                DEBUG_PRINTLN("[NET] Falha de conectividade util detectada. Aguardando confirmacao...");
            }

            if (operationalFailureSinceMs != 0 &&
                (now - operationalFailureSinceMs >= cfg::OPERATIONAL_FAILURE_CONFIRM_MS))
            {
                if (server_status::isServerOn() && !shouldRestoreServerAfterReconnect)
                {
                    DEBUG_PRINTLN("[NET] Falha confirmada. Desligando servidor...");
                    relay_action::pulsePowerButton();
                    shouldRestoreServerAfterReconnect = true;
                    DEBUG_PRINTLN("[NET] Servidor marcado para religar quando a internet voltar.");
                }

                operationalFailureSinceMs = 0;
            }

            return false;
        }

        // -------------------------------------------------------------------------
        // RECUPERACAO DE CONECTIVIDADE UTIL
        // -------------------------------------------------------------------------
        if (!systemWasOperational)
        {
            systemWasOperational = true;
            DEBUG_PRINTLN("[NET] Conectividade util restabelecida.");

            if (shouldRestoreServerAfterReconnect)
            {
                if (!server_status::isServerOn())
                {
                    DEBUG_PRINTLN("[NET] Religando servidor apos retorno da internet...");
                    relay_action::pulsePowerButton();
                }
                else
                {
                    DEBUG_PRINTLN("[NET] Servidor ja esta ligado.");
                }

                shouldRestoreServerAfterReconnect = false;
            }
        }

        return true;
    }
} // namespace connectivity_monitor
