#include "ro/render/effect_ground_ribbon_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LES QUATRE BÂTISSEURS PARTAGENT.
constexpr float kMeasureAtAnchor = 100.0F;
constexpr std::int16_t kStartsInvisible = 0;
// L'écart latéral : quinze, plus un tirage sur onze, d'un côté ou de l'autre.
constexpr float kSidewaysBase = 15.0F;
constexpr std::int32_t kSidewaysSpread = 11;
// L'extrémité proche se tire sur deux cents, la même borne que celle où le
// mouvement referme son glissement.
constexpr std::int32_t kNearEndSpread = 200;

// LES IMAGES QUE DEUX DES QUATRE CHOISISSENT EUX-MÊMES.
constexpr std::string_view kPlainBeam = "data\\texture\\effect\\line3.tga";
constexpr std::string_view kYellowBeam = "data\\texture\\effect\\line3y.tga";

// LES TROIS FAMILLES DE CONSTANTES, que trois bâtisseurs sur quatre partagent.
// Chacune donne la clé, l'étendue et la marque.
struct RibbonFamily {
  std::int32_t key_spread;
  float key_base;
  std::int32_t span_spread;
  float span_base;
  std::uint8_t mark_base;
  std::int32_t step_spread;
  float step_base;
};

// ⚠ LA FAMILLE BASSE TIRE SA CLÉ SUR QUATRE, pas sur six : le client y emploie
// un ET binaire au lieu d'un modulo. Prendre six pour les trois familles
// élargirait le ruban d'un cinquième là où il doit rester fin.
constexpr RibbonFamily kLowFamily = {4, 0.3F, 31, 20.0F, 0, 21, 1.5F};
constexpr RibbonFamily kMidFamily = {6, 0.5F, 31, 40.0F, 10, 11, 1.0F};
constexpr RibbonFamily kHighFamily = {4, 0.4F, 31, 30.0F, 20, 21, 1.5F};

// ⚠ ET UN BÂTISSEUR AJOUTE TRENTE À SA MARQUE, ce qui l'envoie dans la moitié
// haute des styles — ceux qui s'ajoutent et sont aplatis. Son jumeau n'ajoute
// rien et reste dans la moitié basse, qui se mélange. Deux bâtisseurs qui ne
// diffèrent que par ce nombre donnent deux rendus opposés.
constexpr std::uint8_t kAddedStyleBase = 30;

constexpr std::int32_t kDecade = 10;
constexpr std::int32_t kSecondDecade = 20;

// LA MARQUE SE TIRE SUR TROIS VALEURS au-dessus de sa base.
constexpr std::int32_t kMarkSpread = 3;
// LES DIZAINES D'ARGUMENT OÙ LE BÂTISSEUR TEXTURÉ RETIRE SA MARQUE, et la base
// de chacune.
constexpr std::int32_t kThirdDecade = 2;
constexpr std::int32_t kFourthDecade = 3;
constexpr std::int32_t kRedrawnMarkOfThirdDecade = 33;
constexpr std::int32_t kRedrawnMarkOfFourthDecade = 36;

[[nodiscard]] std::int32_t RollBelow(std::uint32_t& generator, std::int32_t bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                   static_cast<std::uint32_t>(bound));
}

[[nodiscard]] const RibbonFamily& FamilyOf(const EffectGroundRibbonPattern& pattern) {
  switch (pattern.builder) {
    case EffectGroundRibbonBuilder::kInitTextured:
      return kMidFamily;
    case EffectGroundRibbonBuilder::kBuildLineBeams:
      if (pattern.builder_argument >= kSecondDecade) {
        return kHighFamily;
      }
      return pattern.builder_argument >= kDecade ? kMidFamily : kLowFamily;
    case EffectGroundRibbonBuilder::kInitLineBeam:
    case EffectGroundRibbonBuilder::kInitLineBeamYellow:
      break;
  }
  return kLowFamily;
}

}  // namespace

std::span<const EffectGroundRibbonPattern> EffectGroundRibbonPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectGroundRibbonPattern> table = EffectGroundRibbonPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectGroundRibbonPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectGroundRibbonPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::string_view EffectGroundRibbonTextureFor(const EffectGroundRibbonPattern& pattern) {
  if (!pattern.texture.empty()) {
    return pattern.texture;
  }
  // ⚠ LE PREMIER BÂTISSEUR ET LE TROISIÈME CHOISISSENT PAR LA MÊME RÈGLE : un
  // argument de dix ou plus prend le jaune, en deçà le blanc.
  if (pattern.builder == EffectGroundRibbonBuilder::kBuildLineBeams ||
      pattern.builder == EffectGroundRibbonBuilder::kInitLineBeam) {
    return pattern.builder_argument >= kDecade ? kYellowBeam : kPlainBeam;
  }
  return kYellowBeam;
}

