#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES EMPREINTES : le sous-type 67, que le client sème sous les pas d'un
// coureur — la course du Taekwon, Chase Walk.
//
// UNE piste vive, un carré posé à plat et tourné dans le sens de la marche :
// c'est la figure du carré qui respire (voir `ComposeBreathingQuad`), sans
// souffle puisque rien ne fait bouger son inclinaison. Ce qui lui est propre est
// son MOUVEMENT : elle reste où elle est née et s'efface d'un cran tous les deux
// tics, de cent cinquante à zéro.

// UNE EMPREINTE QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur,
// et ce que l'octet du site en fait.
struct EffectFootprintPattern {
  std::uint32_t effect_id = 0;
  std::string_view texture;
  std::uint8_t builder_argument = 0;
  // Le style que la figure relit dans le nœud : cent pour le pas de course,
  // deux couches qui s'ajoutent ; zéro pour celui de Chase Walk, une couche
  // mêlée.
  std::int32_t node_style = 0;
  float radius_at_anchor = 0.0F;
  std::int16_t alpha = 0;
};

[[nodiscard]] std::span<const EffectFootprintPattern> EffectFootprintPatternTable();

// L'empreinte d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectFootprintPattern> EffectFootprintPatternsFor(
    std::uint32_t effect_id);

// La hauteur de l'empreinte, dans le repère du client qui descend : un
// cinquième d'unité au-dessus du sol.
inline constexpr float kFootprintClientHeight = -0.2F;

// Le mot de rendu que le bâtisseur écrit dans le nœud.
inline constexpr std::uint32_t kFootprintRenderWord = 1;

// LES PISTES D'UNE EMPREINTE : la première seule, tournée de `azimuth_degrees`
// — l'angle que celui qui la sème a calculé, en degrés du client.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFootprintTracks(
    const EffectFootprintPattern& pattern, std::int16_t azimuth_degrees);

// Le nœud d'une empreinte, posé à `anchor`.
[[nodiscard]] PooledEffectNode SowEffectFootprintNode(const EffectFootprintPattern& pattern,
                                                      glm::vec3 anchor,
                                                      std::int16_t azimuth_degrees,
                                                      std::int32_t effect_life_ticks);

}  // namespace ro::render
