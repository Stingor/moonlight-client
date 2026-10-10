#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA TENTURE AU SOL, sous-type 48.
//
// UN SEUL bâtisseur pour les cinq identifiants, et l'octet de son site choisit
// TROIS figures entières — pas une teinte, pas une taille : une DISPOSITION.
//
// ⚠ L'OCTET DÉCIDE AUSSI DE L'ALPHA ET DU NOMBRE DE PISTES VIVES, et aucun des
// trois ne se déduit des deux autres. Le lire comme un simple numéro de teinte
// donnerait la bonne couleur sur la mauvaise figure.

// LES TROIS DISPOSITIONS, par l'octet du site.
//
//   - ZÉRO : la GERBE. Quatre traits qui partent du centre vers des points
//     tirés au sort dans un carré de quarante, et un alpha presque nul ;
//   - UN et DEUX : le TREILLIS. Quatre traits qui traversent un carré de six,
//     deux dans un sens et deux dans l'autre, chacun décalé au sort ;
//   - TROIS et QUATRE : la CROIX. Deux traits seulement, l'un sur l'axe des x,
//     l'autre sur celui des z, et les deux autres pistes restent éteintes.
inline constexpr std::uint8_t kGroundCurtainScatterByte = 0;
inline constexpr std::uint8_t kGroundCurtainLatticeFirstByte = 1;
inline constexpr std::uint8_t kGroundCurtainLatticeLastByte = 2;
inline constexpr std::uint8_t kGroundCurtainCrossFirstByte = 3;
inline constexpr std::uint8_t kGroundCurtainCrossLastByte = 4;

// ⚠ ET TOUT AUTRE OCTET NE SÈME RIEN DU TOUT. Le client saute la piste entière
// — pas même son drapeau. Traiter le défaut comme la gerbe inventerait une
// figure qu'aucun site ne demande.

// CE QUE CHAQUE DISPOSITION POSE : un alpha, une portée, un nombre de pistes.
inline constexpr std::int16_t kGroundCurtainScatterAlpha = 15;
inline constexpr std::int16_t kGroundCurtainLatticeAlpha = 50;
inline constexpr std::int16_t kGroundCurtainCrossAlpha = 100;

inline constexpr std::int32_t kGroundCurtainScatterSpread = 40;
inline constexpr float kGroundCurtainScatterFloor = -20.0F;
inline constexpr std::int32_t kGroundCurtainLatticeSpread = 7;
inline constexpr float kGroundCurtainLatticeHalf = 3.0F;
inline constexpr float kGroundCurtainCrossHalf = 6.0F;
inline constexpr std::size_t kGroundCurtainCrossLiveTracks = 2;

// ⚠ LA MOITIÉ DES PISTES TRAVERSE DANS L'AUTRE SENS, et la coupure n'est pas au
// même rang selon la disposition : le treillis bascule après la DEUXIÈME piste,
// la croix après la PREMIÈRE. Prendre la même coupure pour les deux ferait une
// croix à deux branches parallèles.
inline constexpr std::size_t kGroundCurtainLatticeTurnsAt = 2;
inline constexpr std::size_t kGroundCurtainCrossTurnsAt = 1;

// UNE TENTURE QU'UN EFFET POSE.
struct EffectGroundCurtainPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'OCTET DU SITE, tel quel : il choisit la disposition, l'alpha, le nombre
  // de pistes vives ET la teinte.
  std::uint8_t arrangement = 0;
};

[[nodiscard]] std::span<const EffectGroundCurtainPattern> EffectGroundCurtainPatternTable();

[[nodiscard]] std::span<const EffectGroundCurtainPattern> EffectGroundCurtainPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, dont deux ou quatre sont vives.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGroundCurtainTracks(
    const EffectGroundCurtainPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectGroundCurtainNode(const EffectGroundCurtainPattern& pattern,
                                                          glm::vec3 anchor,
                                                          std::int32_t effect_life_ticks,
                                                          std::uint32_t& generator);

}  // namespace ro::render
