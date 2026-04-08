//
// Created by lucas on 06/04/2026.
//

#include "modules/mqtt/ventilation.h"
#include "modules/mqtt/mqtt_manager.h"
#include "modules/control/relay_action.h"
#include "config.h"
#include "debug.h"

bool ventilationOn = false;


namespace mqtt_ventilation
{
    static bool setVentilationState(bool turnOn)
    {
        relay_action::setRelayState(cfg::VENTILATION_PIN, turnOn == false);
        ventilationOn = turnOn;

        DEBUG_PRINT("[VENTILATION] Novo estado: ");
        DEBUG_PRINTLN(ventilationOn ? "ON" : "OFF");

        return true;
    }

    static void sendVentilationAck(
        int requestId,
        const char* action,
        bool success,
        const char* message)
    {
        JsonDocument ackDoc;
        ackDoc["request_id"] = requestId;
        ackDoc["type"] = "command_response";
        ackDoc["action"] = action;
        ackDoc["status"] = success ? "ok" : "error";
        ackDoc["message"] = message;
        ackDoc["device"] = "esp32";
        ackDoc["ventilation_on"] = ventilationOn;

        bool published = mqtt_manager::publishJson(cfg::TOPIC_ACK_VENTILATION, ackDoc);

        if (published)
            DEBUG_PRINTLN("[ACK] ACK de ventilacao enviado.");
        else
            DEBUG_PRINTLN("[ACK] Falha ao publicar ACK de ventilacao.");
    }

    void handleVentilationCommand(JsonDocument& doc)
    {
        int requestId = doc["request_id"] | -1;
        const char* type = doc["type"];
        JsonVariantConst actionNode = doc["action"];

        if (requestId < 0 || type == nullptr || actionNode.isNull())
        {
            DEBUG_PRINTLN("[ERRO] Command request incompleto.");
            DEBUG_PRINTLN(requestId);
            return;
        }

        if (strcmp(type, "command_request") != 0)
        {
            DEBUG_PRINTLN("[INFO] Mensagem ignorada: type inesperado.");
            DEBUG_PRINTLN(requestId);
            return;
        }

        bool success = false;
        const char* responseAction = nullptr;
        const char* responseMessage = "unsupported action";

        if (actionNode.is<bool>())
        {
            const bool turnOn = actionNode.as<bool>();
            responseAction = turnOn ? "true" : "false";
            setVentilationState(turnOn);
            success = true;
            responseMessage = turnOn ? "ventilation turned on" : "ventilation turned off";
        }
        else
        {
            const char* action = actionNode.as<const char*>();
            responseAction = action != nullptr ? action : "invalid";

            if (action != nullptr)
            {
                if (strcmp(action, "true") == 0 || strcmp(action, "on") == 0)
                {
                    setVentilationState(true);
                    success = true;
                    responseMessage = "ventilation turned on";
                }
                else if (strcmp(action, "false") == 0 || strcmp(action, "off") == 0)
                {
                    setVentilationState(false);
                    success = true;
                    responseMessage = "ventilation turned off";
                }
            }
        }

        sendVentilationAck(requestId, responseAction, success, responseMessage);
    }
}
