//
// Created by lucas on 12/03/2026.
//

#include "modules/control/relay_action.h"

#include <Arduino.h>
#include "config.h"

namespace
{
    void pulseRelay(int pin, unsigned long activeTimeMs)
    {
        relay_action::setRelayState(pin, true);
        delay(activeTimeMs);
        relay_action::setRelayState(pin, false);
    }
}

namespace relay_action
{
    void setRelayState(int pin, bool active)
    {
        if (cfg::RELAY_ACTIVE_LOW)
        {
            digitalWrite(pin, active ? LOW : HIGH);
        }
        else
        {
            digitalWrite(pin, active ? HIGH : LOW);
        }
    }

    void pulsePowerButton()
    {
        pulseRelay(cfg::STARTER_PIN, cfg::POWER_ON_AND_RESET);
    }

    void forcePowerButton()
    {
        pulseRelay(cfg::STARTER_PIN, cfg::FORCE_POWER_OFF);
    }

    void PulseResetButton()
    {
        pulseRelay(cfg::RESET_PIN, cfg::POWER_ON_AND_RESET);
    }
}
