#pragma once

#include <array>

#include "ro/net/connection.h"
#include "ro/sim/minigame_trophies.h"

namespace moonclient {

// LES TROPHÉES DES MINI-JEUX, dits au serveur (`CZ 0x0F4B`), qui achève pour
// chacun le succès qui lui correspond.
//
// 🔴 TOUS REDITS À CHAQUE POIGNÉE DE MAIN : un trophée gagné liaison coupée, ou
// avant que le serveur sache les recevoir, ne serait sinon jamais dit. Un succès
// déjà achevé ne coûte rien de son côté : il l'ignore.
class MinigameTrophyAnnouncer {
 public:
  // À CHAQUE IMAGE. Non vérifié : se réarme, rien n'est envoyé. Sinon, dit
  // chaque trophée gagné qui n'a pas été dit depuis le dernier réarmement.
  void Pump(ro::net::Connection& link, bool verified, const ro::sim::MinigameRecord& record);

 private:
  std::array<bool, ro::sim::kMinigameTrophyCount> told_ = {};
};

}  // namespace moonclient
