#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU MUR TRIPLE, sous-type 75.
//
// ⚠ CHAQUE EFFET POSE DEUX NŒUDS, aux azimuts ZÉRO et QUARANTE-CINQ. Le site
// appelle son bâtisseur deux fois de suite ; n'en poser qu'un donnerait la
// moitié des murs, entrelacés autrement.
//
// ⚠ L'ANGE D'UNE MONTÉE DE NIVEAU LES POSE À DEUX TICS DE SUITE : un nœud par
// tic, l'azimut zéro au soixante-cinquième et quarante-cinq au suivant. Voir
// `EffectTripleWallPattern::one_azimuth_per_tick`.

// DE COMBIEN LES QUATRE PISTES D'UN NŒUD S'ÉCARTENT : un quart de tour.
inline constexpr std::int16_t kTripleWallDegreesPerTrack = 90;

// LES DEUX AZIMUTS qu'un effet emploie, dans l'ordre où le site les passe.
inline constexpr std::array<std::int16_t, 2> kTripleWallNodeAzimuths = {0, 45};

// UN NŒUD DE MURS QU'UN EFFET POSE.
struct EffectTripleWallPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // LA VARIANTE que porte le nœud.
  std::uint8_t trail_variant = 0;
  // LA HAUTEUR DE DÉPART du mur, et la marque qui décide de son rythme.
  //
  // ⚠ UN BÂTISSEUR SUR QUATRE SÈME UNE HAUTEUR NULLE, et c'est celui dont la
  // marque vaut UN — la plus lente des deux. Son mur part donc de rien et monte
  // doucement ; les trois autres partent déjà hauts.
  float lift_key = 0.0F;
  std::uint8_t mark = 0;
  // QUAND cet effet pose ses deux nœuds.
  //
  // ⚠ UN DES TROIS BÂTISSEURS ATTEND LE CINQUIÈME TIC, et il en sert deux sur
  // quatre : son mur vert ne paraît qu'un quart de seconde après le début de
  // l'effet. Le semer au premier tic le montrerait en avance sur le reste.
  EffectNodeSchedule schedule;
  // VRAI QUAND CHAQUE TIC DE L'HORAIRE POSE UN SEUL AZIMUT, dans leur ordre. Faux
  // quand le seul tic de l'horaire pose les deux.
  bool one_azimuth_per_tick = false;
};

// LES AZIMUTS que ce patron pose à ce tic de son horaire.
[[nodiscard]] constexpr std::span<const std::int16_t> EffectTripleWallAzimuthsAt(
    const EffectTripleWallPattern& pattern, std::int32_t tick) {
  const std::span<const std::int16_t> azimuths = kTripleWallNodeAzimuths;
  return pattern.one_azimuth_per_tick
             ? azimuths.subspan(static_cast<std::size_t>(tick - pattern.schedule.first_tick), 1)
             : azimuths;
}

[[nodiscard]] std::span<const EffectTripleWallPattern> EffectTripleWallPatternTable();

[[nodiscard]] std::span<const EffectTripleWallPattern> EffectTripleWallPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, à partir de l'azimut que le site lui donne.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTripleWallTracks(
    const EffectTripleWallPattern& pattern, std::int16_t node_azimuth_degrees);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectTripleWallNode(const EffectTripleWallPattern& pattern,
                                                       std::int16_t node_azimuth_degrees,
                                                       glm::vec3 anchor,
                                                       std::int32_t effect_life_ticks);

}  // namespace ro::render
