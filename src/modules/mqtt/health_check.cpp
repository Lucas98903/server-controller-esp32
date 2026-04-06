//
// Created by lucas on 06/04/2026.
//

#include "modules/mqtt/health_check.h"
#include "modules/mqtt/mqtt_manager.h"
#include "modules/mqtt/ventilation.h"
#include "config.h"
#include "debug.h"

static unsigned long lastHealthCheckMs = 0;
static unsigned long healthRequestSentAtMs = 0;
static unsigned long lastServerHealthyMs = 0;

static bool waitingHealthResponse = false;
static int nextHealthRequestId = 1;
static int currentHealthRequestId = -1;
static int missedHealthResponses = 0;

bool serverAlive = false;

void resetHealthCheckState()
{
    waitingHealthResponse = false;
    lastHealthCheckMs = millis();
}

static void printServerHealthState()
{
    DEBUG_PRINT("[HEALTH] serverAlive=");
    DEBUG_PRINT(serverAlive ? "true" : "false");
    DEBUG_PRINT(" | missedHealthResponses=");
    DEBUG_PRINT(missedHealthResponses);
    DEBUG_PRINT(" | ventilationOn=");
    DEBUG_PRINTLN(ventilationOn ? "true" : "false");
}

void handleHealthResponse(JsonDocument& doc)
{
    int requestId = doc["request_id"] | -1;
    const char* type = doc["type"];
    const char* status = doc["status"];

    if (requestId < 0 || type == nullptr || status == nullptr)
    {
        DEBUG_PRINTLN("[ERRO] Health response incompleta.");
        return;
    }

    if (strcmp(type, "health_check_response") != 0)
    {
        DEBUG_PRINTLN("[INFO] Mensagem ignorada: type inesperado no health response.");
        return;
    }

    if (!waitingHealthResponse)
    {
        DEBUG_PRINTLN("[INFO] Health response recebida sem request pendente.");
        return;
    }

    if (requestId != currentHealthRequestId)
    {
        DEBUG_PRINTLN("[INFO] Health response ignorada: request_id nao confere.");
        return;
    }

    waitingHealthResponse = false;

    if (strcmp(status, "healthy") == 0)
    {
        serverAlive = true;
        missedHealthResponses = 0;
        lastServerHealthyMs = millis();

        DEBUG_PRINTLN("[HEALTH] Servidor marcado como HEALTHY.");
    }
    else
    {
        missedHealthResponses++;

        if (missedHealthResponses >= cfg::MAX_HEALTH_MISSES)
        {
            serverAlive = false;
        }

        DEBUG_PRINTLN("[HEALTH] Servidor respondeu, mas status nao e healthy.");
    }

    printServerHealthState();
}

void sendHealthCheckRequest()
{
    if (!mqttClient.connected())
    {
        DEBUG_PRINTLN("[HEALTH] MQTT desconectado. Health check nao enviado.");
        serverAlive = false;
        return;
    }

    currentHealthRequestId = nextHealthRequestId;
    nextHealthRequestId++;

    if (nextHealthRequestId <= 0)
    {
        nextHealthRequestId = 1;
    }

    StaticJsonDocument<192> requestDoc;
    requestDoc["request_id"] = currentHealthRequestId;
    requestDoc["type"] = "health_check_request";
    requestDoc["source"] = "esp32";

    bool published = mqtt_manager::publishJson(cfg::TOPIC_HEALTH_REQUEST, requestDoc);

    if (published)
    {
        waitingHealthResponse = true;
        healthRequestSentAtMs = millis();

        DEBUG_PRINTLN("[HEALTH] Health check enviado. request_id=");
        DEBUG_PRINTLN(currentHealthRequestId);
    }
    else
    {
        DEBUG_PRINTLN("[HEALTH] Falha ao publicar health check.");
        serverAlive = false;
    }
}

void processServerHealthCheck()
{
    unsigned long now = millis();

    if (!mqttClient.connected())
    {
        waitingHealthResponse = false;
        serverAlive = false;
        return;
    }

    if (waitingHealthResponse)
    {
        if (now - healthRequestSentAtMs >= cfg::HEALTH_TIMEOUT_MS)
        {
            waitingHealthResponse = false;
            missedHealthResponses++;

            DEBUG_PRINTLN("[HEALTH] Timeout aguardando resposta. Falhas seguidas: ");
            DEBUG_PRINTLN(missedHealthResponses);

            if (missedHealthResponses >= cfg::MAX_HEALTH_MISSES)
            {
                serverAlive = false;
                DEBUG_PRINTLN("[HEALTH] Servidor marcado como OFFLINE.");
            }

            printServerHealthState();
        }
        return;
    }

    if (now - lastHealthCheckMs >= cfg::HEALTH_INTERVAL_MS)
    {
        lastHealthCheckMs = now;
        sendHealthCheckRequest();
    }
}
