//
// Created by lucas on 08/04/2026.
//

#include <debug.h>
#include "modules/mqtt/health_check.h"
#include "modules/mqtt/ventilation.h"


void mqttCallback(const char* topic, byte* payload, unsigned int length)
{
    DEBUG_PRINT("[MQTT CALLBACK] Topic: ");
    DEBUG_PRINT(topic);
    DEBUG_PRINT(" payload=");

    for (unsigned int i = 0; i < length; i++)
    {
        DEBUG_PRINT(static_cast<char>(payload[i]));
    }
    DEBUG_PRINTLN();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error)
    {
        DEBUG_PRINT("[ERRO] JSON invalido: ");
        DEBUG_PRINTLN(error.c_str());
        return;
    }

    if (strcmp(topic, cfg::TOPIC_CMD_VENTILATION) == 0)
    {
        mqtt_ventilation::handleVentilationCommand(doc);
        return;
    }

    if (strcmp(topic, cfg::TOPIC_HEALTH_RESPONSE) == 0)
    {
        mqtt_health::handleHealthResponse(doc);
        return;
    }

    DEBUG_PRINTLN("[INFO] Topico recebido sem handler.");
}
