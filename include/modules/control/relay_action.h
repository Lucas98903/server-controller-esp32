//
// Created by lucas on 12/03/2026.
//

#ifndef RELAY_ACTION_H
#define RELAY_ACTION_H

namespace relay_action
{
    void setRelayState(int pin, bool active);
    void pulsePowerButton();
    void forcePowerButton();
    void PulseResetButton();
}

#endif // RELAY_ACTION_H