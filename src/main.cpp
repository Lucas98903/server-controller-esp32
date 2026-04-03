//
// Created by lucas on 11/03/2026.
//

#include <Arduino.h>

#include "../include/modules/wifi/wifi_connection.h"
#include "../include/modules/wifi/wifi_manager.h"
#include "modules/firebase_auth_manager.h"
#include "modules/ota_manager.h"
#include "modules/relay_action.h"
#include "modules/rtdb_manager.h"

#include "config.h"
#include "debug.h"

namespace
{
  rtdb_manager::DeviceData deviceData;

  bool statusInitialized = false;
  bool lastServerStatusSent = false;

  bool systemWasOperational = false;
  bool shouldRestoreServerAfterReconnect = false;
  bool isServerOn = false;

  unsigned long operationalFailureSinceMs = 0;
  unsigned long serverStateDebounceStartMs = 0;

  bool isRtdbOperational()
  {
    const unsigned long now = millis();
    const unsigned long lastOk = rtdb_manager::getLastSuccessfulCommunicationMs();

    if (rtdb_manager::isStreamConnected())
      return true;

    if (lastOk != 0 && (now - lastOk) < cfg::RTDB_COMMUNICATION_TIMEOUT_MS)
      return true;

    return false;
  }

  bool isSystemOperational()
  {
    return wifi_connection::isConnected() && isRtdbOperational();
  }

} // namespace

void networkTask(void * /*pvParameters*/)
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

void setup()
{
  DEBUG_BEGIN(115200);
  DEBUG_PRINTLN("[SYSTEM] Inicializando...");

  pinMode(cfg::STATUS_SUPPLY_PIN, INPUT);
  pinMode(cfg::STATUS_MOBO_PIN, INPUT);

  pinMode(cfg::VENTILATION_PIN, OUTPUT);
  pinMode(cfg::STARTER_PIN, OUTPUT);
  pinMode(cfg::LED_PIN, OUTPUT);
  pinMode(cfg::RESET_PIN, OUTPUT);
  pinMode(cfg::COOLER_127_PIN, OUTPUT);

  wifi_recovery::ensureConnected();

  relay_action::setRelayState(cfg::STARTER_PIN, false);
  relay_action::setRelayState(cfg::VENTILATION_PIN, false);
  relay_action::setRelayState(cfg::RESET_PIN, false);
  relay_action::setRelayState(cfg::COOLER_127_PIN, false);
  rtdb_manager::begin();

  xTaskCreatePinnedToCore(networkTask, "NetTask", 8192, nullptr,
                          1, // Core 1
                          nullptr,
                          0 // Core 0
  );
}

