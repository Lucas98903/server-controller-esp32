//
// Created by lucas on 01/04/2026.
//

#include "modules/network/ota_manager.h"
#include <ArduinoOTA.h>
#include "debug.h"
#include "secrets.h"
#include "config.h"

namespace ota_manager
{
    void begin()
    {
        ArduinoOTA.setHostname(cfg::HOSTNAME);
        ArduinoOTA.setPassword(secrets::OTA_PASSWORD);

        ArduinoOTA.onStart([]()
                           { DEBUG_PRINTLN("[OTA] Iniciando atualizacao de firmware..."); });

        ArduinoOTA.onEnd([]()
                         { DEBUG_PRINTLN("[OTA] Atualizacao concluida. Reiniciando..."); });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                              {
            if (cfg::DEBUG_SERIAL)
                Serial.printf("[OTA] Progresso: %u%%\r", progress / (total / 100)); });

        ArduinoOTA.onError([](ota_error_t error)
                           {
            if (cfg::DEBUG_SERIAL)
                Serial.printf("[OTA] Erro [%u]\n", error); });

        ArduinoOTA.begin();
        DEBUG_PRINTLN("[OTA] Servico iniciado.");
    }

    void handle()
    {
        ArduinoOTA.handle();
    }
}
