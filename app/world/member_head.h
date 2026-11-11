#pragma once

#include <cstdint>
#include <optional>

#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/style.h"
#include "ro/sim/world.h"

namespace moonclient {

class ActorStyles;
class AbsentStyles;

// CE QU'IL FAUT POUR HABILLER LA TÊTE D'UN MEMBRE DE LISTE. ⚠ TOUT PEUT
// MANQUER : sans garde-robe ou sans styles, la tête reste aux couleurs
// d'origine.
struct MemberHeadPieces {
  const ro::sim::World* world = nullptr;
  ActorStyles* styles = nullptr;
  ro::render::ActorWardrobe* wardrobe = nullptr;
  AbsentStyles* absent = nullptr;
};

// L'APPARENCE D'UNE TÊTE DE GUILDE, DE GROUPE OU D'AMI, habillée de sa recette
// entière, prête pour une vignette de tête.
//
// - En vue : l'apparence de son acteur, la coupe et la couleur que sa recette
//   impose, et le style que la carte lui a posé.
// - Hors de vue : ce que le serveur a dit sous son char_id, demandé au besoin,
//   et habillé par `ActorStyles::DressAbsent`.
// - Faute de réponse : `listed`, l'apparence que porte la liste (la guilde), aux
//   couleurs d'origine ; sans elle, rien.
//
// ⚠ L'HABIT DU GM ET LA TÊTE D'ORC NE VOYAGENT PAS : hors de vue, la tête est
// celle du personnage.
[[nodiscard]] std::optional<ro::render::ActorLook> MemberHeadLook(
    const MemberHeadPieces& pieces, std::uint32_t account_id, std::uint32_t char_id,
    const std::optional<ro::sim::AbsentLook>& listed, std::uint32_t now_ms);

}  // namespace moonclient
