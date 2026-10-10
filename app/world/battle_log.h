#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ro/render/actor_wardrobe.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/strike_log.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/item_words.h"

#include "world/battle_log_rules.h"

namespace moonclient {

// LES LIGNES DE COMBAT, programmées au coup et posées à l'instant où le client
// les écrit : celui où chaque frappe touche. Voir `battle_log_rules.h`.
class BattleLog {
 public:
  explicit BattleLog(const ro::vfs::ItemWords& words) : words_(&words) {}

  // Relève les coups nouveaux et pose les lignes dues.
  //
  // ⚠ LE MONDE N'EST PAS CONSTANT : c'est lui qui porte la conversation. ⚠ À
  // APPELER DANS LE POMPAGE RÉSEAU, comme le rapport du ramassage : une image
  // sautée ne doit pas perdre une ligne.
  //
  // ⚠ LES NOMS SE LISENT AU COUP, et non à l'instant de la ligne : le client
  // les tire d'un dictionnaire qui survit au départ d'un acteur, là où notre
  // monde oublie le nom avec l'acteur.
  void Notice(ro::sim::World& world, ro::render::ActorWardrobe* wardrobe,
              const ro::sim::PartyRoster& party, std::uint32_t now_ms);

 private:
  struct Pending {
    BattleMessage message;
    std::string target_name;
    std::string attacker_name;
  };

  const ro::vfs::ItemWords* words_;
  ro::sim::StrikeCursor noticed_;
  std::vector<Pending> pending_;
};

}  // namespace moonclient