void loop()
{
  const unsigned long now = millis();
  const bool systemOperational = isSystemOperational();

  // -------------------------------------------------------------------------
  // PERDA DE CONECTIVIDADE UTIL (Wi-Fi + RTDB)
  // -------------------------------------------------------------------------
  if (!systemOperational)
  {
    if (systemWasOperational)
    {
      systemWasOperational = false;
      operationalFailureSinceMs = now;
      DEBUG_PRINTLN("[NET] Falha de conectividade util detectada. Aguardando confirmacao...");
    }

    if (operationalFailureSinceMs != 0 && (now - operationalFailureSinceMs >= cfg::OPERATIONAL_FAILURE_CONFIRM_MS))
    {
      const bool serverState = (digitalRead(cfg::STATUS_SUPPLY_PIN) == HIGH);

      if (serverState && !shouldRestoreServerAfterReconnect)
      {
        DEBUG_PRINTLN("[NET] Falha confirmada. Desligando servidor...");
        relay_action::pulsePowerButton();

        shouldRestoreServerAfterReconnect = true;
        DEBUG_PRINTLN("[NET] Servidor marcado para religar quando Wi-Fi e RTDB voltarem.");
      }

      operationalFailureSinceMs = 0;
    }

    const bool serverState = (digitalRead(cfg::STATUS_SUPPLY_PIN) == HIGH);
    digitalWrite(cfg::LED_PIN, serverState ? HIGH : LOW);
    return;
  }

  // -------------------------------------------------------------------------
  // RECUPERACAO DE CONECTIVIDADE UTIL
  // -------------------------------------------------------------------------
  if (!systemWasOperational)
  {
    systemWasOperational = true;
    DEBUG_PRINTLN("[NET] Conectividade util restabelecida.");

    if (shouldRestoreServerAfterReconnect)
    {
      const bool serverState = (digitalRead(cfg::STATUS_SUPPLY_PIN) == HIGH);

      if (!serverState)
      {
        DEBUG_PRINTLN("[NET] Religando servidor apos retorno de Wi-Fi/RTDB...");
        relay_action::pulsePowerButton();
      }
      else
      {
        DEBUG_PRINTLN("[NET] Servidor ja esta ligado.");
      }

      shouldRestoreServerAfterReconnect = false;
    }
  }

  // -------------------------------------------------------------------------
  // LEITURA DE COMANDOS DO RTDB
  // -------------------------------------------------------------------------
  rtdb_manager::DeviceData latest;

  // -------------------------------------------------------------------------
  // SINCRONISMO DE ESTADOS (com debounce)
  // -------------------------------------------------------------------------
  const bool rawServerState = (digitalRead(cfg::STATUS_SUPPLY_PIN) == HIGH);
  const bool rawMoboState = (digitalRead(cfg::STATUS_MOBO_PIN) == HIGH);

  if (rawServerState != lastServerStatusSent)
  {
    if (serverStateDebounceStartMs == 0)
      serverStateDebounceStartMs = now;

    if ((now - serverStateDebounceStartMs) >= cfg::SERVER_STATUS_DEBOUNCE_MS)
    {
      lastServerStatusSent = rawServerState;
      statusInitialized = true;
      serverStateDebounceStartMs = 0;
      rtdb_manager::enqueueServerStatusUpdate(rawServerState);
      DEBUG_PRINTLN("[RTDB] isPowerOn enfileirado para atualizacao.");
    }
  }
  else
  {
    serverStateDebounceStartMs = 0;
    if (!statusInitialized)
    {
      statusInitialized = true;
      rtdb_manager::enqueueServerStatusUpdate(rawServerState);
      DEBUG_PRINTLN(
          "[RTDB] isPowerOn enfileirado para atualizacao (inicial).");
    }
  }

  digitalWrite(cfg::LED_PIN, rawServerState ? HIGH : LOW);

  if (rawMoboState != deviceData.moboStatusServer)
  {
    deviceData.moboStatusServer = rawMoboState;
    rtdb_manager::enqueueMoboStatusUpdate(rawMoboState);
    DEBUG_PRINTLN("[RTDB] isMoboOn enfileirado para atualizacao.");
  }

  isServerOn = rawServerState || rawMoboState;
  if (rtdb_manager::consumeLatest(latest))
  {
    deviceData = latest;

    if (deviceData.turnServerOn)
    {
      DEBUG_PRINTLN("[ACTION] Comando recebido: ligar servidor.");
      relay_action::pulsePowerButton();
      rtdb_manager::enqueueClearTurnServerOn();
      rtdb_manager::enqueuePowerOnCountUpdate(deviceData.powerOnCount + 1);
    }

    if (deviceData.forcePowerOff)
    {
      DEBUG_PRINTLN("[ACTION] Comando recebido: forcar desligamento.");
      relay_action::forcePowerButton();
      rtdb_manager::enqueueClearForcePowerOff();
    }

    if (deviceData.resetServer)
    {
      DEBUG_PRINTLN("[ACTION] Comando recebido: resetar servidor.");
      relay_action::PulseResetButton();
      rtdb_manager::enqueueClearReset();
    }

    if (isServerOn)
    {
      relay_action::setRelayState(cfg::VENTILATION_PIN, deviceData.turnVentilationOn == false);
      relay_action::setRelayState(cfg::COOLER_127_PIN, deviceData.turnVentilation127On);
    }
  }

  if (!isServerOn)
  {
    relay_action::setRelayState(cfg::VENTILATION_PIN, false);
    relay_action::setRelayState(cfg::COOLER_127_PIN, false);
  }

  if (!deviceData.itsAlive)
  {
    DEBUG_PRINTLN("[ACTION] itsAlive esta false. Enfileirando atualizacao...");
    rtdb_manager::enqueueUpdateItsAlive(true);
    deviceData.itsAlive = true;
  }
}
