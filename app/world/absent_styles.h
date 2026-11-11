#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "ro/net/connection.h"
#include "ro/sim/world.h"

namespace moonclient {

// LES STYLES QU'ON DEMANDE POUR LES MEMBRES HORS DE VUE : guilde, groupe, amis.
//
// Les panneaux disent, image par image, quels membres ils affichent sans que la
// carte les montre (`Want`) ; `Pump` en demande au serveur seize au plus par
// seconde (`ro/net/absent_styles.h`). La réponse se range au livre du monde,
// sous le char_id, et `ActorStyles::DressAbsent` en habille la tête.
//
// 🔴 RIEN NE PART AVANT LA RÈGLE 26 : un serveur qui ne la dit pas ne sert pas
// le CZ 0x0F63, et le recevoir le ferait couper la liaison.
//
// ⚠ ON NE REDEMANDE PAS À CHAQUE OUVERTURE : une réponse vaut une minute, celle
// qui dit « rien à montrer » comprise. Une demande sans réponse se refait après
// cinq secondes, et un refus du serveur fait attendre ce qu'il dit.
class AbsentStyles {
 public:
  // Ce membre est affiché, et la carte ne le montre pas.
  void Want(std::uint32_t char_id, std::uint32_t now_ms);

  // Les char_id que la prochaine demande porterait à cet instant, dans l'ordre.
  [[nodiscard]] std::vector<std::uint32_t> Due(const ro::sim::World& world,
                                               std::uint32_t now_ms) const;

  // Une fois par image : la demande, si le serveur la sert et que la cadence le
  // permet.
  void Pump(ro::net::Connection& link, const ro::sim::World& world, std::uint32_t now_ms);

  // Pour l'essai : ce que `Pump` noterait d'un envoi, sans liaison.
  void NoteSent(const std::vector<std::uint32_t>& char_ids, std::uint32_t now_ms);
  // Ce que `Pump` lit d'un refus du serveur : l'attente qu'il impose.
  void HearRefusals(const ro::sim::World& world);
  [[nodiscard]] bool MaySend(const ro::sim::World& world, std::uint32_t now_ms) const;

 private:
  // L'instant où un panneau a voulu ce membre pour la dernière fois.
  std::map<std::uint32_t, std::uint32_t> wanted_at_ms_;
  // L'instant de la dernière demande de ce membre.
  std::map<std::uint32_t, std::uint32_t> asked_at_ms_;
  bool sent_once_ = false;
  std::uint32_t last_sent_ms_ = 0;
  std::uint32_t seen_refusal_ = 0;
  bool holding_ = false;
  std::uint32_t hold_until_ms_ = 0;
};

}  // namespace moonclient
