#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_fading_sprite.h"
#include "ro/render/effect_node_fwd.h"
#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_track.h"

namespace ro::render {

// LE SEMIS DU SPRITE QUI S'ÉLÈVE, sous-type 85.
//
// UN seul bâtisseur, et SEPT couples `.spr`/`.act` : l'ange, ses ailes, trois plumes, et l'ange en
// hanbok avec les siennes.
//
// ⚠ C'EST UNE FONCTION PARTAGÉE AU SENS DE L'ADR 0019 : son premier paramètre
// choisit le couple, et c'est le SITE D'APPEL qui le pousse. Lire le bâtisseur
// seul donnerait sept chemins et aucun moyen de savoir lequel va à qui.
//
// ⚠ ET UN EFFET APPELLE SON BÂTISSEUR PLUSIEURS FOIS. `EF_ANGEL2` le fait
// quatre fois — l'ange, ses ailes, deux plumes —, et la table porte donc une
// rangée par APPEL et non une par effet. N'en semer qu'une montrerait un ange
// sans ailes.

// DE COMBIEN L'ANCRE NAÎT PLUS HAUT que le point de l'effet : notre axe vertical
// monte, c'est donc une addition.
inline constexpr float kFadingSpriteSpawnHeight = 40.0F;
inline constexpr float kFadingSpriteHanbokSpawnHeight = 30.0F;

// LE MOT DE RENDU de chaque style. Voir `effect_render_word.h`.
//
// ⚠ LE CORPS DU HANBOK SEUL NE S'AJOUTE PAS : il se mélange à l'ordinaire et passe
// par-dessus tout, décor et acteurs compris. Les autres sprites s'ajoutent, sous
// l'épreuve de profondeur.
inline constexpr std::uint32_t kFadingSpriteRenderWord = kEffectRenderWordOfAdditiveSprite;
inline constexpr std::uint32_t kFadingSpriteHanbokBodyRenderWord =
    kEffectRenderWordOfConstructor | kEffectRenderWordOverEverything;

// UN APPEL du bâtisseur : un sprite, un style, et quand il naît.
struct EffectFadingSpritePattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  // Le couple `.spr`/`.act` SANS extension : les deux portent le même nom.
  std::string_view sprite_base;
  std::uint8_t style = 0;
  // QUAND ce nœud naît. Les ailes se resèment tous les trois tics jusqu'au
  // dix-huitième ; tout le reste ne naît qu'au tic zéro.
  EffectNodeSchedule schedule;
};

[[nodiscard]] std::span<const EffectFadingSpritePattern> EffectFadingSpritePatternTable();

[[nodiscard]] std::span<const EffectFadingSpritePattern> EffectFadingSpritePatternsFor(
    std::uint32_t effect_id);

// LA PISTE d'un nœud : une seule vive, qui porte l'ancre et les deux marques.
//
// `tick` est le tic de l'instance où le nœud naît : le bâtisseur le range dans
// la marque, et le mouvement en tire le plafond d'alpha.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFadingSpriteTracks(
    const EffectFadingSpritePattern& pattern, glm::vec3 anchor, std::int32_t tick);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectFadingSpriteNode(const EffectFadingSpritePattern& pattern,
                                                         glm::vec3 anchor,
                                                         std::int32_t effect_life_ticks,
                                                         std::int32_t tick);

}  // namespace ro::render
