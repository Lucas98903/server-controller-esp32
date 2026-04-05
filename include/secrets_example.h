//
// Created by lucas on 12/03/2026.
//

#ifndef SERVER_CONTROLLER_ESP32_SECRETS_H
#define SERVER_CONTROLLER_ESP32_SECRETS_H

namespace secrets
{
    constexpr const char *WIFI_SSID = "Nome do WiFi";
    constexpr const char *WIFI_PASSWORD = "Senha Do Wifi";

    constexpr const char *API_KEY = " Sua API Key do Firebase ";
    constexpr const char *PROJECT_ID = "Seu Project ID do Firebase ";
    constexpr const char *DOCUMENT_PATH =
        "Caminho do documento para status (ex: server-status/host-main)";

    constexpr const char RTDB_HOST[] =
        "Seu RTDB Host (ex: server-therabithia-default-rtdb.firebaseio.com)";
    constexpr const char RTDB_NODE_PATH[] =
        "Caminho do nó para dados do dispositivo (ex: /server-terabithia)";

    constexpr const char AUTH_EMAIL[] = "Seu email de autenticação do Firebase ";
    constexpr const char AUTH_PASSWORD[] = "Sua senha de autenticação do Firebase ";

    constexpr const char *OTA_PASSWORD = "Sua senha para upload OTA";
} // namespace secrets

#endif // SERVER_CONTROLLER_ESP32_SECRETS_H
