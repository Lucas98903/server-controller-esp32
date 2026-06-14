//
// Created by lucas on 13/03/2026.
//

#include "modules/firebase/rtdb_manager.h"
#include "modules/firebase/firebase_auth_manager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "debug.h"
#include "secrets.h"

namespace
{
    WiFiClientSecure streamClient;
    WiFiClientSecure writeClient;

    rtdb_manager::DeviceData cachedData;
    bool hasFreshData = false;

    String currentEventName;
    String currentEventData;

    unsigned long lastReconnectAttemptMs = 0;
    unsigned long lastSuccessfulCommunicationMs = 0;

    constexpr unsigned long RECONNECT_INTERVAL_MS = 2000;
    constexpr size_t JSON_CAPACITY = 2048;

    // --- Thread safety (Core 0 = rede, Core 1 = loop principal) ---

    SemaphoreHandle_t s_dataMutex = nullptr;
    SemaphoreHandle_t s_pendingMutex = nullptr;

    // Lidas pelo Core 1; escritas pelo Core 0 — valores atômicos de 32 bits no ESP32
    volatile bool s_streamConnectedCache = false;
    volatile unsigned long s_lastSuccessfulMsCache = 0;

    struct PendingWrites
    {
        bool serverStatusPending = false;
        bool serverStatusValue = false;

        bool moboStatusPending = false;
        bool moboStatusValue = false;

        bool clearTurnServerOnPending = false;
        bool clearForcePowerOffPending = false;
        bool clearResetPending = false;

        bool updateItsAlivePending = false;
        bool itsAliveValue = true;

        bool powerOnCountPending = false;
        int powerOnCountValue = 0;
    };

    PendingWrites s_pending;

    int parseJsonInt(JsonVariantConst value)
    {
        if (value.is<int>())
            return value.as<int>();

        if (value.is<long>())
            return static_cast<int>(value.as<long>());

        if (value.is<long long>())
            return static_cast<int>(value.as<long long>());

        return 0;
    }

    String buildStreamPath()
    {
        String path = secrets::RTDB_NODE_PATH;
        path += ".json";

        if (firebase_auth_manager::hasValidToken())
        {
            path += "?auth=";
            path += firebase_auth_manager::getIdToken();
        }

        return path;
    }

    String buildUrl(const char *suffix, bool silent = true)
    {
        String url = "https://";
        url += secrets::RTDB_HOST;
        url += secrets::RTDB_NODE_PATH;

        if (suffix != nullptr)
            url += suffix;

        url += ".json";

        bool hasQuery = false;

        if (firebase_auth_manager::hasValidToken())
        {
            url += "?auth=";
            url += firebase_auth_manager::getIdToken();
            hasQuery = true;
        }

        if (silent)
        {
            url += hasQuery ? "&" : "?";
            url += "print=silent";
        }

        return url;
    }

    bool beginHttp(HTTPClient &https, const String &url)
    {
        writeClient.setInsecure();

        if (!https.begin(writeClient, url))
        {
            DEBUG_PRINTLN("[RTDB] Falha ao iniciar HTTPClient.");
            return false;
        }

        return true;
    }

    bool sendPut(const char *suffix, const String &body)
    {
        HTTPClient https;
        const String url = buildUrl(suffix);

        if (!beginHttp(https, url))
            return false;

        https.addHeader("Content-Type", "application/json");

        const int httpCode = https.sendRequest("PUT", body);
        https.end();

        const bool ok = (httpCode == 200 || httpCode == 204);
        if (ok)
            lastSuccessfulCommunicationMs = millis();

        return ok;
    }

    void applyDesiredStateObject(JsonVariantConst desiredState)
    {
        if (desiredState["turnServerOn"].is<bool>())
            cachedData.turnServerOn = desiredState["turnServerOn"].as<bool>();

        if (desiredState["forcePowerOff"].is<bool>())
            cachedData.forcePowerOff = desiredState["forcePowerOff"].as<bool>();

        if (desiredState["turnVentilationOn"].is<bool>())
            cachedData.turnVentilationOn = desiredState["turnVentilationOn"].as<bool>();

        if (desiredState["turnVentilation127On"].is<bool>())
            cachedData.turnVentilation127On = desiredState["turnVentilation127On"].as<bool>();

        if (desiredState["resetServer"].is<bool>())
            cachedData.resetServer = desiredState["resetServer"].as<bool>();

        if (desiredState["itsAlive"].is<bool>())
            cachedData.itsAlive = desiredState["itsAlive"].as<bool>();
    }

