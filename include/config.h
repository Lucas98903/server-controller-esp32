//
// Created by lucas on 11/03/2026.
//

#ifndef SERVER_CONTROLLER_ESP32_CONFIG_H
#define SERVER_CONTROLLER_ESP32_CONFIG_H

namespace cfg
{
    constexpr bool DEBUG_SERIAL = true;
    constexpr bool DEBUG_VERBOSE = true;

    constexpr const char *HOSTNAME = "ESP32_Server";

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

    // Tempo para ligar o servidor [ms]
    constexpr unsigned long POWER_ON_AND_RESET = 700;

    // Tempo para forcar desligamento [ms];
    constexpr unsigned long FORCE_POWER_OFF = 15000;

    constexpr float SERIES_RESISTOR = 10000.0f;
    constexpr float NOMINAL_RESISTANCE = 10000.0f;
    constexpr float NOMINAL_TEMPERATURE = 25.0f;
    constexpr float BETA_COEFFICIENT = 3950.0f;
    constexpr float ADC_MAX = 4095.0f;
    constexpr float VCC = 3.3f;
}

#endif // SERVER_CONTROLLER_ESP32_CONFIG_H
