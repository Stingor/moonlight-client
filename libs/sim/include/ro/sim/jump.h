#pragma once

#include <cstdint>

#include "ro/sim/actor.h"

// LE SAUT, purement cosmétique : le corps monte et redescend, sa case ne bouge
// pas. Ni la portée, ni les collisions, ni les chemins n'en tiennent compte.
namespace ro::sim {

// LA DURÉE D'UN SAUT, montée et descente.
//
// ⚠ C'EST AUSSI LE DÉLAI QUE LE SERVEUR IMPOSE entre deux sauts d'un même
// joueur, et il jette sans rien dire ceux qui arrivent plus tôt. Un arc plus
// court laisserait relancer un saut que les autres ne verraient pas.
inline constexpr std::uint32_t kJumpDurationMs = 600;

// La hauteur du sommet, en unités du monde : deux cases.
inline constexpr float kJumpCrestUnits = 10.0F;

// Vrai tant que l'arc de son dernier saut n'est pas retombé.
[[nodiscard]] bool Airborne(const Actor& actor, std::uint32_t now_ms);

// LA HAUTEUR DU CORPS AU-DESSUS DE SON SOL à `now_ms`, en unités du monde ;
// zéro hors d'un saut.
//
// 🔴 UNE PARABOLE FERMÉE, ET NON UNE CHUTE SOUS GRAVITÉ. La hauteur ne dépend
// que du temps écoulé : le corps retombe toujours sur le sol de la case où il
// se trouve à ce moment-là, même s'il a marché sur une pente pendant le saut.
// Une gravité simulée demanderait de savoir où atterrir, ce qu'un saut qui ne
// déplace personne n'a pas à décider.
[[nodiscard]] float JumpLiftAt(const Actor& actor, std::uint32_t now_ms);

}  // namespace ro::sim
