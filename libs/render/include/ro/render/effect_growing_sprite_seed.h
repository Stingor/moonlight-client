#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SPRITE QUI ENFLE, et les effets du sous-type 88.
//
// Une image, un RETARD et un style. Une seule piste vive, alpha nul au départ
// — c'est le mouvement qui l'allume.
//
// ⚠ LE RETARD EST UN COMPTEUR NÉGATIF, et c'est tout le sujet d'EF_STASIS. Il
// pose SIX sprites au même tic, avec des retards de moins dix à moins trente par
// pas de quatre : la piste ne bouge pas tant que son compteur n'est pas remonté
// à un, si bien que les six boules de foudre paraissent l'une après l'autre,
// tous les quatre tics. Les semer sans retard les ferait toutes naître ensemble.
//
// ⚠ ET LE STYLE TROIS TIRE SA HAUTEUR AU SORT, seul de la famille : cinq unités
// plus un tirage sur six. C'est ce qui disperse les six boules en hauteur ; leur
// donner la même les empilerait sur une seule ligne.

// LES CASES QUE CE SEMIS REMPLIT, et leur sens appartient à la figure qu'il
// partage avec le carré qui respire et le disque au sol (ADR 0017).
inline constexpr std::size_t kSlotOfGrowingSpriteHeight = 0;

// UN SPRITE QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectGrowingSpritePattern {
  std::uint32_t effect_id = 0;
  // L'étiquette de la routine du client qui pose ce sprite.
  std::string_view client_function;
  std::string_view texture;
  // LE RETARD, en tics et NÉGATIF : le compteur de la piste part de là.
  std::int32_t delay_ticks = 0;
  std::uint8_t builder_argument = 0;
};

// Tous les sprites connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectGrowingSpritePattern> EffectGrowingSpritePatternTable();

// TOUS les sprites d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectGrowingSpritePattern> EffectGrowingSpritePatternsFor(
    std::uint32_t effect_id);

// LA TEINTE DU NŒUD QU'UN STYLE POSE, et un pour un style que le client ne
// nomme pas — il laisse alors le champ tel que le constructeur l'a mis.
//
// ⚠ CE N'EST PAS UNE ÉCHELLE MAIS UN NUMÉRO DE VARIANTE, et le MOUVEMENT le lit
// aussi : la variante cent treize monte son alpha trois fois plus vite que les
// autres. Voir `AdvanceGrowingSpriteTracks`.
[[nodiscard]] std::uint8_t EffectGrowingSpriteVariantFor(std::uint8_t builder_argument);

// LES QUATRE PISTES D'UN SPRITE : une seule vive, et sa hauteur tirée pour un
// seul style.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGrowingSpriteTracks(
    const EffectGrowingSpritePattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, piste semée, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectGrowingSpriteNode(const EffectGrowingSpritePattern& pattern,
                                                          glm::vec3 anchor,
                                                          std::int32_t effect_life_ticks,
                                                          std::uint32_t& generator);

}  // namespace ro::render
