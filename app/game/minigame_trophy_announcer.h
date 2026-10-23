#pragma once

#include <array>
#include <cstdint>

#include "ro/net/connection.h"
#include "ro/sim/minigame_trophies.h"

namespace moonclient {

// LES TROPHÉES DES MINI-JEUX, dits au serveur (`CZ 0x0F4B`), qui achève pour
// chacun le succès qui lui correspond.
//
// 🔴 TOUS REDITS À CHAQUE POIGNÉE DE MAIN : un trophée gagné liaison coupée, ou
// avant que le serveur sache les recevoir, ne serait sinon jamais dit. Un succès
// déjà achevé ne coûte rien de son côté : il l'ignore.
//
// 🔴 ET À CHAQUE ENTRÉE EN CARTE, même serveur et même carte compris : entre le
// changement de carte et l'avis de chargement, le serveur jette tout ce qu'il
// reçoit, et un trophée dit dans l'intervalle attendrait sinon la poignée de
// main suivante.
class MinigameTrophyAnnouncer {
 public:
  // À CHAQUE IMAGE. Non vérifié : se réarme, rien n'est envoyé. Sinon, dit
  // chaque trophée gagné qui n'a pas été dit depuis le dernier réarmement —
  // qu'un `map_entries` différent du précédent provoque aussi. Voir
  // `ro::sim::World::map_entries`.
  void Pump(ro::net::Connection& link, bool verified, std::uint32_t map_entries,
            const ro::sim::MinigameRecord& record);

 private:
  std::array<bool, ro::sim::kMinigameTrophyCount> told_ = {};
  // Le compte d'entrées en carte au dernier réarmement.
  std::uint32_t told_map_entries_ = 0;
};

}  // namespace moonclient
