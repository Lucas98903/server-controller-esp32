//
// Created by lucas on 11/03/2026.
//

#ifndef SERVER_CONTROLLER_ESP32_CONFIG_H
#define SERVER_CONTROLLER_ESP32_CONFIG_H

namespace cfg
{
    constexpr bool DEBUG_SERIAL = true;
    constexpr bool DEBUG_VERBOSE = true;

    constexpr const char* HOSTNAME = "ESP32_Server";

    // Quantidade de tentativas para conectar com a internet
    constexpr int PRIMARY_RETRY_LIMIT = 3;
    constexpr unsigned long WIFI_TIMEOUT_MS = 15000;

    constexpr unsigned long OPERATIONAL_FAILURE_CONFIRM_MS = 3000;
    constexpr unsigned long RTDB_COMMUNICATION_TIMEOUT_MS = 30000;

    // Ajuste conforme seu módulo de relé: true = relé ativa com LOW | false = relé ativa com HIGH
    constexpr bool RELAY_ACTIVE_LOW = true;

    // Pinos GPIO SAIDA
    constexpr int VENTILATION_PIN = 26;
    constexpr int STARTER_PIN = 25;
    constexpr int LED_PIN = 2;
    constexpr int RESET_PIN = 32;
    constexpr int COOLER_127_PIN = 33;

    // Pino de ENTRADA
    constexpr int STATUS_SUPPLY_PIN = 4;
    constexpr int STATUS_MOBO_PIN = 23;

    // Tempo minimo que o pino STATUS_SUPPLY_PIN deve ficar estavel para aceitar mudanca [ms]
    constexpr unsigned long SERVER_STATUS_DEBOUNCE_MS = 500;

    // Tempo para ligar e resetar o servidor [ms]
    constexpr unsigned long POWER_ON_AND_RESET = 700;

    // Tempo para forcar desligamento [ms];
    constexpr unsigned long FORCE_POWER_OFF = 15000;

    // =======================================- MQTT -=======================================
    //Configuracoes do broker
    constexpr const char* MQTT_BROKER_HOST = "192.168.89.227";
    constexpr uint16_t MQTT_BROKER_PORT = 1883;
    constexpr const char* MQTT_CLIENT_ID = "esp32-server-controller";

    // Topicos MQTT
    constexpr const char* TOPIC_CMD_VENTILATION = "server-controller/cmd/change-fan-state";
    constexpr const char* TOPIC_ACK_VENTILATION = "server-controller/ack/change-fan-state";
    constexpr const char* TOPIC_HEALTH_REQUEST = "server-controller/health/request";
    constexpr const char* TOPIC_HEALTH_RESPONSE = "server-controller/health/response";

    // Health check
    constexpr unsigned long HEALTH_INTERVAL_MS = 30000;
    constexpr unsigned long HEALTH_TIMEOUT_MS = 4000;
    constexpr int MAX_HEALTH_MISSES = 2;
}

#endif // SERVER_CONTROLLER_ESP32_CONFIG_H
