#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU CADRE ET DE LA BARRE, sous-type 90.
//
// UN SEUL bâtisseur et HUIT
// sites d'appel pour cinq identifiants : chaque effet l'appelle DEUX FOIS, à
// deux octets de style consécutifs, et pose donc deux nœuds emboîtés.
//
// ⚠ CE SEMIS NE TIRE RIEN. C'est le premier du lot dans ce cas : tout vient de
// l'octet du site, et deux lancements du même effet donnent exactement la même
// figure. Lui passer un générateur laisserait croire à un hasard qui n'existe
// pas.
//
// ⚠ ET L'OCTET EST UNE TABLE À LUI SEUL. Il choisit la hauteur des murs,
// l'alpha du cadre, sa demi-taille, celle de la barre, le départ de la barre,
// sa longueur et la durée de l'ensemble — sept nombres. Le lire comme un simple
// numéro de teinte en perdrait six.

// DE COMBIEN LE SECOND NŒUD MONTE au-dessus du premier, et de combien sa
// demi-taille grandit : le rang de la piste entre dans les deux.
inline constexpr float kFrameAndBarRankHeightStep = 1.0F;
inline constexpr float kFrameAndBarRankSideStep = 0.4F;

// LES DEUX ÉTAGES DE HAUTEUR, et les deux alphas de cadre qui les accompagnent.
inline constexpr float kFrameAndBarTallLift = 30.0F;
inline constexpr float kFrameAndBarShortLift = 20.0F;
inline constexpr std::int16_t kFrameAndBarTallAlpha = 65;
inline constexpr std::int16_t kFrameAndBarShortAlpha = 32;
// L'alpha de la barre ne dépend de rien : quinze pour les cinq octets.
inline constexpr std::int16_t kFrameAndBarBarAlpha = 15;

// LES QUATRE DURÉES ÉCRITES EN DUR, en tics — plus de cent secondes chacune.
inline constexpr std::int16_t kFrameAndBarLongDuration = 2415;
inline constexpr std::int16_t kFrameAndBarPlainDuration = 2385;
inline constexpr std::int16_t kFrameAndBarWatchedDuration = 2170;
inline constexpr std::int16_t kFrameAndBarShortDuration = 2140;

// CE QUE LA DURÉE DEVIENT quand l'octet est dans la dizaine : la vie de
// l'effet moins quatre-vingt-dix, et le client la borne avant d'y toucher.
inline constexpr std::int32_t kFrameAndBarLifeMargin = 90;
inline constexpr std::int32_t kFrameAndBarLifeCeiling = 30000;

// CE QU'UN OCTET DE STYLE POSE. Les branches du bâtisseur, mises à plat.
struct EffectFrameAndBarRecipe {
  // La hauteur de base des murs, à quoi le rang de la piste s'ajoute.
  float lift_floor = 0.0F;
  std::int16_t frame_alpha = 0;
  std::int16_t frame_duration = 0;
  std::int16_t bar_duration = 0;
  // ⚠ VRAI POUR LA SEULE DIZAINE — de dix à dix-neuf. La durée écrite juste
  // avant est alors JETÉE et remplacée par la vie de l'effet.
  bool duration_from_life = false;
  // Le demi-côté du cadre, à quoi quatre dixièmes par rang s'ajoutent.
  float frame_half_side = 0.0F;
  // Celui de la barre, qui ne dépend PAS du rang — les deux barres courent sur
  // le même carré, à des départs opposés.
  float bar_half_side = 0.0F;
  float bar_start_step = 0.0F;
  float bar_start_floor = 0.0F;
  float bar_length = 0.0F;
};

[[nodiscard]] EffectFrameAndBarRecipe EffectFrameAndBarRecipeOf(std::uint8_t style_byte);

// UN NŒUD de cadre et de barre qu'un effet pose.
struct EffectFrameAndBarPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'OCTET DU SITE, et il porte tout.
  std::uint8_t style_byte = 0;
};

[[nodiscard]] std::span<const EffectFrameAndBarPattern> EffectFrameAndBarPatternTable();

[[nodiscard]] std::span<const EffectFrameAndBarPattern> EffectFrameAndBarPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud : DEUX de cadre, puis DEUX de barre.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFrameAndBarTracks(
    const EffectFrameAndBarPattern& pattern, std::int32_t effect_life_ticks);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectFrameAndBarNode(const EffectFrameAndBarPattern& pattern,
                                                        glm::vec3 anchor,
                                                        std::int32_t effect_life_ticks);

}  // namespace ro::render