    void applyCurrentStateObject(JsonVariantConst currentState)
    {
        if (currentState["isPowerOn"].is<bool>())
            cachedData.isPowerOn = currentState["isPowerOn"].as<bool>();

        if (currentState["isMoboOn"].is<bool>())
            cachedData.moboStatusServer = currentState["isMoboOn"].as<bool>();
    }

    void applyCountersObject(JsonVariantConst counters)
    {
        if (counters["powerOnCount"].is<int>() ||
            counters["powerOnCount"].is<long>() ||
            counters["powerOnCount"].is<long long>())
        {
            cachedData.powerOnCount = parseJsonInt(counters["powerOnCount"]);
        }
    }

    void applyFullSnapshot(JsonVariantConst root)
    {
        if (root["desiredState"].is<JsonObjectConst>())
            applyDesiredStateObject(root["desiredState"]);

        if (root["currentState"].is<JsonObjectConst>())
            applyCurrentStateObject(root["currentState"]);

        if (root["counters"].is<JsonObjectConst>())
            applyCountersObject(root["counters"]);
    }

    void applyPartialUpdate(const String &path, JsonVariantConst data)
    {
        if (path == "/")
        {
            applyFullSnapshot(data);
            return;
        }

        if (path == "/desiredState")
        {
            applyDesiredStateObject(data);
            return;
        }

        if (path == "/desiredState/turnServerOn" && data.is<bool>())
        {
            cachedData.turnServerOn = data.as<bool>();
            return;
        }

        if (path == "/desiredState/forcePowerOff" && data.is<bool>())
        {
            cachedData.forcePowerOff = data.as<bool>();
            return;
        }

        if (path == "/desiredState/turnVentilation127On" && data.is<bool>())
        {
            cachedData.turnVentilation127On = data.as<bool>();
            return;
        }

        if (path == "/desiredState/resetServer" && data.is<bool>())
        {
            cachedData.resetServer = data.as<bool>();
            return;
        }

        if (path == "/desiredState/turnVentilationOn" && data.is<bool>())
        {
            cachedData.turnVentilationOn = data.as<bool>();
            return;
        }

        if (path == "/currentState")
        {
            applyCurrentStateObject(data);
            return;
        }

        if (path == "/currentState/isPowerOn" && data.is<bool>())
        {
            cachedData.isPowerOn = data.as<bool>();
            return;
        }

        if (path == "/currentState/isMoboOn" && data.is<bool>())
        {
            cachedData.moboStatusServer = data.as<bool>();
            return;
        }

        if (path == "/counters")
        {
            applyCountersObject(data);
            return;
        }

        if (path == "/counters/powerOnCount")
        {
            cachedData.powerOnCount = parseJsonInt(data);
        }

        if (path == "/desiredState/itsAlive" && data.is<bool>())
        {
            cachedData.itsAlive = data.as<bool>();
            return;
        }
    }

    void processSseFrame()
    {
        if (currentEventName.isEmpty())
            return;

        if (currentEventName == "keep-alive")
            return;

        if (currentEventName == "cancel" || currentEventName == "auth_revoked")
        {
            DEBUG_PRINT("[RTDB] Stream encerrado. Evento: ");
            DEBUG_PRINTLN(currentEventName);
            streamClient.stop();
            return;
        }

        if (currentEventName != "put" && currentEventName != "patch")
            return;

        DynamicJsonDocument doc(JSON_CAPACITY);
        const DeserializationError error = deserializeJson(doc, currentEventData);

        if (error)
        {
            DEBUG_PRINT("[RTDB] Erro ao parsear SSE: ");
            DEBUG_PRINTLN(error.c_str());
            return;
        }

        const String path = doc["path"] | "/";
        JsonVariantConst data = doc["data"];

        if (xSemaphoreTake(s_dataMutex, portMAX_DELAY) == pdTRUE)
        {
            applyPartialUpdate(path, data);
            hasFreshData = true;
            lastSuccessfulCommunicationMs = millis();
            s_lastSuccessfulMsCache = lastSuccessfulCommunicationMs;
            xSemaphoreGive(s_dataMutex);
        }

        DEBUG_PRINT("[RTDB] Evento: ");
        DEBUG_PRINT(currentEventName);
        DEBUG_PRINT(" | path: ");
        DEBUG_PRINTLN(path);
    }

