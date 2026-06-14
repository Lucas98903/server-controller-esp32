//
// Created by lucas on 13/03/2026.
//

#ifndef RTDB_MANAGER_H
#define RTDB_MANAGER_H

#include <Arduino.h>

namespace rtdb_manager
{
    struct DeviceData
    {
        bool turnServerOn = false;
        bool forcePowerOff = false;
        bool turnVentilationOn = false;
        bool turnVentilation127On = false;
        bool resetServer = false;

        bool isPowerOn = false;
        bool itsAlive = true;

        int powerOnCount = 0;
    };

    void begin();
    void maintainConnection();
    void processIncoming();

    bool consumeLatest(DeviceData &out);

    // Chamadas diretas (uso interno / network task)
    bool clearTurnServerOn();
    bool clearForcePowerOff();
    bool clearReset();
    bool updateServerStatus(bool isOn);
    bool updateItsAlive();

    // Fila de escritas pendentes — chamar do loop(), processar na network task
    void enqueueIsServerOn(bool isOn);
    void enqueueClearTurnServerOn();
    void enqueueClearForcePowerOff();
    void enqueueClearReset();
    void enqueueUpdateItsAlive(bool value);
    void processPendingWrites();

    bool isStreamConnected();
    unsigned long getLastSuccessfulCommunicationMs();
}

#endif
