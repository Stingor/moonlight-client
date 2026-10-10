#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/render/ground_quad.h"

namespace ro::render {

// LA TRACE DU GUIDAGE AU SOL : des flèches couchées sur le chemin qui mène à la
// cible, une case sur deux, et une vague plus vive qui court vers la cible.
//
// Description PURE, comme le curseur au sol : le chemin entre, des marques et
// des quads sortent. Les textures sont l'affaire de l'appelant.
//
// Ce qui s'écarte du client d'origine, et pourquoi :
//
//   · il ne dessine que les cases à dix pas du joueur. Ici la trace porte sur
//     `kNaviTrailReachCells` cases de chemin : à la caméra du jeu, dix cases ne
//     montrent guère que le tournant suivant ;
//   · il compte les cases DEPUIS LE JOUEUR, sur un chemin qu'il ne recalcule
//     pas. Le nôtre se relit à chaque pas ; compté depuis le joueur, chaque pas
//     ferait sauter les flèches d'une case à sa voisine. On les compte donc
//     depuis la CIBLE, qui ne bouge pas : une flèche reste sur sa case tant que
//     le joueur suit la trace, et la vague ne saute pas non plus ;
//   · sa vague n'éclaire que les dix premiers rangs du chemin. Ici elle revient
//     tous les `kNaviTrailWaveRanks` rangs, sur toute la portée.

// Les jeux de flèches que le joueur choisit, numérotés de 1 à 8.
inline constexpr int kNaviTrailFirstIconSet = 1;
inline constexpr int kNaviTrailLastIconSet = 8;

// Huit directions, dans l'ordre des images de l'archive — celui des acteurs :
// 0 vers −Y (sud), puis dans le sens horaire (1 sud-ouest, 2 −X ouest, 4 nord, 6 est).
inline constexpr std::size_t kNaviTrailDirections = 8;

// Combien de cases de chemin, devant le joueur, portent la trace.
inline constexpr std::size_t kNaviTrailReachCells = 40;

// Une flèche toutes les deux cases.
inline constexpr std::size_t kNaviTrailCellsPerMark = 2;

// L'opacité d'une flèche, et celle de la flèche que la vague éclaire.
inline constexpr std::uint8_t kNaviTrailAlpha = 0x9A;
inline constexpr std::uint8_t kNaviTrailWaveAlpha = 0xEF;

// La vague avance d'une flèche tous les `kNaviTrailWaveStepMs`, et revient au
// bout de `kNaviTrailWaveRanks` flèches.
inline constexpr std::uint32_t kNaviTrailWaveStepMs = 100;
inline constexpr std::uint32_t kNaviTrailWaveRanks = 10;

// Une flèche : sa case, sa direction (0..7, voir plus haut) et son opacité.
struct NaviTrailMark {
  std::int32_t cell_x = 0;
  std::int32_t cell_y = 0;
  std::uint8_t direction = 0;
  std::uint8_t alpha = kNaviTrailAlpha;

  [[nodiscard]] friend bool operator==(const NaviTrailMark&, const NaviTrailMark&) = default;
};

// La direction d'un déplacement de (dx, dy) cases, arrondie au huitième de tour
// le plus proche. Un pas d'une case tombe juste ; un segment plus long — le
// chemin de secours, droit jusqu'à la cible — prend l'octant qui le contient.
// Rend nullopt pour un déplacement nul.
[[nodiscard]] std::optional<std::uint8_t> NaviTrailDirectionOf(std::int32_t delta_x,
                                                               std::int32_t delta_y);

// Les flèches à poser sur `path_cells`, ordonné du joueur (rang 0) vers la cible.
//
// La flèche de la case k pointe vers la case k + 1 : la dernière case n'en porte
// donc jamais, faute de suivante. Seules les cases de rang inférieur à
// `reach_cells` en portent. Voir le haut du fichier pour le compte depuis la
// cible et la vague.
[[nodiscard]] std::vector<NaviTrailMark> ComposeNaviTrail(
    std::span<const glm::ivec2> path_cells, std::uint32_t now_ms,
    std::size_t reach_cells = kNaviTrailReachCells);

// Le quad d'une flèche, posé sur les hauteurs de la grille.
//
// ⚠ L'IMAGE SE LIT VUE DE DESSUS, LE NORD EN HAUT : son bord haut va du côté
// des y croissants, son bord gauche du côté des x décroissants. C'est
// l'inverse, en v, du quad de cellule ordinaire — indifférent pour le cadre du
// curseur, qui est symétrique, mais une flèche pointerait à rebours.
//
// Rend nullopt hors de la grille.
[[nodiscard]] std::optional<GroundQuad> ComposeNaviTrailQuad(const formats::GndMap& gnd,
                                                             const formats::GatMap& gat,
                                                             const NaviTrailMark& mark);

// Le dossier des images de flèches, dans l'archive. 32 × 32, avec alpha.
inline constexpr std::string_view kNaviTrailTextureFolder =
    "data\\texture\\유저인터페이스\\navigation_interface3\\";

// Le chemin, dans l'archive, de l'image d'un jeu et d'une direction. Un jeu
// hors de 1..8 est ramené dans l'intervalle.
[[nodiscard]] std::string NaviTrailTexturePath(int icon_set, std::uint8_t direction);

}  // namespace ro::render
