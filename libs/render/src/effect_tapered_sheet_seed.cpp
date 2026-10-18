#include "ro/render/effect_tapered_sheet_seed.h"

#include <algorithm>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_tapered_sheet.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectTaperedSheetPattern> EffectTaperedSheetPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectTaperedSheetPatternTable(), effect_id,
                           &EffectTaperedSheetPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedTaperedSheetTracks(glm::vec3 owner_position,
                                                                     std::uint8_t octet) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = -kTaperedSheetDelayPerTrack * static_cast<std::int32_t>(rang);
    track.radius_at_anchor = kTaperedSheetRadius;
    track.mark[kTaperedSheetMarkOfUnreadTint] = kTaperedSheetUnreadTint;
    // ⚠ LE CLIENT RETRANCHE L'ÉTAGE À SON ORDONNÉE, QUI DESCEND : chez nous, il
    // s'ajoute.
    track.tail[kTaperedSheetSlotOfHeight] = owner_position.y + kTaperedSheetLifts[rang];
    track.degrees = kTaperedSheetHeadingDegrees;
    track.value[kTaperedSheetSlotOfUnread] = 0.0F;
    // ⚠ OPAQUE DÈS LE SEMIS : la piste qui attend se peint aussi, mais sans aire
    // — son bord intérieur est encore sur l'extérieur.
    track.alpha = kEffectTrackAlphaMax;
    // ⚠ LA HAUTEUR DE L'ANCRE N'EST PAS ÉCRITE PAR LE BÂTISSEUR : le premier tic
    // utile la pose. Nous la posons déjà, ce qui ne change rien à une feuille
    // sans aire.
    track.anchor = {owner_position.x, track.tail[kTaperedSheetSlotOfHeight], owner_position.z};
    track.azimuth_degrees = 0;
    track.mark[kTaperedSheetMarkOfImage] = 0;
    track.mark[kTaperedSheetMarkOfSiteOctet] = octet;
  }
  return tracks;
}

PooledEffectNode SowTaperedSheetNode(const EffectTaperedSheetPattern& pattern,
                                     glm::vec3 owner_position, std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kTaperedSheets, owner_position);
  pooled.node.life_ticks = std::max(effect_life_ticks, 1);
  pooled.node.render_word = kTaperedSheetRenderWord;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedTaperedSheetTracks(owner_position, pattern.octet);
  return pooled;
}

}  // namespace ro::render