std::uint32_t EffectGroundRibbonRenderWord(const EffectGroundRibbonPattern& pattern) {
  // ⚠ UN SEUL DES QUATRE GARDE LE MÉLANGE DE CHAQUE PARTICULE ET L'ÉPREUVE DE
  // PROFONDEUR ; les trois autres peignent PAR-DESSUS TOUT, sous le mélange
  // ordinaire que leur seau impose.
  return pattern.builder == EffectGroundRibbonBuilder::kInitLineBeam
             ? kEffectRenderWordOfConstructor | kEffectRenderWordOwnBlend
             : kEffectRenderWordOfConstructor | kEffectRenderWordOverEverything;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGroundRibbonTracks(
    const EffectGroundRibbonPattern& pattern, std::int16_t target_degrees,
    std::uint32_t& generator) {
  const RibbonFamily& family = FamilyOf(pattern);
  const std::uint8_t base_mark = pattern.builder == EffectGroundRibbonBuilder::kInitLineBeam
                                     ? kAddedStyleBase
                                     : family.mark_base;
  // Le reste par dix décide du sens : d'un côté ou de l'autre du cap.
  const std::int16_t sens = pattern.builder_argument % kDecade;

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.ticks = 0;
    track.alpha = kStartsInvisible;
    track.radius_at_anchor = kMeasureAtAnchor;
    // ⚠ L'ORDRE DES TIRAGES EST CELUI DU CLIENT, et il compte : deux rubans semés
    // à la suite doivent différer comme chez lui, pas autrement.
    if (sens == 0) {
      track.value[kSlotOfRibbonSideways] =
          -kSidewaysBase - static_cast<float>(RollBelow(generator, kSidewaysSpread));
    } else if (sens == 1) {
      track.value[kSlotOfRibbonSideways] =
          kSidewaysBase + static_cast<float>(RollBelow(generator, kSidewaysSpread));
    }
    // ⚠ UN AUTRE RESTE NE POSE RIEN, et la case garde son défaut : le ruban n'est
    // alors décalé d'aucun côté. Aucun des douze sites n'y tombe.
    track.value[kSlotOfRibbonNearEnd] = static_cast<float>(RollBelow(generator, kNearEndSpread));
    // ⚠ LE BÂTISSEUR À TRENTE NE TIRE RIEN DE PLUS pour un argument de dix ou
    // plus : la clé, l'étendue, la marque et le pas gardent alors leur zéro.
    const bool no_family = pattern.builder == EffectGroundRibbonBuilder::kInitLineBeam &&
                           pattern.builder_argument >= kDecade;
    if (!no_family) {
      track.lift_key =
          static_cast<float>(RollBelow(generator, family.key_spread)) * 0.1F + family.key_base;
      track.value[kSlotOfRibbonSpan] =
          static_cast<float>(RollBelow(generator, family.span_spread)) + family.span_base;
      track.mark[kMarkOfRibbonStyle] =
          static_cast<std::uint8_t>(RollBelow(generator, kMarkSpread) + base_mark);
      track.value[kSlotOfRibbonStep] =
          static_cast<float>(RollBelow(generator, family.step_spread)) * 0.1F + family.step_base;
    }
    // 🔴 ET LE BÂTISSEUR TEXTURÉ TIRE UNE CINQUIÈME FOIS pour un argument de la
    // troisième ou de la quatrième dizaine : la marque est RETIRÉE dans la
    // moitié haute des styles, ceux qui s'ajoutent — trente-trois à trente-cinq,
    // puis trente-six à trente-huit.
    if (pattern.builder == EffectGroundRibbonBuilder::kInitTextured) {
      const std::int32_t dizaine = pattern.builder_argument / kDecade;
      if (dizaine == kThirdDecade || dizaine == kFourthDecade) {
        track.mark[kMarkOfRibbonStyle] = static_cast<std::uint8_t>(
            RollBelow(generator, kMarkSpread) +
            (dizaine == kThirdDecade ? kRedrawnMarkOfThirdDecade : kRedrawnMarkOfFourthDecade));
      }
    }
    // ⚠ LE CAP VIENT DE LA CIBLE, pas d'un tirage.
    track.tilt_degrees = target_degrees;
  }
  return tracks;
}

std::int16_t EffectGroundRibbonTargetDegrees(glm::vec3 from, glm::vec3 to) {
  // LA PROFONDEUR SE RETOURNE en passant chez le client ; l'abscisse, non.
  return static_cast<std::int16_t>(HeadingOfVector(to.x - from.x, -(to.z - from.z)));
}

PooledEffectNode SowEffectGroundRibbonNode(const EffectGroundRibbonPattern& pattern,
                                           glm::vec3 anchor, std::int16_t target_degrees,
                                           std::int32_t effect_life_ticks,
                                           std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kGroundRibbon;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.node.render_word = EffectGroundRibbonRenderWord(pattern);
  pooled.resource_paths = {std::string(EffectGroundRibbonTextureFor(pattern))};
  pooled.tracks = SeedEffectGroundRibbonTracks(pattern, target_degrees, generator);
  return pooled;
}

}  // namespace ro::render