    bool openStream()
    {
        streamClient.stop();
        streamClient.setInsecure();
        streamClient.setTimeout(50);

        if (!streamClient.connect(secrets::RTDB_HOST, 443))
        {
            DEBUG_PRINTLN("[RTDB] Falha ao conectar stream.");
            return false;
        }

        const String target = buildStreamPath();

        streamClient.print("GET ");
        streamClient.print(target);
        streamClient.print(" HTTP/1.1\r\nHost: ");
        streamClient.print(secrets::RTDB_HOST);
        streamClient.print("\r\nAccept: text/event-stream\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\n\r\n");

        unsigned long start = millis();
        String statusLine;

        while (millis() - start < 5000)
        {
            while (streamClient.available())
            {
                String line = streamClient.readStringUntil('\n');

                if (statusLine.isEmpty())
                    statusLine = line;

                line.trim();

                if (line.isEmpty())
                {
                    const bool ok = statusLine.indexOf("200") >= 0;

                    if (!ok)
                    {
                        DEBUG_PRINT("[RTDB] HTTP inesperado no stream: ");
                        DEBUG_PRINTLN(statusLine);
                        streamClient.stop();
                        return false;
                    }

                    lastSuccessfulCommunicationMs = millis();
                    s_lastSuccessfulMsCache = lastSuccessfulCommunicationMs;
                    s_streamConnectedCache = true;
                    DEBUG_PRINTLN("[RTDB] Stream conectado.");
                    currentEventName = "";
                    currentEventData = "";
                    return true;
                }
            }
        }

        DEBUG_PRINTLN("[RTDB] Timeout aguardando headers.");
        streamClient.stop();
        return false;
    }

    bool updateDesiredStateBool(const char *fieldName, bool value)
    {
        String suffix = "/desiredState/";
        suffix += fieldName;

        return sendPut(suffix.c_str(), value ? "true" : "false");
    }
}

namespace rtdb_manager
{
    void begin()
    {
        hasFreshData = false;
        currentEventName = "";
        currentEventData = "";
        lastSuccessfulCommunicationMs = 0;
        s_lastSuccessfulMsCache = 0;
        s_streamConnectedCache = false;

        if (s_dataMutex == nullptr)
            s_dataMutex = xSemaphoreCreateMutex();
        if (s_pendingMutex == nullptr)
            s_pendingMutex = xSemaphoreCreateMutex();
    }

    void maintainConnection()
    {
        s_streamConnectedCache = streamClient.connected();

        if (s_streamConnectedCache)
            return;

        const unsigned long now = millis();
        if (now - lastReconnectAttemptMs < RECONNECT_INTERVAL_MS)
            return;

        lastReconnectAttemptMs = now;
        openStream();
    }

    void processIncoming()
    {
        while (streamClient.connected() && streamClient.available())
        {
            String line = streamClient.readStringUntil('\n');
            line.trim();

            if (line.startsWith("event:"))
            {
                currentEventName = line.substring(6);
                currentEventName.trim();
            }
            else if (line.startsWith("data:"))
            {
                currentEventData = line.substring(5);
                currentEventData.trim();
            }
            else if (line.isEmpty())
            {
                processSseFrame();
                currentEventName = "";
                currentEventData = "";
            }
        }
    }

    bool consumeLatest(DeviceData &out)
    {
        if (!hasFreshData)
            return false;

        if (xSemaphoreTake(s_dataMutex, pdMS_TO_TICKS(10)) != pdTRUE)
            return false;

        const bool fresh = hasFreshData;
        if (fresh)
        {
            out = cachedData;
            hasFreshData = false;

            // Campos one-shot: zera no cache imediatamente após consumir,
            // sem esperar o echo do Firebase. Evita duplo acionamento caso
            // outro SSE chegue antes do PUT de limpeza ser enviado.
            cachedData.turnServerOn = false;
            cachedData.forcePowerOff = false;
            cachedData.resetServer = false;
        }
        xSemaphoreGive(s_dataMutex);
        return fresh;
    }

    bool clearTurnServerOn()
    {
        return updateDesiredStateBool("turnServerOn", false);
    }

    bool clearForcePowerOff()
    {
        return updateDesiredStateBool("forcePowerOff", false);
    }

