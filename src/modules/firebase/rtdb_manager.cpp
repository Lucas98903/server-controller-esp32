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

    constexpr unsigned long RECONNECT_INTERVAL_MS = 2000;
    constexpr size_t JSON_CAPACITY = 2048;

    // --- Chaves JSON (nós e folhas) ---

    constexpr const char *KEY_DESIRED_STATE = "desiredState";
    constexpr const char *KEY_CURRENT_STATE = "currentState";

    constexpr const char *KEY_TURN_SERVER_ON = "turnServerOn";
    constexpr const char *KEY_FORCE_POWER_OFF = "forcePowerOff";
    constexpr const char *KEY_VENTILATION_127 = "turnVentilation127On";
    constexpr const char *KEY_RESET_SERVER = "resetServer";
    constexpr const char *KEY_ITS_ALIVE = "itsAlive";
    constexpr const char *KEY_IS_POWER_ON = "isPowerOn";

    // --- Paths RTDB (SSE + HTTP PUT) ---

    constexpr const char *PATH_ROOT = "/";
    constexpr const char *PATH_DESIRED_STATE = "/desiredState";
    constexpr const char *PATH_DESIRED_TURN_SERVER_ON = "/desiredState/turnServerOn";
    constexpr const char *PATH_DESIRED_FORCE_POWER_OFF = "/desiredState/forcePowerOff";
    constexpr const char *PATH_DESIRED_VENTILATION_127 = "/desiredState/turnVentilation127On";
    constexpr const char *PATH_DESIRED_RESET_SERVER = "/desiredState/resetServer";
    constexpr const char *PATH_DESIRED_ITS_ALIVE = "/desiredState/itsAlive";
    constexpr const char *PATH_CURRENT_STATE = "/currentState";
    constexpr const char *PATH_CURRENT_IS_SERVER_ON = "/currentState/isServerOn";

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

        bool clearTurnServerOnPending = false;
        bool clearForcePowerOffPending = false;
        bool clearResetPending = false;

        bool updateItsAlivePending = false;
        bool itsAliveValue = true;
    };

    PendingWrites s_pending;

    template <typename F>
    void withPendingLock(F fn)
    {
        if (xSemaphoreTake(s_pendingMutex, pdMS_TO_TICKS(10)) != pdTRUE)
            return;
        fn();
        xSemaphoreGive(s_pendingMutex);
    }

    void applyBool(JsonVariantConst obj, const char *key, bool &field)
    {
        if (obj[key].is<bool>())
            field = obj[key].as<bool>();
    }

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
            s_lastSuccessfulMsCache = millis();

        return ok;
    }

    void applyDesiredStateObject(JsonVariantConst desiredState)
    {
        applyBool(desiredState, KEY_TURN_SERVER_ON, cachedData.turnServerOn);
        applyBool(desiredState, KEY_FORCE_POWER_OFF, cachedData.forcePowerOff);
        applyBool(desiredState, KEY_VENTILATION_127, cachedData.turnVentilation127On);
        applyBool(desiredState, KEY_RESET_SERVER, cachedData.resetServer);
        applyBool(desiredState, KEY_ITS_ALIVE, cachedData.itsAlive);
    }

    void applyCurrentStateObject(JsonVariantConst currentState)
    {
        applyBool(currentState, KEY_IS_POWER_ON, cachedData.isPowerOn);
    }

    void applyFullSnapshot(JsonVariantConst root)
    {
        if (root[KEY_DESIRED_STATE].is<JsonObjectConst>())
            applyDesiredStateObject(root[KEY_DESIRED_STATE]);

        if (root[KEY_CURRENT_STATE].is<JsonObjectConst>())
            applyCurrentStateObject(root[KEY_CURRENT_STATE]);
    }

    struct BoolLeaf
    {
        const char *path;
        bool rtdb_manager::DeviceData::*field;
    };

    constexpr BoolLeaf BOOL_LEAVES[] = {
        {PATH_DESIRED_TURN_SERVER_ON, &rtdb_manager::DeviceData::turnServerOn},
        {PATH_DESIRED_FORCE_POWER_OFF, &rtdb_manager::DeviceData::forcePowerOff},
        {PATH_DESIRED_VENTILATION_127, &rtdb_manager::DeviceData::turnVentilation127On},
        {PATH_DESIRED_RESET_SERVER, &rtdb_manager::DeviceData::resetServer},
        {PATH_DESIRED_ITS_ALIVE, &rtdb_manager::DeviceData::itsAlive},
        {PATH_CURRENT_IS_SERVER_ON, &rtdb_manager::DeviceData::isPowerOn},
    };

    void applyPartialUpdate(const String &path, JsonVariantConst data)
    {
        if (path == PATH_ROOT)
        {
            applyFullSnapshot(data);
            return;
        }
        if (path == PATH_DESIRED_STATE)
        {
            applyDesiredStateObject(data);
            return;
        }
        if (path == PATH_CURRENT_STATE)
        {
            applyCurrentStateObject(data);
            return;
        }

        if (data.is<bool>())
        {
            for (const auto &leaf : BOOL_LEAVES)
            {
                if (path == leaf.path)
                {
                    cachedData.*leaf.field = data.as<bool>();
                    return;
                }
            }
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

        const String path = doc["path"] | PATH_ROOT;
        JsonVariantConst data = doc["data"];

        if (xSemaphoreTake(s_dataMutex, portMAX_DELAY) == pdTRUE)
        {
            applyPartialUpdate(path, data);
            hasFreshData = true;
            s_lastSuccessfulMsCache = millis();
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

                    s_lastSuccessfulMsCache = millis();
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

    bool updateDesiredStateBool(const char *path, bool value)
    {
        return sendPut(path, value ? "true" : "false");
    }
}

namespace rtdb_manager
{
    void begin()
    {
        hasFreshData = false;
        currentEventName = "";
        currentEventData = "";
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
        return updateDesiredStateBool(PATH_DESIRED_TURN_SERVER_ON, false);
    }

    bool clearForcePowerOff()
    {
        return updateDesiredStateBool(PATH_DESIRED_FORCE_POWER_OFF, false);
    }

    bool clearReset()
    {
        return updateDesiredStateBool(PATH_DESIRED_RESET_SERVER, false);
    }

    bool updateItsAlive()
    {
        return updateDesiredStateBool(PATH_DESIRED_ITS_ALIVE, true);
    }

    bool updateServerStatus(bool isOn)
    {
        return sendPut(PATH_CURRENT_IS_SERVER_ON, isOn ? "true" : "false");
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

    void enqueueIsServerOn(bool isOn)
    {
        withPendingLock([&]
                        {
            s_pending.serverStatusPending = true;
            s_pending.serverStatusValue = isOn; });
    }

    void enqueueClearTurnServerOn()
    {
        withPendingLock([]
                        { s_pending.clearTurnServerOnPending = true; });
    }

    void enqueueClearForcePowerOff()
    {
        withPendingLock([]
                        { s_pending.clearForcePowerOffPending = true; });
    }

    void enqueueClearReset()
    {
        withPendingLock([]
                        { s_pending.clearResetPending = true; });
    }

    void enqueueUpdateItsAlive(bool value)
    {
        withPendingLock([&]
                        {
            s_pending.updateItsAlivePending = true;
            s_pending.itsAliveValue = value; });
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
            sendPut(PATH_CURRENT_IS_SERVER_ON, local.serverStatusValue ? "true" : "false");

        struct
        {
            bool PendingWrites::*flag;
            const char *path;
        } const clearOps[] = {
            {&PendingWrites::clearTurnServerOnPending, PATH_DESIRED_TURN_SERVER_ON},
            {&PendingWrites::clearForcePowerOffPending, PATH_DESIRED_FORCE_POWER_OFF},
            {&PendingWrites::clearResetPending, PATH_DESIRED_RESET_SERVER},
        };
        for (const auto &op : clearOps)
            if (local.*op.flag)
                updateDesiredStateBool(op.path, false);

        if (local.updateItsAlivePending)
            updateDesiredStateBool(PATH_DESIRED_ITS_ALIVE, local.itsAliveValue);
    }
}
