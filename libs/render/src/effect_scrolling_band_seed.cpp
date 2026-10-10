#include "ro/render/effect_scrolling_band_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// COMBIEN DE JOINTURES le bâtisseur pose, et de combien il les espace.
//
// Vingt, alors que la figure n'en lit que six : le client remplit sa boucle
// interne deux fois de dix cases. Les poser toutes coûte trois lignes et garde
// la piste telle que le client la laisse — en poser six ferait dépendre le semis
// de ce que la figure d'aujourd'hui consomme.
constexpr std::size_t kSownJoints = 20;
constexpr float kJointSpacing = kScrollingBandPhaseSpan;

// LES DEUX ALLURES que le bâtisseur de construction propose, en tics par cycle,
// et la seule que le bâtisseur d'initialisation connaisse — qui est la
// première.
//
// Chaque piste est plus rapide que la précédente, du pas donné. C'est ce
// décalage qui fait l'eau : quatre rideaux à la même allure glisseraient
// ensemble et ne montreraient qu'un plan.
constexpr std::int32_t kSlowCycleFirstTicks = 80;
constexpr std::int32_t kSlowCycleStepTicks = 13;
constexpr std::int32_t kFastCycleFirstTicks = 30;
constexpr std::int32_t kFastCycleStepTicks = 6;

// LES DEUX LARGEURS que le bâtisseur d'initialisation propose, en unités du
// monde, et la seule que le bâtisseur de construction emploie — qui est la
// large.
//
// Chaque piste est d'une unité plus large que la précédente : les quatre
// rideaux ne se recouvrent donc jamais tout à fait sur les bords.
constexpr float kWideBandFirst = 36.0F;
constexpr float kNarrowBandFirst = 18.0F;
constexpr float kBandWidthStep = 1.0F;

// L'ÉCHELONNEMENT EN PROFONDEUR des quatre rideaux, et il diffère d'un bâtisseur
// à l'autre : l'un les écarte d'une unité, l'autre d'un quart.
constexpr float kDeepStepInit = 1.0F;
constexpr float kDeepStepBuild = 0.25F;
constexpr float kDeepFirst = -1.0F;

// LES ALPHAS que les deux bâtisseurs posent, en entier comme le client.
constexpr std::int16_t kInitAlpha = 80;
constexpr std::int16_t kBuildAlpha = 120;
constexpr std::int16_t kFlareAlpha = 70;

// LES VALEURS DU SECOND OCTET, telles que les treize sites les poussent.
constexpr std::uint8_t kArgumentFirst = 0;
constexpr std::uint8_t kArgumentSecond = 1;
constexpr std::uint8_t kArgumentFlare = 10;

// LES MARQUES DE STYLE que chaque bâtisseur écrit. Voir
// `EffectScrollingBandStyleFor` pour ce que la figure en fait.
constexpr std::uint8_t kStyleOrdinary = 0;
constexpr std::uint8_t kStyleDeepWater = 1;
constexpr std::uint8_t kStyleFlare = 10;

// LE TASSEMENT QUE LE BÂTISSEUR DE CONSTRUCTION POSE, et il ne tasse rien : la figure
// ne tasse qu'au-DESSUS de dix. Le poser quand même garde la piste telle que le
// client la laisse.
constexpr std::uint8_t kBuildSqueeze = 10;

// ⚠ ET CELUI DU BÂTISSEUR D'INITIALISATION NE TASSE JAMAIS NON PLUS. Il le tire
// d'un champ de l'instance d'effet que nous ne portons pas, et qui ne vaut que
// zéro, un, ou dix — jamais plus. Zéro est donc la seule valeur qu'on puisse
// écrire sans supposer, et elle donne ce que les trois donnent.
constexpr std::uint8_t kInitSqueeze = 0;

}  // namespace

std::span<const EffectScrollingBandPattern> EffectScrollingBandPatternsFor(
    std::uint32_t effect_id) {
  const std::span<const EffectScrollingBandPattern> table = EffectScrollingBandPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectScrollingBandPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectScrollingBandPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectScrollingBandTracks(
    const EffectScrollingBandPattern& pattern) {
  const bool init = pattern.builder == EffectScrollingBandBuilder::kInitWaterfall;
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    const auto rang_flottant = static_cast<float>(rang);
    const auto rank_int = static_cast<std::int32_t>(rang);

    track.active = true;
    track.ticks = 0;
    track.mark[kMarkOfBandPlane] = pattern.plane;
    for (std::size_t jointure = 0; jointure < kSownJoints; ++jointure) {
      track.value[jointure] = static_cast<float>(jointure) * kJointSpacing;
    }

    if (init) {
      track.alpha = kInitAlpha;
      track.mark[kMarkOfBandStyle] = kStyleOrdinary;
      track.mark[kMarkOfBandSqueeze] = kInitSqueeze;
      track.lift_key = static_cast<float>(kSlowCycleFirstTicks - kSlowCycleStepTicks * rank_int);
      track.anchor.z = kDeepFirst + rang_flottant * kDeepStepInit;
      // ⚠ UNE TROISIÈME VALEUR DU SECOND OCTET NE POSE AUCUNE LARGEUR, et la
      // piste garde alors zéro : la bande n'a plus d'aire. Aucun des treize
      // sites n'y tombe, mais inventer une largeur cacherait le jour où l'un
      // d'eux y tomberait.
      if (pattern.builder_argument == kArgumentFirst) {
        track.radius_at_anchor = kWideBandFirst + rang_flottant * kBandWidthStep;
      } else if (pattern.builder_argument == kArgumentSecond) {
        track.radius_at_anchor = kNarrowBandFirst + rang_flottant * kBandWidthStep;
      }
      continue;
    }

    track.mark[kMarkOfBandSqueeze] = kBuildSqueeze;
    track.radius_at_anchor = kWideBandFirst + rang_flottant * kBandWidthStep;
    track.anchor.z = kDeepFirst + rang_flottant * kDeepStepBuild;
    if (pattern.builder_argument == kArgumentFirst) {
      track.alpha = kBuildAlpha;
      track.mark[kMarkOfBandStyle] = kStyleDeepWater;
      track.lift_key = static_cast<float>(kSlowCycleFirstTicks - kSlowCycleStepTicks * rank_int);
    } else if (pattern.builder_argument == kArgumentSecond) {
      track.alpha = kBuildAlpha;
      track.mark[kMarkOfBandStyle] = kStyleDeepWater;
      track.lift_key = static_cast<float>(kFastCycleFirstTicks - kFastCycleStepTicks * rank_int);
    } else if (pattern.builder_argument == kArgumentFlare) {
      // ⚠ LA SEULE BRANCHE QUI FIGE LA BANDE : cycle nul, un seul quad. Et c'est
      // aussi la seule dont le style renverse la phase, ce qui la ramène à zéro
      // et rend au quad toute son aire.
      track.alpha = kFlareAlpha;
      track.mark[kMarkOfBandStyle] = kStyleFlare;
      track.lift_key = 0.0F;
    } else {
      track.alpha = kBuildAlpha;
      track.mark[kMarkOfBandStyle] = kStyleDeepWater;
    }
  }
  return tracks;
}

PooledEffectNode SowEffectScrollingBandNode(const EffectScrollingBandPattern& pattern,
                                            glm::vec3 anchor, std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kScrollingBand;
  pooled.ground_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths.reserve(pattern.textures.size());
  for (const std::string_view image : pattern.textures) {
    pooled.resource_paths.emplace_back(image);
  }
  pooled.tracks = SeedEffectScrollingBandTracks(pattern);
  return pooled;
}

}  // namespace ro::render
