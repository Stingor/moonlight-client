#pragma once

#include <cstdint>

#include "ro/render/sprite_frame.h"
#include "ro/sim/actor.h"

// SI L'OMBRE D'UN ACTEUR SE PEINT À CETTE IMAGE. Sa taille, elle, ne dépend que
// de son apparence : voir `ro::render::ShadowScaleFor`.
namespace moonclient {

// Ce que la règle regarde d'un acteur.
struct ShadowFacts {
  ro::sim::ActorKind kind = ro::sim::ActorKind::kPlayer;
  ro::render::Motion motion = ro::render::Motion::kIdle;
  // Le métier MONTRÉ, celui dont l'acteur prend le corps.
  std::uint32_t shown_job = 0;
  // L'opacité du corps à cette image, de 0 à 255.
  std::uint8_t body_alpha = 255;
  // Vrai pour un acteur SORTI DE VUE qui s'efface encore : le client lui retire
  // son ombre en même temps que son identifiant.
  bool leaving_sight = false;
};

// TROIS RÈGLES, une par famille d'acteurs :
//
//   - un JOUEUR perd son ombre ASSIS ou MORT, et la garde dans tout autre geste,
//     quelle que soit son opacité. Seul le vol de l'union, qui lévite encore
//     assis, la garde assis ;
//   - une CRÉATURE ou un PNJ ne la porte que PLEINEMENT OPAQUE, quel que soit
//     son geste : une créature qui meurt la garde jusqu'à ce que son corps
//     commence à s'effacer, et n'en a pas tant qu'elle apparaît en fondu ;
//   - un OBJET AU SOL la porte toujours.
//
// Et aucun acteur qui sort de vue ne la garde pendant son fondu.
[[nodiscard]] bool CastsShadow(const ShadowFacts& facts) noexcept;

}  // namespace moonclient
