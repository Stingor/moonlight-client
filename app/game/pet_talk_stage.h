#pragma once

// LA PAROLE DES FAMILIERS DANS LA BOUCLE DE LA PARTIE : ce que tout familier en
// vue dit ou montre, et ce que le nôtre doit dire, mis sur le fil.
//
// Voir `ro/sim/pet_talk.h` : c'est le client du MAÎTRE qui fait parler son
// familier, et l'écho du serveur qui le fait paraître chez tous — nous compris.

#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/pet.h"
#include "ro/sim/pet_talk.h"
#include "ro/sim/talk_mute.h"
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

  // `/notalkmsg` ET `/notalkmsg2` taisent la ligne d'une réplique dans la
  // conversation ; la bulle reste. Nuls : rien ne se tait.
  void SeeTalkMute(const ro::sim::TalkMute* mute) noexcept { talk_mute_ = mute; }

  // 🔴 LE COUP DU MAÎTRE SE LIT AVANT QUE LE MONDE N'APPLIQUE LA TRAME : la
  // chasse se juge sur ce qu'il était juste avant ce coup — au repos ou non —
  // et sur la cible qu'il avait mémorisée (voir `ro::sim::PetHuntWatch`).
  // `owner_at_rest` ne s'interroge que pour un coup du maître.
  void NoteBeforeWorld(const ro::net::Frame& frame, const ro::sim::Pet& pet,
                       const ro::sim::World& world, const std::function<bool()>& owner_at_rest,
                       std::uint32_t now_ms);
  // LE PAQUET DE PARAMÈTRE QUI REDIT LA VIE DU MAÎTRE, une fois le monde à jour :
  // c'est lui, et non le changement de la valeur, qui fait juger le danger.
  void NoteOwnerHealth(const ro::net::Frame& frame, const ro::sim::Pet& pet,
                       const ro::sim::World& world, std::uint32_t now_ms);
  // Le repas vient d'être pris.
  void NoteFed(const ro::sim::Pet& pet, const ro::sim::World& world);
  // Le serveur a tiré un spectacle : seul le nôtre parle. Vrai quand la trame
  // en annonce un, de quelque familier que ce soit.
  bool NotePerformance(const ro::net::Frame& frame, const ro::sim::Pet& pet,
                       const ro::sim::World& world);
  // À chaque image : ce que le maître et le familier viennent de vivre — la
  // mort, un niveau, un arrêt.
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
  const ro::sim::TalkMute* talk_mute_ = nullptr;

  // La cible que le maître a mémorisée, et la carte où il l'a fait : l'entrée
  // sur une autre l'oublie.
  ro::sim::PetHuntWatch hunt_;
  std::uint32_t hunt_map_entries_ = 0;

  // CE QU'ON A DÉJÀ VU DU MAÎTRE : un changement se juge contre lui, et rien
  // ne se dit à la première image.
  struct OwnerSeen {
    bool dead = false;
    std::int64_t base_level = 0;
  };
  std::optional<OwnerSeen> owner_;
};

}  // namespace moonclient
