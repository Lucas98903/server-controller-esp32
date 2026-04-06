//
// Created by lucas on 11/03/2026.
//

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

namespace wifi_connection
{
    struct WiFiCredentials
    {
        const char* ssid;
        const char* password;
    };

    bool connect(const WiFiCredentials& credentials,
                 unsigned long timeout_ms = 15000,
                 bool force_disconnect = true);

    bool isConnected();

    void disconnect(bool wifi_off = false);

    IPAddress localIP();
}

#endif // WIFI_MANAGER_H