#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DES LUCIOLES, sous-type 130.
//
// UN SEUL bâtisseur et TROIS sites, un par identifiant. Chacun lui passe un
// octet, et cet octet fait tout : zéro, un, ou deux.

// LA TAILLE DU CARRÉ et la clé, les mêmes pour les trois.
inline constexpr float kWanderingMoteSquareRadius = 2.4F;
inline constexpr float kWanderingMoteUnusedKey = 0.15F;

// OÙ UNE LUCIOLE NAÎT : de moins vingt à plus vingt, sur les deux axes.
inline constexpr std::int32_t kWanderingMoteSpawnSpread = 41;
inline constexpr std::int32_t kWanderingMoteSpawnFloor = -20;

// ⚠ ET UNE HAUTEUR SE TIRE POUR RIEN. Le bâtisseur tire un nombre de zéro à
// quatre-vingt-dix-neuf pour chaque luciole, le range, puis REMET LES QUATRE À
// ZÉRO avant de rendre la main. Le tirage compte quand même : le sauter
// décalerait toute la suite du générateur.
inline constexpr std::int32_t kWanderingMoteDiscardedSpread = 100;

// COMBIEN DE TIRAGES par luciole, selon le style : sept d'ordinaire, cinq pour
// le solitaire qui ne tire ni abscisse ni profondeur.
inline constexpr std::size_t kWanderingMoteDrawsPerMote = 7;
inline constexpr std::size_t kWanderingMoteLoneDrawsPerMote = 5;

// L'OCTET QUI REND LES LUCIOLES BLANCHES plutôt que jaunes.
inline constexpr std::uint8_t kWanderingMoteWhiteStyle = 2;

// UN ESSAIM DE LUCIOLES qu'un effet pose.
struct EffectWanderingMotePattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'OCTET DU SITE : il choisit le nombre de pistes vives, le pas, la teinte,
  // et si les lucioles errent ou montent.
  std::uint8_t style_byte = 0;
};

[[nodiscard]] std::span<const EffectWanderingMotePattern> EffectWanderingMotePatternTable();

[[nodiscard]] std::span<const EffectWanderingMotePattern> EffectWanderingMotePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud — ou UNE SEULE vive pour l'octet un.
//
// ⚠ LES TROIS PISTES ÉTEINTES TIRENT QUAND MÊME. Le bâtisseur les éteint puis
// continue à tirer leurs angles ; les sauter décalerait la suite.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectWanderingMoteTracks(
    const EffectWanderingMotePattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectWanderingMoteNode(const EffectWanderingMotePattern& pattern,
                                                          glm::vec3 anchor,
                                                          std::int32_t effect_life_ticks,
                                                          std::uint32_t& generator);

}  // namespace ro::render