    bool clearReset()
    {
        return updateDesiredStateBool("resetServer", false);
    }

    bool updateItsAlive()
    {
        return updateDesiredStateBool("itsAlive", true);
    }

    bool updateServerStatus(bool isOn)
    {
        return sendPut("/currentState/isPowerOn", isOn ? "true" : "false");
    }

    bool updatePowerOnCount(int count)
    {
        return sendPut("/counters/powerOnCount", String(count));
    }

    bool isStreamConnected()
    {
        return s_streamConnectedCache;
    }

    unsigned long getLastSuccessfulCommunicationMs()
    {
        return s_lastSuccessfulMsCache;
    }

    // -------------------------------------------------------------------------
    // Enfileiramento de escritas (chamado do Core 1 / loop principal)
    // -------------------------------------------------------------------------

    void enqueueSupplyStatusUpdate(bool isOn)
    {
        if (xSemaphoreTake(s_pendingMutex, pdMS_TO_TICKS(10)) != pdTRUE)
            return;
        s_pending.serverStatusPending = true;
        s_pending.serverStatusValue = isOn;
        xSemaphoreGive(s_pendingMutex);
    }

    void enqueueMoboStatusUpdate(bool isOn)
    {
        if (xSemaphoreTake(s_pendingMutex, pdMS_TO_TICKS(10)) != pdTRUE)
            return;
        s_pending.moboStatusPending = true;
        s_pending.moboStatusValue = isOn;
        xSemaphoreGive(s_pendingMutex);
    }

    void enqueueClearTurnServerOn()
    {
        if (xSemaphoreTake(s_pendingMutex, pdMS_TO_TICKS(10)) != pdTRUE)
            return;
        s_pending.clearTurnServerOnPending = true;
        xSemaphoreGive(s_pendingMutex);
    }

    void enqueueClearForcePowerOff()
    {
        if (xSemaphoreTake(s_pendingMutex, pdMS_TO_TICKS(10)) != pdTRUE)
            return;
        s_pending.clearForcePowerOffPending = true;
        xSemaphoreGive(s_pendingMutex);
    }

    void enqueueClearReset()
    {
        if (xSemaphoreTake(s_pendingMutex, pdMS_TO_TICKS(10)) != pdTRUE)
            return;
        s_pending.clearResetPending = true;
        xSemaphoreGive(s_pendingMutex);
    }

    void enqueueUpdateItsAlive(bool value)
    {
        if (xSemaphoreTake(s_pendingMutex, pdMS_TO_TICKS(10)) != pdTRUE)
            return;
        s_pending.updateItsAlivePending = true;
        s_pending.itsAliveValue = value;
        xSemaphoreGive(s_pendingMutex);
    }

    void enqueuePowerOnCountUpdate(int count)
    {
        if (xSemaphoreTake(s_pendingMutex, pdMS_TO_TICKS(10)) != pdTRUE)
            return;
        s_pending.powerOnCountPending = true;
        s_pending.powerOnCountValue = count;
        xSemaphoreGive(s_pendingMutex);
    }

    // -------------------------------------------------------------------------
    // Executa as escritas pendentes (chamado da network task, Core 0)
    // -------------------------------------------------------------------------

    void processPendingWrites()
    {
        if (!firebase_auth_manager::hasValidToken())
            return;

        PendingWrites local;
        if (xSemaphoreTake(s_pendingMutex, pdMS_TO_TICKS(10)) != pdTRUE)
            return;
        local = s_pending;
        s_pending = PendingWrites{};
        xSemaphoreGive(s_pendingMutex);

        if (local.serverStatusPending)
            sendPut("/currentState/isPowerOn", local.serverStatusValue ? "true" : "false");

        if (local.moboStatusPending)
            sendPut("/currentState/isMoboOn", local.moboStatusValue ? "true" : "false");

        if (local.clearTurnServerOnPending)
            updateDesiredStateBool("turnServerOn", false);

        if (local.clearForcePowerOffPending)
            updateDesiredStateBool("forcePowerOff", false);

        if (local.clearResetPending)
            updateDesiredStateBool("resetServer", false);

        if (local.updateItsAlivePending)
            updateDesiredStateBool("itsAlive", local.itsAliveValue);

        if (local.powerOnCountPending)
            sendPut("/counters/powerOnCount", String(local.powerOnCountValue));
    }
}
