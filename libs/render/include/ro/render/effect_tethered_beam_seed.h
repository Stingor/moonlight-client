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

// LE SEMIS DU TRAIT TENDU, sous-type 38.
//
// TROIS bâtisseurs et CINQ sites : deux jumeaux qui centrent un alpha sur
// l'acteur, et celui du nuage, qui sert trois identifiants et dont l'octet de
// site choisit la variante.
//
// ⚠ AUCUN TIRAGE : les trois bâtisseurs posent tout en dur. Le générateur ne
// bouge pas d'un cran.
//
// ⚠ UNE SEULE PISTE VIVE SUR QUATRE. Les trois autres sont éteintes
// explicitement par les trois bâtisseurs.

// CE QU'UN SITE POSE.
struct EffectTetheredBeamPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'ALPHA DE DÉPART. Zéro pour les deux centres alpha — leur mouvement le
  // lève —, quarante-cinq pour le nuage.
  std::int16_t alpha = 0;
  // LE RAYON DE L'ÂME. La gaine en prend le double.
  float core_radius = 0.0F;
  // LA VARIANTE : elle choisit les deux teintes, le mélange et l'alpha.
  float variant = 0.0F;
};

[[nodiscard]] std::span<const EffectTetheredBeamPattern> EffectTetheredBeamPatternTable();

[[nodiscard]] std::span<const EffectTetheredBeamPattern> EffectTetheredBeamPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, dont UNE SEULE est vive.
//
// `anchor` est la position du porteur, `target` celle de l'acteur visé. Une
// cible ABSENTE fait naître la piste ÉTEINTE, comme le client quand sa
// recherche échoue, et son mouvement ne la rallume jamais.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTetheredBeamTracks(
    const EffectTetheredBeamPattern& pattern, glm::vec3 anchor,
    const std::optional<glm::vec3>& target);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectTetheredBeamNode(const EffectTetheredBeamPattern& pattern,
                                                         glm::vec3 anchor,
                                                         const std::optional<glm::vec3>& target,
                                                         std::int32_t effect_life_ticks);

}  // namespace ro::render
