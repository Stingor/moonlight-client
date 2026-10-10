#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA ROUE DE VENT, sous-type 126.
//
// UN SEUL bâtisseur et UN SEUL site : l'effet 624, Kamaitachi.
//
// ⚠ CINQ NŒUDS, UN PAR TIC : le bâtisseur sème tant que l'âge de l'instance
// est sous cinq, et chacun naît à une branche de la CROIX — au
// centre, puis cinq unités à l'est, à l'ouest, et de part et d'autre en
// profondeur. Les cinq roues roulent donc côte à côte, légèrement décalées.

// COMBIEN DE TICS la roue sème, et donc combien de nœuds.
inline constexpr std::int32_t kRollingGustSowingTicks = 5;

// DE COMBIEN chaque branche de la croix s'écarte du centre.
inline constexpr float kRollingGustCrossStep = 5.0F;

// COMBIEN DE TICS la roue se fait attendre : le compteur de la piste zéro naît
// à moins trois, comme celui de la bourrasque du 119.
inline constexpr std::int32_t kRollingGustDelayTicks = 3;

// Son rayon et sa vitesse de départ.
inline constexpr float kRollingGustRadius = 20.0F;
inline constexpr float kRollingGustStartSpeed = 1.5F;

// DE COMBIEN elle flotte au-dessus du nœud, et de combien le tirage la
// redescend — de zéro à quatre.
inline constexpr float kRollingGustLiftAboveNode = 10.0F;
inline constexpr std::int32_t kRollingGustLiftSpread = 5;

// UNE ROUE qu'un effet lance.
struct EffectRollingGustPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
};

[[nodiscard]] std::span<const EffectRollingGustPattern> EffectRollingGustPatternTable();

[[nodiscard]] std::span<const EffectRollingGustPattern> EffectRollingGustPatternsFor(
    std::uint32_t effect_id);

// OÙ LE NŒUD DU TIC `sow_tick` NAÎT par rapport au centre : la croix du
// bâtisseur.
//
// ⚠ LA PROFONDEUR EST RETOURNÉE : le client ajoute cinq à son z au troisième
// tic ; chez nous, c'est MOINS cinq.
[[nodiscard]] glm::vec3 RollingGustCrossOffset(std::int32_t sow_tick);

// LES QUATRE PISTES du nœud semé au tic `sow_tick` de l'effet.
//
// `toward_target` va du nœud vers la cible du sort :
// le client en tire le cap au premier tic utile, et comme ni l'un ni l'autre ne
// change d'ici là, le poser au semis revient au même — c'est le choix fait pour
// le 119.
//
// ⚠ LE NŒUD EST SEMÉ TOUT DE SUITE, MAIS RETARDÉ DE `sow_tick` : son compteur
// naît plus bas d'autant. Le mouvement ne fait rien d'autre que compter tant
// qu'il est négatif, et l'alpha est nul — un nœud retardé et un nœud semé plus
// tard ne se distinguent pas.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRollingGustTracks(
    glm::vec3 anchor, glm::vec3 toward_target, std::int32_t sow_tick, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, au tic donné. Sa vie est allongée du même retard,
// si bien qu'il meurt au même instant que chez le client.
[[nodiscard]] PooledEffectNode SowEffectRollingGustNode(const EffectRollingGustPattern& pattern,
                                                        glm::vec3 anchor, glm::vec3 toward_target,
                                                        std::int32_t sow_tick,
                                                        std::int32_t effect_life_ticks,
                                                        std::uint32_t& generator);

}  // namespace ro::render
