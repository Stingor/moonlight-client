#include "ro/render/effect_drifting_icon_seed.h"

#include <algorithm>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_drifting_icon.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"

namespace ro::render {
namespace {

// LES CASES QUE LE BÂTISSEUR ÉCRIT, et ce qu'elles deviennent.
//
// Les quatre angles qui courent : deux couples (valeur, cible), tous tirés dans
// le tour.
constexpr std::size_t kChasingSlots = kDriftingIconChasingPairs * kDriftingIconSlotsPerPair;

// ⚠ LA CASE SEPT (3.0) N'EST LUE PAR PERSONNE : ni par le mouvement, ni par le
// carré teinté. Elle est posée parce que le bâtisseur la pose.
constexpr std::size_t kUnreadSlot = 7;
constexpr float kUnreadSlotValue = 3.0F;

// La marque d'image (zéro) et celle de teinte.
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfTint = 2;

}  // namespace

std::span<const EffectDriftingIconPattern> EffectDriftingIconPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectDriftingIconPatternTable(), effect_id,
                           &EffectDriftingIconPattern::effect_id);
}

std::size_t EffectDriftingIconTextureCount(const EffectDriftingIconPattern& pattern) {
  const auto first_empty =
      std::ranges::find_if(pattern.textures, [](std::string_view image) { return image.empty(); });
  return static_cast<std::size_t>(first_empty - pattern.textures.begin());
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectDriftingIconTracks(
    glm::vec3 player_position, std::int32_t life_ticks, const EffectScatterWorld& world,
    std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.ticks = 0;
    track.value[kUnreadSlot] = kUnreadSlotValue;
    track.degrees = static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    track.alpha = 0;
    track.mark[kMarkOfTexture] = 0;
    // LE SEUIL DE LA PÂLEUR, rangé dans le cap : `(WORD) vie − 50`.
    track.azimuth_degrees = static_cast<std::int16_t>(life_ticks - kDriftingIconFadeTicks);

    const float x =
        player_position.x + static_cast<float>(DrawBelow(generator, kDriftingIconScatterSpan) -
                                               kDriftingIconScatterHalf);
    // ⚠ LE TIRAGE DE LA PROFONDEUR SE RETRANCHE, pour la même raison que sa
    // dérive : le client l'ajoute dans un repère dont la profondeur est opposée.
    const float z =
        player_position.z - static_cast<float>(DrawBelow(generator, kDriftingIconScatterSpan) -
                                               kDriftingIconScatterHalf);
    // ⚠ LE TIRAGE DE LA HAUTEUR PRÉCÈDE LA LECTURE DU SOL ; l'ordre ne change
    // rien au sol, il garde la suite des tirages.
    const auto surcroit = static_cast<float>(DrawBelow(generator, kDriftingIconLiftSpread));
    const float sol = world.ground_height ? world.ground_height(x, z) : player_position.y;
    track.anchor = {x, sol + kDriftingIconLiftAboveGround + surcroit, z};

    for (std::size_t case_courante = 0; case_courante < kChasingSlots; ++case_courante) {
      track.value[case_courante] =
          static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    }
    track.radius_at_anchor = static_cast<float>(DrawBelow(generator, kDriftingIconRadiusDraws)) *
                                 kDriftingIconRadiusStep +
                             kDriftingIconRadiusBase;
    track.value[kDriftingIconSlotOfSpinDegrees] =
        static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    // ⚠ POSÉE SUR CHAQUE PISTE, LUE SUR LA PREMIÈRE SEULE : le carré teinté la
    // prend à un déplacement que sa boucle ne décale pas.
    track.mark[kMarkOfTint] = kDriftingIconTint;
  }
  return tracks;
}

PooledEffectNode SowEffectDriftingIconNode(const EffectDriftingIconPattern& pattern,
                                           glm::vec3 player_position, std::int32_t life_ticks,
                                           const EffectScatterWorld& world,
                                           std::uint32_t& generator) {
  // ⚠ LA PLACE DU NŒUD N'ENTRE DANS AUCUN COIN : les ancres sont déjà dans le
  // monde. Le mouvement du client y recopie pourtant le porteur à chaque tic,
  // une écriture que ni lui ni la figure ne relisent.
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kDriftingIcons, player_position);
  pooled.node.life_ticks = std::max(life_ticks, 1);
  pooled.node.render_word = kDriftingIconRenderWord;
  const std::size_t choix = EffectDriftingIconTextureCount(pattern);
  const std::size_t image =
      choix > 1 ? static_cast<std::size_t>(DrawBelow(generator, static_cast<std::int32_t>(choix)))
                : 0;
  if (choix > 0) {
    pooled.resource_paths = {std::string(pattern.textures[image])};
  }
  pooled.tracks = SeedEffectDriftingIconTracks(player_position, life_ticks, world, generator);
  // ⚠ LE MOUVEMENT TIRE À CHAQUE CIBLE ATTEINTE, et chaque nœud emporte SA
  // graine, prise dans l'état courant du générateur : quatre-vingts nœuds semés
  // d'une graine commune choisiraient leurs cibles en chœur.
  pooled.node.wander_seed = generator;
  return pooled;
}

}  // namespace ro::render
