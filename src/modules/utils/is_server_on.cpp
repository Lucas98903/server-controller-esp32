//
// Created by lucas on 06/04/2026.
//
#include "modules/utils/is_server_on.h"

namespace server_status
{
    bool isMoboOn()
    {
        static bool lastRaw = false;
        static bool stable = false;
        static unsigned long lastChangeMs = 0;

        const bool raw = digitalRead(cfg::STATUS_MOBO_PIN) == HIGH;
        if (raw != lastRaw)
        {
            lastRaw = raw;
            lastChangeMs = millis();
        }
        if (millis() - lastChangeMs >= cfg::SERVER_STATUS_DEBOUNCE_MS)
            stable = lastRaw;

        return stable;
    }

    bool isSupplyOn()
    {
        static bool lastRaw = false;
        static bool stable = false;
        static unsigned long lastChangeMs = 0;

        const bool raw = digitalRead(cfg::STATUS_SUPPLY_PIN) == HIGH;
        if (raw != lastRaw)
        {
            lastRaw = raw;
            lastChangeMs = millis();
        }
        if (millis() - lastChangeMs >= cfg::SERVER_STATUS_DEBOUNCE_MS)
            stable = lastRaw;

        return stable;
    }

    bool isServerOn()
    {
        return isMoboOn() || isSupplyOn();
    }
}
