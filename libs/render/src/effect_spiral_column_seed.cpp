#include "ro/render/effect_spiral_column_seed.h"

#include <cmath>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spiral_column.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"
#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES OCTETS QUE LE CLIENT COMPARE, un à un.
constexpr std::uint8_t kStyleWideRing = 2;
constexpr std::uint8_t kStyleSixRing = 3;
constexpr std::uint8_t kStyleScattered = 10;
constexpr std::uint8_t kStyleTiered = 11;
constexpr std::uint8_t kStyleThreeWay = 12;
// LES OCTETS QUI PRENNENT LA VARIANTE QUATRE, comparés signés : un et moins.
constexpr std::int8_t kLastStyleOfFour = 1;
constexpr std::uint8_t kVariantOfFour = 4;
// LA VARIANTE DU CONSTRUCTEUR DE NŒUD, que garde un octet non nommé.
constexpr std::uint8_t kVariantOfConstructor = 1;

// CE QUE CHAQUE OCTET POSE, relevé dans les deux bâtisseurs.
constexpr std::int16_t kAlphaTiered = 50;
constexpr std::int16_t kAlphaThreeWay = 130;
constexpr std::int16_t kAlphaOrdinary = 150;
constexpr float kHeightOrdinary = 120.0F;
constexpr float kHeightTieredBase = 60.0F;
constexpr float kHeightTieredPerTrack = 20.0F;
constexpr float kHeightThreeWayBase = 30.0F;
constexpr float kHeightThreeWayPerTrack = 30.0F;

// ⚠ LE COMPTEUR NAÎT DÉJÀ HAUT, et c'est le retard. Le mouvement ne montre rien
// avant le centième tic ; un compteur semé entre soixante et quatre-vingt-dix-
// neuf fait donc attendre la piste de UN à QUARANTE tics. Le semer à zéro
// ferait attendre cent tics à tout le monde, et les quatre colonnes partiraient
// ensemble.
constexpr std::int32_t kDelayFloorOrdinary = 60;
constexpr std::int32_t kDelaySpreadOrdinary = 40;
constexpr std::int32_t kDelayFloorThreeWay = 50;
constexpr std::int32_t kDelaySpreadThreeWay = 50;
constexpr std::int32_t kDelaySpreadScattered = 100;

// DE COMBIEN LES QUATRE ANCRES SE DISPERSENT autour du point de l'effet.
constexpr float kScatterOrdinary = 5.0F;
constexpr float kScatterTieredFloor = 6.0F;
constexpr std::int32_t kScatterTieredSpread = 31;
constexpr float kScatterTieredStep = 0.1F;
constexpr std::int32_t kScatterThreeWaySpread = 17;
constexpr std::int32_t kScatterScatteredSpread = 41;

// LE RAYON DU CERCLE INTÉRIEUR : minuscule, et c'est voulu.
constexpr float kRadiusFloor = 0.15F;
constexpr std::int32_t kRadiusSpread = 11;
constexpr float kRadiusStep = 0.015F;

// L'INCLINAISON DES QUATRE PISTES : droite. C'est elle qui fait la colonne.
constexpr std::int16_t kUpright = 90;

}  // namespace

std::span<const EffectSpiralColumnPattern> EffectSpiralColumnPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectSpiralColumnPatternTable(), effect_id,
                           &EffectSpiralColumnPattern::effect_id);
}

