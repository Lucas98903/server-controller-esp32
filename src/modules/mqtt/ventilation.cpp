//
// Created by lucas on 06/04/2026.
//

#include "modules/mqtt/ventilation.h"
#include "config.h"
#include "debug.h"
#include "modules/mqtt/mqtt_manager.h"

bool ventilationOn = false;

bool setVentilationState(bool turnOn)
{
    digitalWrite(RELAY_PIN, turnOn ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
    ventilationOn = turnOn;

    DEBUG_PRINT("[VENTILATION] Novo estado: ");
    DEBUG_PRINTLN(ventilationOn ? "ON" : "OFF");

    return true;
}

void handleVentilationCommand(JsonDocument& doc)
{
    int requestId = doc["request_id"] | -1;
    const char* type = doc["type"];
    const char* action = doc["action"];

    if (requestId < 0 || type == nullptr || action == nullptr)
    {
        DEBUG_PRINTLN("[ERRO] Command request incompleto.");
        return;
    }

    if (strcmp(type, "command_request") != 0)
    {
        DEBUG_PRINTLN("[INFO] Mensagem ignorada: type inesperado.");
        return;
    }

    bool success = false;
    const char* responseMessage = "unsupported action";

    if (strcmp(action, "true") == 0 || strcmp(action, "on") == 0)
    {
        success = setVentilationState(true);
        responseMessage = success ? "ventilation turned on" : "failed to turn ventilation on";
    }
    else if (strcmp(action, "false") == 0 || strcmp(action, "off") == 0)
    {
        success = setVentilationState(false);
        responseMessage = success ? "ventilation turned off" : "failed to turn ventilation off";
    }

    sendVentilationAck(requestId, action, success, responseMessage);
}

void sendVentilationAck(
    int requestId,
    const char* action,
    bool success,
    const char* message)
{
    StaticJsonDocument < 256 > ackDoc;
    ackDoc["request_id"] = requestId;
    ackDoc["type"] = "command_response";
    ackDoc["action"] = action;
    ackDoc["status"] = success ? "ok" : "error";
    ackDoc["message"] = message;
    ackDoc["device"] = "esp32";
    ackDoc["ventilation_on"] = ventilationOn;

    bool published = publishJson(TOPIC_ACK_VENTILATION, ackDoc);

    if (published)
    {
        DEBUG_PRINTLN(ln("[ACK] ACK de ventilacao enviado.");
    }
    else
    {
        DEBUG_PRINTLN(ln("[ACK] Falha ao publicar ACK de ventilacao.");
    }
}
