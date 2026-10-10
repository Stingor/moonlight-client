#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_grand_cross.h"
#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE GRAND CROSS : deux nœuds pour un identifiant.
//
// Deux bâtisseurs — les anneaux de Grand Cross, puis les anneaux jaune et
// noir —, que le cas du répartiteur appelle l'un après l'autre avec le même
// octet.
//
// ⚠ CET OCTET CHANGE LES TROIS IMAGES À LA FOIS, et c'est tout ce qui sépare
// 226 de 450 : blanc et jaune d'un côté, noir des deux côtés de l'autre. La
// ressource appartient au SITE (ADR 0019), et non au bâtisseur.

// CE QUE LE BÂTISSEUR DU RUBAN SÈME, et qui ne dépend d'aucun site.
inline constexpr std::int16_t kGrandCrossRibbonSweepDegrees = 90;
inline constexpr float kGrandCrossRibbonLiftKey = 120.0F;
inline constexpr float kGrandCrossRibbonRadius = 19.9F;
inline constexpr std::int16_t kGrandCrossRibbonTilt = 90;
// LES QUATRE AZIMUTS, dans l'ordre des pistes. Ils ne montent pas : le client
// écrit cent quatre-vingts, quatre-vingt-dix, zéro puis deux cent soixante-dix.
inline constexpr std::array<std::int16_t, kEffectTracksPerNode> kGrandCrossRibbonAzimuths = {
    180, 90, 0, 270};
// LES QUATRE DÉCALAGES, un par bras, en octets SIGNÉS. C'est eux qui font la
// croix : sans eux les quatre rubans se superposeraient sur le même cercle.
inline constexpr std::int8_t kGrandCrossArmOffset = 23;
inline constexpr std::array<std::array<std::int8_t, 2>, kEffectTracksPerNode>
    kGrandCrossRibbonArmOffsets = {{{kGrandCrossArmOffset, kGrandCrossArmOffset},
                                    {kGrandCrossArmOffset, -kGrandCrossArmOffset},
                                    {-kGrandCrossArmOffset, -kGrandCrossArmOffset},
                                    {-kGrandCrossArmOffset, kGrandCrossArmOffset}}};

// CE QUE LE BÂTISSEUR DU CADRE SÈME.
//
// ⚠ IL N'ALLUME QUE DEUX PISTES SUR QUATRE, et il couche les deux autres
// explicitement. En allumer quatre doublerait les douze panneaux du cadre.
inline constexpr std::size_t kGrandCrossFrameLivingTracks = 2;
inline constexpr float kGrandCrossFrameLiftKey = 60.0F;
inline constexpr float kGrandCrossFrameHalfWidth = 24.0F;
inline constexpr float kGrandCrossFrameHalfDepth = 3.2F;
inline constexpr std::int16_t kGrandCrossFrameAzimuthStep = 90;

// LE MOT DE RENDU DES DEUX NŒUDS : translucide, au mélange PROPRE à chaque
// panneau. Les deux bâtisseurs le posent en trois temps — zéro, puis un, puis
// cinq — et c'est le dernier qui reste. Voir `effect_render_word.h`.
inline constexpr std::uint32_t kGrandCrossRenderWord =
    kEffectRenderWordTranslucent | kEffectRenderWordOwnBlend;

// CE QU'UN IDENTIFIANT DE GRAND CROSS PORTE.
struct EffectGrandCrossPattern {
  std::uint32_t effect_id = 0;
  std::string_view ribbon_texture;
  std::string_view frame_texture;
  // L'échelle que le bâtisseur du ruban pose dans le nœud : un pour le blanc,
  // douze pour le noir. Le cadre garde un dans les deux cas.
  float ribbon_node_scale = 1.0F;
};

[[nodiscard]] std::span<const EffectGrandCrossPattern> EffectGrandCrossPatternTable();
[[nodiscard]] std::span<const EffectGrandCrossPattern> EffectGrandCrossPatternsFor(
    std::uint32_t effect_id);

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedGrandCrossRibbonTracks();
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedGrandCrossFrameTracks();

[[nodiscard]] PooledEffectNode SowGrandCrossRibbonNode(const EffectGrandCrossPattern& pattern,
                                                       glm::vec3 anchor,
                                                       std::int32_t effect_life_ticks);
[[nodiscard]] PooledEffectNode SowGrandCrossFrameNode(const EffectGrandCrossPattern& pattern,
                                                      glm::vec3 anchor,
                                                      std::int32_t effect_life_ticks);

}  // namespace ro::render
