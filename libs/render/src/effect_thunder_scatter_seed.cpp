#include "ro/render/effect_thunder_scatter_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_thunder_scatter.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES TROIS PREMIÈRES MARQUES QUE LE BÂTISSEUR ÉCRIT.
//
// ⚠ LA DEUXIÈME N'EST LUE PAR PERSONNE : le carré teinté lit la première piste
// par piste et la troisième et la quatrième sur la première piste seule, et le
// mouvement ne touche aucune marque. Posée à zéro, comme le client la pose.
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kUnreadMark = 1;
constexpr std::size_t kMarkOfTint = 2;

// L'ANGLE PROPRE, posé à zéro, que le carré teinté lit pour faire
// respirer son rayon : à zéro et jamais touché, le rayon ne respire pas.
constexpr std::size_t kSlotOfSpinDegrees = 8;
// ⚠ LA CASE SEPT, posée à zéro, N'EST LUE PAR PERSONNE : ni le mouvement, ni le
// carré teinté, dont les lectures par piste s'arrêtent aux cases huit et neuf.
constexpr std::size_t kUnreadSlot = 7;

[[nodiscard]] std::int32_t DrawBelow(std::int32_t exclusive_bound, std::uint32_t& generator) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

}  // namespace

std::span<const EffectThunderScatterPattern> EffectThunderScatterPatternsFor(
    std::uint32_t effect_id) {
  const std::span<const EffectThunderScatterPattern> table = EffectThunderScatterPatternTable();
  const auto [first, last] =
      std::ranges::equal_range(table, effect_id, {}, &EffectThunderScatterPattern::effect_id);
  return {first, last};
}

std::array<EffectTrack, kEffectTracksPerNode> SeedThunderScatterTracks(glm::vec3 aim_point,
                                                                       std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.ticks = DrawBelow(kThunderScatterDelayDraws, generator) + kThunderScatterDelayBase;
    track.radius_at_anchor = static_cast<float>(DrawBelow(kThunderScatterRadiusDraws, generator)) *
                                 kThunderScatterRadiusStep +
                             kThunderScatterRadiusBase;
    track.value[kSlotOfSpinDegrees] = 0.0F;
    track.alpha = 0;
    // L'ANCRE : le point visé, huit unités plus haut.
    track.anchor = {aim_point.x, aim_point.y + kThunderScatterLiftAboveAim, aim_point.z};
    track.azimuth_degrees = 0;
    track.value[kThunderScatterSlotOfSpeed] =
        static_cast<float>(DrawBelow(kThunderScatterSpeedDraws, generator)) *
            kThunderScatterDrawStep +
        kThunderScatterSpeedBase;
    track.value[kThunderScatterSlotOfRise] =
        static_cast<float>(DrawBelow(kThunderScatterRiseDraws, generator)) *
            kThunderScatterDrawStep +
        kThunderScatterRiseBase;
    track.degrees = 0;
    track.mark[kMarkOfTexture] = 0;
    track.mark[kUnreadMark] = 0;
    // ⚠ POSÉE SUR CHAQUE PISTE, LUE SUR LA PREMIÈRE SEULE : le carré teinté la
    // prend à un déplacement que sa boucle ne décale pas.
    track.mark[kMarkOfTint] = kThunderScatterTint;
    track.value[kUnreadSlot] = 0.0F;
  }
  return tracks;
}

std::optional<PooledEffectNode> SowThunderScatterNode(glm::vec3 owner_position,
                                                      const std::optional<glm::vec3>& aim_point,
                                                      std::int32_t effect_life_ticks,
                                                      std::uint32_t& generator) {
  // ⚠ SANS POINT VISÉ, RIEN. Voir l'en-tête : le client poserait ses ancres
  // près de l'origine de la carte, et deviner un autre point montrerait ce
  // qu'il ne montre pas.
  if (!aim_point.has_value()) {
    return std::nullopt;
  }
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kThunderScatter;
  // ⚠ LE NŒUD NAÎT SUR L'INSTANCE, sans écart, et prend sa place : celle du
  // porteur.
  pooled.ground_position = owner_position;
  pooled.owner_position = owner_position;
  // LE POINT VISÉ, recopié dans le nœud.
  // Le mouvement n'en lit que l'abscisse et la cote.
  pooled.target_position = aim_point;
  pooled.node.life_ticks = std::max(effect_life_ticks, 1);
  pooled.node.render_word = kThunderScatterRenderWord;
  // L'IMAGE UNIQUE, en première case du tableau d'images.
  // ⚠ L'échelle de sprite, posée à zéro, n'est lue ni par le mouvement
  // ni par le carré teinté : elle n'a pas de champ ici.
  pooled.resource_paths = {std::string(kThunderScatterTexture)};
  pooled.tracks = SeedThunderScatterTracks(*aim_point, generator);
  return pooled;
}

}  // namespace ro::render
