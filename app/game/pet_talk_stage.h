#pragma once

// LA PAROLE DES FAMILIERS DANS LA BOUCLE DE LA PARTIE : ce que tout familier en
// vue dit ou montre, et ce que le nôtre doit dire, mis sur le fil.
//
// Voir `ro/sim/pet_talk.h` : c'est le client du MAÎTRE qui fait parler son
// familier, et l'écho du serveur qui le fait paraître chez tous — nous compris.

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/pet.h"
#include "ro/sim/pet_talk.h"
#include "ro/sim/world.h"
#include "ro/vfs/pet_talk_table.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// La couleur d'une réplique dans la conversation, en 0xRRGGBB.
inline constexpr std::uint32_t kPetLineColor = 0xFAFAFA;

class PetTalkStage {
 public:
  // La table se lit une fois ; absente, les familiers se taisent.
  void LoadTable(ro::vfs::Vfs& vfs);
  // UNE PARTIE NEUVE : ce qu'on a vu du personnage d'avant est oublié. La
  // table reste.
  void StartSession();

  // LA PAROLE D'UN FAMILIER EN VUE : la réplique au-dessus de sa tête et dans
  // la conversation, ou son émoticône. `kUnhandled` pour une autre trame.
  [[nodiscard]] ro::net::FrameOutcome Hear(const ro::net::Frame& frame, const ro::sim::Pet& pet,
                                           ro::sim::World& world, std::uint32_t now_ms);

  // ── CE QUE NOTRE FAMILIER DOIT DIRE, envoyé à la prochaine image ─────────

  // Le repas vient d'être pris.
  void NoteFed(const ro::sim::Pet& pet, const ro::sim::World& world);
  // Le serveur a tiré un spectacle : seul le nôtre parle. Vrai quand la trame
  // en annonce un, de quelque familier que ce soit.
  bool NotePerformance(const ro::net::Frame& frame, const ro::sim::Pet& pet,
                       const ro::sim::World& world);
  // À chaque image : ce que le maître et le familier viennent de vivre —
  // un coup porté, une blessure, la mort, un niveau, un arrêt.
  void Observe(const ro::sim::Pet& pet, const ro::sim::World& world, std::uint32_t now_ms);
  // Envoie ce qui attend.
  void Flush(ro::net::Connection& link);

 private:
  [[nodiscard]] int Roll();
  void Queue(const std::vector<std::int32_t>& codes);
  void ShowLine(const ro::sim::PetLineHeard& heard, std::uint32_t actor_id, const ro::sim::Pet& pet,
                ro::sim::World& world, std::uint32_t now_ms);

  std::optional<ro::vfs::PetTalkTable> table_;
  ro::sim::PetChatter chatter_;
  std::minstd_rand draw_;
  std::vector<std::int32_t> pending_;

  // CE QU'ON A DÉJÀ VU DU MAÎTRE : un changement se juge contre lui, et rien
  // ne se dit à la première image.
  struct OwnerSeen {
    // L'instant du dernier coup porté ; rien tant qu'il n'en a porté aucun.
    std::optional<std::uint32_t> strike_ms;
    std::int32_t health = 0;
    bool dead = false;
    std::int64_t base_level = 0;
  };
  std::optional<OwnerSeen> owner_;
};

}  // namespace moonclient
