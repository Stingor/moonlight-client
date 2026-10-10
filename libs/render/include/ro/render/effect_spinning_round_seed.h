#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA RONDE, sous-type 27.
//
// UN SEUL bâtisseur et TROIS sites, un par identifiant. Chaque site lui passe DEUX octets : une
// hauteur, et un drapeau de teinte.
//
// ⚠ LES TROIS SITES POSENT LA MÊME HAUTEUR — deux — et deux d'entre eux la
// même teinte. Le champ existe parce que le bâtisseur le prend, et non parce
// que les sites en varient.

// LA TAILLE DU CARRÉ, la même pour les trois.
inline constexpr float kSpinningRoundSquareRadius = 1.0F;

// CE QUE LE BÂTISSEUR POSE DANS LA CLÉ, et que personne ne relit.
//
// ⚠ NI LE DESSINATEUR NI LE MOUVEMENT NE LA LISENT. Elle est posée et morte ;
// la transcrire coûte une ligne et évite de croire, plus tard, qu'elle manque.
inline constexpr float kSpinningRoundUnusedKey = 0.15F;

// DE COMBIEN L'OCTET DE HAUTEUR ÉLÈVE LA RONDE.
//
// ⚠ LE CLIENT ÉCRIT MOINS DEUX FOIS L'OCTET, et son repère DESCEND : la ronde
// flotte donc AU-DESSUS du nœud. Recopier le signe l'enfoncerait sous la carte.
inline constexpr float kSpinningRoundHeightPerByte = 2.0F;

// COMBIEN D'ANGLES le bâtisseur tire par piste : quatre courants de
// tremblement, puis quatre angles de ronde.
inline constexpr std::size_t kSpinningRoundDrawsPerTrack = 8;

// UNE RONDE qu'un effet pose.
struct EffectSpinningRoundPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'OCTET DE HAUTEUR, tel que le site le passe.
  std::uint8_t height_byte = 0;
  // VRAI quand le second octet du site vaut deux : la ronde est alors VERTE et
  // son alpha de plein pied monte de vingt.
  bool green = false;
};

[[nodiscard]] std::span<const EffectSpinningRoundPattern> EffectSpinningRoundPatternTable();

[[nodiscard]] std::span<const EffectSpinningRoundPattern> EffectSpinningRoundPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes vives.
//
// ⚠ LES QUATRE PARTICULES NAISSENT AU MÊME POINT, sur l'axe du nœud : c'est le
// premier tic de mouvement qui les écarte sur leur cercle. Les semer déjà
// écartées ferait paraître la ronde d'un coup au lieu de s'ouvrir.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpinningRoundTracks(
    const EffectSpinningRoundPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectSpinningRoundNode(const EffectSpinningRoundPattern& pattern,
                                                          glm::vec3 anchor,
                                                          std::int32_t effect_life_ticks,
                                                          std::uint32_t& generator);

}  // namespace ro::render
