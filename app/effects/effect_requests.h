#pragma once

#include <cstdint>

#include <glm/glm.hpp>

// CE QUE LES EFFETS DEMANDENT EN CHEMIN, hors de leur semis ordinaire : un
// cercle d'incantation à semer puis à relancer tant que le lanceur incante, et
// ce qu'une classe du gestionnaire demande sur son porteur en s'en allant.
//
// Des fonctions libres, sur l'API publique d'`EffectsInPlay` : elles vivent à
// côté de ce qui les appelle, et la mémoire de la carte leur est passée par
// qui la tient.
namespace moonclient {

class EffectsInPlay;
struct EffectMapMemory;

// LE CERCLE D'UNE INCANTATION, semé sur le lanceur et relancé tant qu'il
// incante — celui que la table des incantations donne comme celui qu'une
// action pose par son numéro. Vrai quand quelque chose a été semé.
bool SowCastRing(EffectsInPlay& effects, EffectMapMemory& memory, std::uint32_t actor_id,
                 glm::vec3 ground_position, std::uint32_t effect_id, std::uint32_t started_ms,
                 std::uint32_t cast_ms);

}  // namespace moonclient
