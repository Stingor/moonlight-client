#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "ro/render/actor_wardrobe.h"
#include "ro/sim/actor.h"

#include "world/battle_log_rules.h"

namespace moonclient {

// ── L'AGONIE D'UNE CRÉATURE ─────────────────────────────────────────────────
//
// Une créature morte joue une passe de son action de mort, puis s'en va : son
// CADAVRE, que plus rien ne désigne, prend sa place et s'efface — voir
// `ro/render/vanish_fade.h`. Seule une créature dont la mort laisse un cadavre
// en a une : voir `ro::sim::DeathAftermathOf`.

// Quand l'agonie commence, et ce qu'elle dure.
struct Agony {
  std::uint32_t start_ms = 0;
  float duration_ms = 0.0F;

  [[nodiscard]] std::uint32_t EndMs() const noexcept;
};

// L'INSTANT OÙ L'AGONIE COMMENCE : l'annonce de la mort `death_ms`, ou plus
// tard le dernier des messages de dégâts différés que reçoit `receiver_id`. Le
// client ne fait mourir la créature qu'une fois ces dégâts rejoués.
[[nodiscard]] std::uint32_t AgonyStartAfter(std::uint32_t death_ms, std::uint32_t receiver_id,
                                            std::span<const BattleMessage> messages) noexcept;

// L'AGONIE D'UNE CRÉATURE MORTE, partie à `start_ms` et vue dans la direction
// `facing` : autant d'images que l'action de mort de cette direction en porte,
// à son délai. Rien pour tout autre acteur.
[[nodiscard]] std::optional<Agony> AgonyOf(const ro::sim::Actor& actor,
                                           const ro::render::ActorOutfit& outfit,
                                           std::size_t facing, std::uint32_t start_ms);

}  // namespace moonclient
