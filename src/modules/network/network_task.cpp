//
// Created by lucas on 11/03/2026.
//

#include "modules/network/network_task.h"

#include <Arduino.h>

#include "modules/network/wifi/wifi_connection.h"
#include "modules/network/wifi/wifi_manager.h"
#include "modules/firebase/firebase_auth_manager.h"
#include "modules/network/ota_manager.h"
#include "modules/firebase/rtdb_manager.h"

#include "debug.h"

namespace
{
  void networkTaskFn(void * /*pvParameters*/)
  {
    bool otaInitialized = false;

    for (;;)
    {
      wifi_recovery::ensureConnected();

      if (wifi_connection::isConnected())
      {
        if (!otaInitialized)
        {
          ota_manager::begin();
          otaInitialized = true;
        }

        ota_manager::handle();

        if (firebase_auth_manager::refreshIdTokenIfNeeded())
        {
          rtdb_manager::maintainConnection();
          rtdb_manager::processIncoming();
          rtdb_manager::processPendingWrites();
        }
        else
        {
          DEBUG_PRINTLN("[AUTH] Sem token valido para acessar RTDB.");
        }
      }

      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
} // namespace

namespace network_task
{
  void begin()
  {
    xTaskCreatePinnedToCore(networkTaskFn, "NetTask", 8192, nullptr,
                            1,       // prioridade
                            nullptr,
                            0        // Core 0
    );
  }
} // namespace network_task
