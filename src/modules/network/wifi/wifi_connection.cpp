//
// Created by lucas on 11/03/2026.
//

#include "modules/network/wifi/wifi_connection.h"
#include "config.h"

namespace wifi_connection
{
    bool connect(const WiFiCredentials &credentials,
                 unsigned long timeout_ms,
                 bool force_disconnect)
    {
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(cfg::HOSTNAME);

        if (force_disconnect)
        {
            WiFi.disconnect(true, true);
            delay(300);
        }

        WiFi.begin(credentials.ssid, credentials.password);

        const unsigned long start_time = millis();

        while (WiFi.status() != WL_CONNECTED &&
               (millis() - start_time) < timeout_ms)
        {
            delay(500);
        }

        return WiFi.status() == WL_CONNECTED;
    }

    bool isConnected()
    {
        return WiFi.status() == WL_CONNECTED;
    }

    void disconnect(bool wifi_off)
    {
        WiFi.disconnect(true, true);

        if (wifi_off)
            WiFi.mode(WIFI_OFF);
    }

    IPAddress localIP()
    {
        return WiFi.localIP();
    }
}
