#pragma once

// LA PAROLE DES MONSTRES DANS LA BOUCLE DE LA PARTIE : la réplique qu'un acteur
// dit en frappant, au-dessus de sa tête et dans la conversation.
//
// Voir `ro/sim/monster_talk.h` pour la règle.

#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <span>
#include <string>

#include "ro/net/world_update.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/actor.h"
#include "ro/sim/monster_talk.h"
#include "ro/sim/talk_mute.h"
#include "ro/sim/world.h"
#include "ro/vfs/monster_talk_table.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// La couleur d'une réplique dans la conversation, en 0xRRGGBB.
inline constexpr std::uint32_t kMonsterLineColor = 0xFAFAFA;

class MonsterTalkStage {
 public:
  // La table se lit une fois ; absente, les monstres se taisent.
  void LoadTable(ro::vfs::Vfs& vfs);
  // UNE PARTIE NEUVE : les cibles mémorisées sont oubliées. La table reste.
  void StartSession();

  // `/notalkmsg` ET `/notalkmsg2` taisent la ligne d'une réplique dans la
  // conversation ; la bulle reste. Nuls : rien ne se tait.
  void SeeTalkMute(const ro::sim::TalkMute* mute) noexcept { talk_mute_ = mute; }
  // Le catalogue qui nomme l'espèce d'une classe, relevé à chaque image. Nul :
  // aucune espèce ne se nomme, et rien ne se dit.
  void SeeCatalog(const ro::render::SpriteCatalog* catalog) noexcept { catalog_ = catalog; }

  // 🔴 LE COUP SE LIT AVANT QUE LE MONDE N'APPLIQUE LA TRAME : il se juge sur ce
  // qu'était l'acteur qui frappe juste avant — au repos ou non — et sur la
  // cible qu'il avait mémorisée. `at_rest` ne s'interroge que pour un acteur
  // qui a quelque chose à dire.
  void NoteBeforeWorld(const ro::net::Frame& frame, ro::sim::World& world,
                       const std::function<bool(const ro::sim::Actor&)>& at_rest,
                       std::uint32_t now_ms);

 private:
  // Les répliques de coup de l'espèce de cet acteur ; vide pour un acteur sans
  // nom, ou sans table ni catalogue.
  [[nodiscard]] std::span<const std::string> LinesOf(const ro::sim::Actor& actor) const;
  void Say(const ro::sim::Actor& speaker, std::span<const std::string> lines, ro::sim::World& world,
           std::uint32_t now_ms);

  std::optional<ro::vfs::MonsterTalkTable> table_;
  const ro::render::SpriteCatalog* catalog_ = nullptr;
  const ro::sim::TalkMute* talk_mute_ = nullptr;
  ro::sim::StrikeMemories memories_;
  std::uint32_t memories_map_entries_ = 0;
  std::minstd_rand draw_;
};

}  // namespace moonclient
