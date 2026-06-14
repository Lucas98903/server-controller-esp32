//
// Created by lucas on 13/03/2026.
//

#include "modules/firebase/firebase_auth_manager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "debug.h"
#include "secrets.h"

namespace
{
    WiFiClientSecure authClient;

    String g_idToken;
    String g_refreshToken;
    unsigned long g_tokenExpiresAtMs = 0;

    constexpr unsigned long TOKEN_REFRESH_MARGIN_MS = 5UL * 60UL * 1000UL;

    bool postJson(const String &url, const String &body, String &response)
    {
        HTTPClient https;
        authClient.setInsecure();

        if (!https.begin(authClient, url))
            return false;

        https.addHeader("Content-Type", "application/json");
        const int httpCode = https.POST(body);
        response = https.getString();
        https.end();

        return httpCode == 200;
    }

    bool parseAuthResponse(const String &response)
    {
        DynamicJsonDocument doc(4096);
        const auto error = deserializeJson(doc, response);
        if (error)
            return false;

        const char *idToken = doc["idToken"];
        const char *refreshToken = doc["refreshToken"];
        const char *expiresIn = doc["expiresIn"];

        if (!idToken || !refreshToken || !expiresIn)
            return false;

        g_idToken = idToken;
        g_refreshToken = refreshToken;

        const unsigned long expiresInSeconds = String(expiresIn).toInt();
        g_tokenExpiresAtMs = millis() + (expiresInSeconds * 1000UL);

        return true;
    }
}

namespace firebase_auth_manager
{
    bool signIn()
    {
        String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=";
        url += secrets::API_KEY;

        StaticJsonDocument<512> doc;
        doc["email"] = secrets::AUTH_EMAIL;
        doc["password"] = secrets::AUTH_PASSWORD;
        doc["returnSecureToken"] = true;

        String body;
        serializeJson(doc, body);

        String response;
        if (!postJson(url, body, response))
        {
            DEBUG_PRINTLN(" [AUTH] Falha no sign-in.");
            return false;
        }

        if (!parseAuthResponse(response))
        {
            DEBUG_PRINTLN("[AUTH] Falha ao parsear resposta do sign-in.");
            return false;
        }

        DEBUG_PRINTLN("[AUTH] Sign-in realizado com sucesso.");
        return true;
    }

    bool refreshIdTokenIfNeeded()
    {
        if (g_idToken.length() > 0 && millis() + TOKEN_REFRESH_MARGIN_MS < g_tokenExpiresAtMs)
            return true;

        if (g_refreshToken.isEmpty())
            return signIn();

        String url = "https://securetoken.googleapis.com/v1/token?key=";
        url += secrets::API_KEY;

        HTTPClient https;
        authClient.setInsecure();

        if (!https.begin(authClient, url))
            return false;

        https.addHeader("Content-Type", "application/x-www-form-urlencoded");

        String body = "grant_type=refresh_token&refresh_token=" + g_refreshToken;
        const int httpCode = https.POST(body);
        const String response = https.getString();
        https.end();

        if (httpCode != 200)
        {
            DEBUG_PRINTLN("[AUTH] Falha ao renovar token. Tentando sign-in completo...");
            return signIn();
        }

        DynamicJsonDocument doc(4096);
        const auto error = deserializeJson(doc, response);
        if (error)
            return false;

        const char *idToken = doc["id_token"];
        const char *refreshToken = doc["refresh_token"];
        const char *expiresIn = doc["expires_in"];

        if (!idToken || !refreshToken || !expiresIn)
            return false;

        g_idToken = idToken;
        g_refreshToken = refreshToken;
        g_tokenExpiresAtMs = millis() + (String(expiresIn).toInt() * 1000UL);

        DEBUG_PRINTLN("[AUTH] Token renovado com sucesso.");
        return true;
    }

    bool hasValidToken()
    {
        return !g_idToken.isEmpty() && millis() < g_tokenExpiresAtMs;
    }

    const String &getIdToken()
    {
        return g_idToken;
    }
}
