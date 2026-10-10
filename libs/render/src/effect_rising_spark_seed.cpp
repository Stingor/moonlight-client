#include "ro/render/effect_rising_spark_seed.h"

#include <string>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LES SEMIS DE CETTE FAMILLE PARTAGENT, relevé dans les cinq bâtisseurs.
//
// Aucun d'eux ne varie sur ces valeurs-là : les poser par patron donnerait cinq
// occasions de les désaccorder, et rien ne les rattraperait.

// ⚠ L'ALPHA NAÎT NUL, et l'étincelle ne se voit donc pas au tic où elle est
// semée. C'est le mouvement qui l'allume, quinze par tic pendant neuf tics.
constexpr std::int16_t kAlphaAtBirth = 0;
// Le cap se tire AU PREMIER TIC, pas au semis : le bâtisseur laisse zéro.
constexpr std::int16_t kAzimuthAtBirth = 0;
constexpr std::int16_t kAngleAtBirth = 0;
// LA CASE que le mouvement emploie pour écarter le carré selon son cap. Presque
// tous les bâtisseurs y posent zéro — leurs étincelles montent DROIT, et le cap
// tiré ne se voit nulle part —, et celui des étincelles de foudre y tire une
// dérive : voir `EffectRisingSparkPattern::drift`.
constexpr std::size_t kSlotOfDrift = 1;
constexpr std::size_t kSlotOfRise = 2;
// Les deux marques que le mouvement et la figure lisent.
constexpr std::size_t kMarkThatCarriesTheFlash = 0;
constexpr std::size_t kMarkThatAllowsTheFlash = 1;
constexpr std::size_t kMarkOfColour = 2;

// Un tirage rendu à sa plage. Une plage vide donne sa borne basse sans déranger
// le générateur : deux patrons voisins gardent ainsi la même suite de tirages.
// ⚠ AVEC SON NOMBRE DE VALEURS : voir `EffectNodeRange::values`.
[[nodiscard]] float DrawWithin(const EffectNodeRange& range, std::uint32_t& generator) {
  return DrawEffectNodeRange(range, generator);
}

// Un écart tiré entre moins la demi-largeur et plus, en `values` valeurs.
[[nodiscard]] float DrawAround(float half_width, std::int32_t values, std::uint32_t& generator) {
  return DrawWithin(EffectNodeRange{-half_width, half_width, values}, generator);
}

}  // namespace

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRisingSparkTracks(
    const EffectRisingSparkPattern& pattern, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    const bool living = rang < pattern.lit_tracks;
    track.active = living;
    track.alpha = kAlphaAtBirth;
    track.azimuth_degrees = kAzimuthAtBirth;
    track.degrees = kAngleAtBirth;
    // ⚠ DANS L'ORDRE DU CLIENT : le retard, le rayon, l'écart en abscisse puis
    // en profondeur, la dérive, et la montée en dernier.
    track.ticks = pattern.delay_spread_ticks > 0
                      ? pattern.delay_floor_ticks +
                            DrawEffectNodeInteger(0, pattern.delay_spread_ticks - 1, generator)
                      : 0;
    if (living) {
      track.radius_at_anchor = DrawWithin(pattern.half_size, generator);
    }
    // L'ancre sort d'ici en ÉCART autour du porteur ; c'est le semis du nœud qui
    // la pose dans le monde. Voir `SowEffectRisingSparkNode`.
    const float offset_x =
        DrawAround(pattern.scatter_half_width, pattern.scatter_values, generator);
    const float offset_z =
        DrawAround(pattern.scatter_half_width, pattern.scatter_values, generator);
    track.anchor = {offset_x, pattern.lift_above_owner, offset_z};
    track.value[kSlotOfDrift] = DrawWithin(pattern.drift, generator);
    track.value[kSlotOfRise] = DrawWithin(pattern.rise_per_tick, generator);
    track.mark[kMarkThatCarriesTheFlash] = 0;
    track.mark[kMarkThatAllowsTheFlash] = pattern.allows_flash ? 1 : 0;
    track.mark[kMarkOfColour] = pattern.colour_mark;
  }
  return tracks;
}

PooledEffectNode SowEffectRisingSparkNode(const EffectRisingSparkPattern& pattern,
                                          glm::vec3 ground_position, std::uint32_t& generator) {
  PooledEffectNode pooled;
  // ⚠ MÊME FIGURE QUE LES CARRÉS TEINTÉS, MOUVEMENT DIFFÉRENT — et c'est
  // pourquoi la forme les sépare. Voir `AdvanceRisingSparkTracks`.
  pooled.shape = PooledNodeShape::kRisingSparks;
  pooled.ground_position = ground_position;
  pooled.owner_position = ground_position;
  pooled.resource_paths = {std::string(pattern.texture)};
  if (!pattern.flash_texture.empty()) {
    pooled.resource_paths.emplace_back(pattern.flash_texture);
  }
  pooled.tracks = SeedEffectRisingSparkTracks(pattern, generator);
  // ⚠ L'ANCRE EST UN POINT DU MONDE, FIGÉ AU SEMIS. Le bâtisseur y range la
  // position du porteur plus l'écart tiré, et la figure n'ajoute jamais
  // l'origine du nœud (son dix-huitième paramètre vaut un). Le mouvement recopie
  // bien le porteur dans le NŒUD à chaque tic, mais aucune piste ne le relit :
  // une étincelle née reste où elle est née, et monte.
  for (EffectTrack& track : pooled.tracks) {
    track.anchor += ground_position;
  }
  // Le bâtisseur recopie la vie de son instance ; les pistes, elles, s'éteignent
  // d'elles-mêmes au cinquante-sixième tic.
  pooled.node.life_ticks = EffectNodeLifeFor(pattern.effect_id);
  return pooled;
}

}  // namespace ro::render
