#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DES DEUX RAYONS POINTÉS VERS LA CIBLE, sous-type 154.
//
// UN bâtisseur, cinq sites : 854, 858 et 863 sur `line3.tga` aux octets zéro,
// un et deux ; 1584 deux fois à l'octet deux, une image par branche du réglage
// des effets allégés — le client n'en appelle qu'une, et nous semons celle dont
// l'image se lit, comme pour l'essaim du même cas.
//
// ⚠ UN NŒUD AU PREMIER TIC SEULEMENT : la garde est l'âge nul de l'instance.
// ⚠ LE NŒUD NAÎT SUR L'INSTANCE, et n'en bouge plus : le mouvement ne recopie
// pas la place du porteur.

// LE MOT DE RENDU que le bâtisseur écrit en dur. Voir `effect_render_word.h`.
inline constexpr std::uint32_t kAimedSpokeRenderWord = 5;

// CE QUE LE BÂTISSEUR ÉCRIT EN DUR dans la première piste : la longueur et la
// largeur d'un rayon, lues par la figure ; un rayon de cent et une case à
// quatre-vingts que personne ne relit.
inline constexpr float kAimedSpokeLength = 50.0F;
inline constexpr float kAimedSpokeWidth = 5.0F;
inline constexpr float kAimedSpokeUnreadRadius = 100.0F;
inline constexpr float kAimedSpokeUnreadValue = 80.0F;

// UN SITE : l'effet, la fonction du client, l'image et l'octet qu'il passe.
struct EffectAimedSpokePattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  std::uint8_t octet = 0;
};

// La table entière, triée par identifiant.
[[nodiscard]] std::span<const EffectAimedSpokePattern> EffectAimedSpokePatternTable();

[[nodiscard]] std::span<const EffectAimedSpokePattern> EffectAimedSpokePatternsFor(
    std::uint32_t effect_id);

// LE CAP DU CLIENT, en degrés entiers, du nœud vers la cible — deux points de
// NOTRE monde. Zéro sans cible : un vecteur nul rend zéro, comme chez le client.
[[nodiscard]] std::int16_t AimedSpokeClientHeading(glm::vec3 node_position,
                                                   const std::optional<glm::vec3>& target);

// LES QUATRE PISTES DU NŒUD, la première seule allumée, alpha nul.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedAimedSpokeTracks(
    std::uint8_t octet, std::int16_t client_heading_degrees);

// LE NŒUD qu'un patron décrit, né à la place de l'instance et tourné vers
// `target`.
[[nodiscard]] PooledEffectNode SowAimedSpokeNode(const EffectAimedSpokePattern& pattern,
                                                 glm::vec3 instance_position,
                                                 const std::optional<glm::vec3>& target,
                                                 std::int32_t effect_life_ticks);

}  // namespace ro::render