std::uint8_t EffectSpiralColumnVariantFor(std::uint8_t style, bool aims_at_target,
                                          std::uint32_t& generator) {
  // ⚠ L'OCTET SE COMPARE SIGNÉ : zéro, un et tout octet au-delà de cent
  // vingt-sept prennent la branche du quatre.
  if (static_cast<std::int8_t>(style) <= kLastStyleOfFour) {
    return kVariantOfFour;
  }
  // ⚠ LE BÂTISSEUR QUI VISE N'A PAS DE BRANCHE TROIS-VOIES : son douze ne pose
  // rien, comme tout octet non nommé.
  if (aims_at_target && style == kStyleThreeWay) {
    return kVariantOfConstructor;
  }
  switch (style) {
    case kStyleWideRing:
      return 7;
    case kStyleSixRing:
      return 6;
    case kStyleTiered:
      return 5;
    case kStyleScattered:
      // Tirée entre quatre et huit.
      return static_cast<std::uint8_t>(DrawBelow(generator, 5) + 4);
    case kStyleThreeWay: {
      // ⚠ TROIS VALEURS QUI NE SE SUIVENT PAS : quatre, quarante, quarante et un.
      // Les prendre pour une plage donnerait trente-sept teintes que le client
      // n'emploie jamais.
      constexpr std::array<std::uint8_t, 3> kTintChoices = {4, 40, 41};
      return kTintChoices[static_cast<std::size_t>(DrawBelow(generator, 3))];
    }
    default:
      // ⚠ UN OCTET QUE LE CLIENT NE NOMME PAS NE POSE RIEN : la variante reste
      // celle du constructeur de nœud, UN — et non le quatre des petits octets.
      return kVariantOfConstructor;
  }
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpiralColumnTracks(
    const EffectSpiralColumnPattern& pattern, glm::vec3 aim, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    const auto etage = static_cast<float>(rang);
    track.active = true;
    track.degrees = static_cast<std::int16_t>(kEffectTrackFullTurnDegrees);
    track.azimuth_degrees =
        static_cast<std::int16_t>(kSpiralColumnDegreesPerTrack * static_cast<std::int16_t>(rang));
    track.tilt_degrees = kUpright;
    track.style = 0;

    float dispersion = kScatterOrdinary;
    if (pattern.style == kStyleTiered) {
      track.alpha = kAlphaTiered;
      // ⚠ CHAQUE COLONNE EST PLUS HAUTE QUE LA PRÉCÉDENTE pour ce style : c'est
      // un escalier, pas quatre colonnes égales.
      track.lift_key = kHeightTieredBase + kHeightTieredPerTrack * etage;
      track.ticks = DrawBelow(generator, kDelaySpreadOrdinary) + kDelayFloorOrdinary;
      dispersion =
          static_cast<float>(DrawBelow(generator, kScatterTieredSpread)) * kScatterTieredStep +
          kScatterTieredFloor;
    } else if (pattern.style == kStyleThreeWay) {
      track.alpha = kAlphaThreeWay;
      track.lift_key = kHeightThreeWayBase + kHeightThreeWayPerTrack * etage;
      track.ticks = DrawBelow(generator, kDelaySpreadThreeWay) + kDelayFloorThreeWay;
      dispersion = static_cast<float>(DrawBelow(generator, kScatterThreeWaySpread));
    } else {
      track.alpha = kAlphaOrdinary;
      track.lift_key = kHeightOrdinary;
      if (pattern.style == kStyleScattered) {
        // ⚠ CELUI-CI PEUT PARTIR TOUT DE SUITE, et attendre jusqu'à cent tics :
        // son compteur est tiré sur la plage entière au lieu de commencer haut.
        track.ticks = DrawBelow(generator, kDelaySpreadScattered);
        dispersion = static_cast<float>(DrawBelow(generator, kScatterScatteredSpread));
      } else {
        track.ticks = DrawBelow(generator, kDelaySpreadOrdinary) + kDelayFloorOrdinary;
      }
    }

    // L'ANCRE DE LA PISTE, posée sur un cercle à un cap TIRÉ sur le tour entier.
    // ⚠ DANS NOTRE REPÈRE, comme l'écart vers la cible qui s'y ajoute : la
    // profondeur se retourne avec l'ordonnée, et un cap du client se pose en
    // profondeur niée.
    const float cap =
        DegreesToRadians(static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees)));
    track.anchor = {dispersion * std::cos(cap), 0.0F, -dispersion * std::sin(cap)};
    track.radius_at_anchor =
        static_cast<float>(DrawBelow(generator, kRadiusSpread)) * kRadiusStep + kRadiusFloor;

    // ⚠ LES VINGT ET UNE CASES ET LEURS MARQUES SONT À ZÉRO : c'est le mouvement
    // qui ouvre la colonne, et rien ne paraît avant le centième tic.
    track.value.fill(0.0F);
    track.mark.fill(0);
    // ⚠ ET LES TROIS DERNIÈRES CASES DE LA QUEUE portent la cible, ou zéro. C'est
    // la seule différence entre les deux bâtisseurs du sous-type.
    track.tail[6] = pattern.aims_at_target ? aim.x : 0.0F;
    track.tail[7] = pattern.aims_at_target ? aim.y : 0.0F;
    track.tail[8] = pattern.aims_at_target ? aim.z : 0.0F;
  }
  return tracks;
}

PooledEffectNode SowEffectSpiralColumnNode(const EffectSpiralColumnPattern& pattern,
                                           glm::vec3 anchor, glm::vec3 aim,
                                           std::int32_t effect_life_ticks,
                                           std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kSpiralColumn, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.trail_variant =
      EffectSpiralColumnVariantFor(pattern.style, pattern.aims_at_target, generator);
  pooled.tracks = SeedEffectSpiralColumnTracks(pattern, aim, generator);
  return pooled;
}

}  // namespace ro::render
