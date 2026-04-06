//
// Created by lucas on 12/03/2026.
//

#include <Arduino.h>

#include "modules/network/wifi/wifi_connection.h"
#include "config.h"
#include "debug.h"
#include "secrets.h"

using wifi_connection::WiFiCredentials;

namespace
{
    constexpr WiFiCredentials WIFI = {
        secrets::WIFI_SSID,
        secrets::WIFI_PASSWORD};

    unsigned long last_wifi_reconnect_attempt_ms = 0;
    constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;

    static bool tryConnectMultipleTimes(const WiFiCredentials &credentials,
                                        int max_attempts,
                                        unsigned long timeout_ms)
    {
        for (int attempt = 1; attempt <= max_attempts; ++attempt)
        {
            DEBUG_VERBOSE_PRINT("[WiFi] Tentativa ");
            DEBUG_VERBOSE_PRINT(attempt);
            DEBUG_VERBOSE_PRINT(" de ");
            DEBUG_VERBOSE_PRINT(max_attempts);
            DEBUG_VERBOSE_PRINT(" na rede: ");
            DEBUG_VERBOSE_PRINTLN(credentials.ssid);

            if (wifi_connection::connect(credentials, timeout_ms, true))
            {
                DEBUG_PRINTLN("[WiFi] Conectado com sucesso.");
                return true;
            }
        }

        return false;
    }
}

namespace wifi_recovery
{
    bool ensureConnected()
    {
        if (wifi_connection::isConnected())
        {
            return true;
        }

        const unsigned long now = millis();
        if (now - last_wifi_reconnect_attempt_ms < WIFI_RECONNECT_INTERVAL_MS)
        {
            return false;
        }

        last_wifi_reconnect_attempt_ms = now;

        DEBUG_PRINTLN("[WiFi] Conexao perdida. Tentando reconectar...");

        if (tryConnectMultipleTimes(
                WIFI,
                cfg::PRIMARY_RETRY_LIMIT,
                cfg::WIFI_TIMEOUT_MS))
        {
            DEBUG_PRINTLN("[WiFi] Reconexao realizada com sucesso.");
            return true;
        }

        DEBUG_PRINTLN("[WiFi] Falha na reconexao.");

        return false;
    }
}