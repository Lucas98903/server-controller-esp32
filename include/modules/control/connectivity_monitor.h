#pragma once

namespace connectivity_monitor
{
  // Retorna true se o sistema esta operacional (loop deve continuar).
  // Retorna false se ha falha de conectividade (loop deve retornar cedo).
  bool update();
}
