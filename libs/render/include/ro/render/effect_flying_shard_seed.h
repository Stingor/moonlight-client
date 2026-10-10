#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'ÉCLAT QUI FUIT, sous-type 44.
//
// DEUX bâtisseurs jumeaux, et ils écrivent les MÊMES champs dans le même
// ordre. Ils ne diffèrent que par QUATRE nombres : le retard, la vitesse
// d'éloignement, la demi-largeur et l'élancement.
//
// ⚠ LES DEUX DÉPLACENT LE NŒUD DE NEUF UNITÉS VERS LE HAUT, et ils le font en
// MUTANT la position de l'instance, pas celle du nœud. C'est à hauteur de
// poitrine que les éclats partent ; les poser au sol les enterrerait à moitié.

// DE COMBIEN LE NŒUD MONTE au-dessus du point de l'effet.
inline constexpr float kFlyingShardBirthHeight = 9.0F;

// LE RAYON DE DÉPART, et il est NÉGATIF.
//
// ⚠ MOINS DIX : l'éclat commence de l'AUTRE CÔTÉ du centre et le traverse. Le
// semer à zéro lui ferait manquer la moitié de sa course, celle qui passe par
// le corps.
inline constexpr float kFlyingShardStartRadius = -10.0F;

// LEQUEL DES DEUX BÂTISSEURS un site appelle.
enum class EffectFlyingShardBuilder : std::uint8_t {
  // Le premier : il part plus tard, va plus vite, et son barreau est fin et
  // long.
  kSlender,
  // Le second : il part plus tôt, va moins vite, et son barreau est large et
  // court.
  kStout,
};

// CE QUE CHACUN TIRE ET POSE. Quatre nombres, et aucun ne se déduit d'un autre.
inline constexpr std::int32_t kFlyingShardSlenderDelayFloor = 10;
inline constexpr std::int32_t kFlyingShardSlenderDelaySpread = 25;
inline constexpr std::int32_t kFlyingShardStoutDelayFloor = 20;
inline constexpr std::int32_t kFlyingShardStoutDelaySpread = 10;
inline constexpr float kFlyingShardSlenderGrowth = 3.0F;
inline constexpr float kFlyingShardStoutGrowth = 2.0F;
inline constexpr float kFlyingShardSlenderHalfWidth = 0.7F;
inline constexpr float kFlyingShardStoutHalfWidth = 1.0F;
inline constexpr std::uint8_t kFlyingShardSlenderLength = 24;
inline constexpr std::uint8_t kFlyingShardStoutLength = 16;

// UN ÉCLAT qu'un effet pose.
struct EffectFlyingShardPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  EffectFlyingShardBuilder builder = EffectFlyingShardBuilder::kSlender;
  // L'OCTET DU SITE : il ne choisit QUE la teinte et le mélange.
  std::uint8_t mark = 0;
  // 🔴 LE SITE APPELLE SON BÂTISSEUR EN BOUCLE, au tic zéro : vingt nœuds de
  // quatre éclats — vingt-quatre pour EF_TEIHIT1X —, et non un seul. En semer
  // un ne montrait que quatre éclats sur quatre-vingts.
  std::uint8_t nodes_per_spawn = 1;
  // Et sous les effets simplifiés : EF_TEIHIT1T tombe à six, les trois autres
  // gardent leur compte.
  std::uint8_t nodes_when_minimal = 1;
};

[[nodiscard]] std::span<const EffectFlyingShardPattern> EffectFlyingShardPatternTable();

[[nodiscard]] std::span<const EffectFlyingShardPattern> EffectFlyingShardPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes vives et toutes tirées séparément.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFlyingShardTracks(
    const EffectFlyingShardPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectFlyingShardNode(const EffectFlyingShardPattern& pattern,
                                                        glm::vec3 anchor,
                                                        std::int32_t effect_life_ticks,
                                                        std::uint32_t& generator);

}  // namespace ro::render
