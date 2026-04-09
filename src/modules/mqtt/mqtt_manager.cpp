//
// Created by lucas on 06/04/2026.
//
#include "modules/mqtt/mqtt_manager.h"
#include "modules/mqtt/health_check.h"
#include "PubSubClient.h"
#include "debug.h"
#include "config.h"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);


namespace mqtt_manager
{
    void setupMqtt(MQTT_CALLBACK_SIGNATURE)
    {
        mqttClient.setServer(cfg::MQTT_BROKER_HOST, cfg::MQTT_BROKER_PORT);
        mqttClient.setCallback(callback);
        mqttClient.setBufferSize(512);
        mqttClient.setKeepAlive(30);
    }

    bool connectMqtt()
    {
        int8_t attempt = 0;
        // TODO: Implementar tentativas e retornar um booleano
        while (!mqttClient.connected())
        {
            DEBUG_PRINT("[MQTT] Conectando no broker...");

            if (mqttClient.connect(cfg::MQTT_CLIENT_ID))
            {
                DEBUG_PRINTLN("ok");

                mqttClient.subscribe(cfg::TOPIC_CMD_VENTILATION, 1);
                mqttClient.subscribe(cfg::TOPIC_HEALTH_RESPONSE, 1);

                DEBUG_PRINT("[MQTT] Inscrito em ");
                DEBUG_PRINTLN(cfg::TOPIC_CMD_VENTILATION);

                DEBUG_PRINT("[MQTT] Inscrito em ");
                DEBUG_PRINTLN(cfg::TOPIC_HEALTH_RESPONSE);

                mqtt_health::resetHealthCheckState();
            }
            else
            {
                if (++attempt >= cfg::MQTT_ATTEMPT_CONNECTION)
                {
                    // TODO: Seria interessante ter logs para saber se essa comunicação está falhano no Firebase
                    DEBUG_PRINTLN("[MQTT] Falha ao conectar após várias tentativas. Verifique o broker.");
                    return false;
                }

                DEBUG_PRINT("falhou! Nao contectou no broker state=");
                DEBUG_PRINTLN(mqttClient.state());
                delay(cfg::MQTT_ATTEMPT_DELAY);
            }
        }
        return true;
    }

    bool publishJson(const char* topic, JsonDocument& doc)
    {
        const size_t requiredSize = measureJson(doc);

        if (requiredSize >= 256)
        {
            DEBUG_PRINTLN("[ERRO] JSON grande demais para buffer local.");
            return false;
        }

        char buffer[256];
        size_t jsonSize = serializeJson(doc, buffer, sizeof(buffer));

        if (jsonSize == 0)
        {
            DEBUG_PRINTLN("[ERRO] Falha ao serializar JSON.");
            return false;
        }

        return mqttClient.publish(
            topic,
            reinterpret_cast<const uint8_t*>(buffer),
            jsonSize,
            false);
    }
}
