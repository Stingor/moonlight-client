#include "ro/render/effect_growing_sprite_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES CINQ VARIANTES QUE LE CLIENT NOMME, style par style et par égalité. Elles
// ne se suivent pas : cent un, cent deux, cent onze, cent treize, cent quatorze.
constexpr std::uint8_t kVariantOfStyleOne = 101;
constexpr std::uint8_t kVariantOfStyleTwo = 102;
constexpr std::uint8_t kVariantOfStyleThree = 111;
constexpr std::uint8_t kVariantOfStyleFour = 113;
constexpr std::uint8_t kVariantOfStyleTen = 114;
// LA VARIANTE DU CONSTRUCTEUR DE NŒUD, que garde un style non nommé : il pose
// un flottant de un dans le champ que la primitive tronque.
constexpr std::uint8_t kVariantOfConstructor = 1;

constexpr std::uint8_t kStyleOne = 1;
constexpr std::uint8_t kStyleTwo = 2;
// LE STYLE QUI TIRE SA HAUTEUR, et le seul qui parte d'un rayon plus petit.
constexpr std::uint8_t kStyleThatDraws = 3;
constexpr std::uint8_t kStyleFour = 4;
constexpr std::uint8_t kStyleTen = 10;

// LES DEUX RAYONS DE DÉPART : treize pour le style qui tire, quinze pour tous
// les autres.
constexpr float kRadiusOfDrawingStyle = 13.0F;
constexpr float kRadiusOfOtherStyles = 15.0F;

// LA HAUTEUR, NÉGATIVE : le repère du client descend, donc le sprite est
// au-dessus de son ancre. Le style qui tire lui ajoute de zéro à cinq unités de
// plus.
constexpr float kHeightAboveAnchor = -5.0F;
constexpr std::uint32_t kHeightSpread = 6;

}  // namespace

std::span<const EffectGrowingSpritePattern> EffectGrowingSpritePatternsFor(
    std::uint32_t effect_id) {
  return core::RowsWithKey(EffectGrowingSpritePatternTable(), effect_id,
                           &EffectGrowingSpritePattern::effect_id);
}

std::uint8_t EffectGrowingSpriteVariantFor(std::uint8_t builder_argument) {
  switch (builder_argument) {
    case kStyleOne:
      return kVariantOfStyleOne;
    case kStyleTwo:
      return kVariantOfStyleTwo;
    case kStyleThatDraws:
      return kVariantOfStyleThree;
    case kStyleFour:
      return kVariantOfStyleFour;
    case kStyleTen:
      return kVariantOfStyleTen;
    default:
      // ⚠ UN STYLE QUE LE CLIENT NE NOMME PAS NE POSE RIEN, et son `switch` le
      // laisse passer sans écrire : la variante reste celle du constructeur,
      // c'est-à-dire UN — et non zéro. Lui en inventer une autre lui donnerait
      // celle d'un voisin.
      return kVariantOfConstructor;
  }
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGrowingSpriteTracks(
    const EffectGrowingSpritePattern& pattern, std::uint32_t& generator) {
  const bool drawn = pattern.builder_argument == kStyleThatDraws;
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ UNE SEULE PISTE VIVE : le bâtisseur éteint les trois autres nommément.
  EffectTrack& track = tracks.front();
  track.active = true;
  // ⚠ LE COMPTEUR PART NÉGATIF, et c'est le retard : la piste ne bouge pas tant
  // qu'il n'est pas remonté à un.
  track.ticks = pattern.delay_ticks;
  track.radius_at_anchor = drawn ? kRadiusOfDrawingStyle : kRadiusOfOtherStyles;
  track.value[kSlotOfGrowingSpriteHeight] =
      drawn ? kHeightAboveAnchor - static_cast<float>(NextEffectNodeDraw(generator) % kHeightSpread)
            : kHeightAboveAnchor;
  track.tilt_degrees = 0;
  // ⚠ L'ALPHA PART À ZÉRO : rien ne se voit tant que le mouvement n'a pas tourné.
  track.alpha = 0;
  return tracks;
}

PooledEffectNode SowEffectGrowingSpriteNode(const EffectGrowingSpritePattern& pattern,
                                            glm::vec3 anchor, std::int32_t effect_life_ticks,
                                            std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kGrowingSprite, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectGrowingSpriteTracks(pattern, generator);
  // ⚠ LA VARIANTE VIT DANS LE NŒUD, et DEUX lecteurs l'y prennent : la figure
  // pour sa teinte, le mouvement pour la vitesse de sa montée d'alpha.
  pooled.trail_variant = EffectGrowingSpriteVariantFor(pattern.builder_argument);
  return pooled;
}

}  // namespace ro::render
