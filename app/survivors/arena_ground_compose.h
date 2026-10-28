#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/formats/image.h"

#include "survivors/arena_ground_plan.h"

namespace moonclient {

// LA TERRE COMPOSÉE : le sol généré peint lui-même les cubes que longe un
// chemin, d'une seule herbe et d'une seule terre lues au point du monde : deux
// cubes de lisière voisins se raccordent sans couture (ADR 0134, « La terre
// composée »).
//
// LA RÈGLE. Un cube se juge sur son BLOC : ses quatre cases et l'anneau d'une
// case autour, seize cases. Sans case de chemin, il reste l'herbe de fond ;
// tout de chemin, la terre pleine ; entre les deux, il se COMPOSE : chaque
// texel mêle l'herbe et la terre, toutes deux lues au point du monde (le même
// texel au même point, quel que soit le cube ou la fenêtre), par un MASQUE
// calculé lui aussi au point du monde :
//
// - la COUVERTURE : la part de chemin dans un carré de 2 × kBlendReachCells de
//   côté centré sur le point. Elle vaut ½ sur la limite d'une case de chemin
//   et d'une case d'herbe : le chemin garde sa largeur, ses marches se fondent ;
// - troublée d'un BRUIT de valeur de la graine visuelle, et de l'herbe
//   elle-même : une touffe sombre de l'herbe résiste à la terre ;
// - puis adoucie entre kBlendLowCover et kBlendHighCover.
//
// Le carré ne sort jamais du bloc, et le trouble ne dépasse jamais l'écart
// entre la couverture et ses seuils : un texel sans chemin dans le bloc est
// l'herbe pure, un texel tout de chemin la terre pure. C'est ce qui raccorde au
// texel près un cube composé à un voisin non composé, qui montre la même herbe
// (ou la même terre) au même point du monde.
//
// Le bloc d'un cube, la graine et la case du monde décident seuls de sa tuile :
// deux fenêtres qui se recouvrent composent les mêmes octets. Tout est pur,
// sans GL ni fichier : le fil de travail compose en bâtissant la fenêtre.

// Le demi-côté du carré de couverture, en cases. Avec le débord d'un texel
// de bord, il reste dans l'anneau d'une case du bloc.
inline constexpr float kBlendReachCells = 0.75F;
// Les seuils de la couverture troublée : en deçà l'herbe, au-delà la terre.
inline constexpr float kBlendLowCover = 0.3F;
inline constexpr float kBlendHighCover = 0.7F;

// Ce que montre un cube de bloc `path_block` (`GroundPlan::path_blocks`).
enum class GroundBlend : std::uint8_t { kGrass, kDirt, kComposed };
[[nodiscard]] GroundBlend GroundBlendOf(std::uint16_t path_block) noexcept;

// CE QUE LA COMPOSITION LIT, une fois, à l'ouverture : l'herbe de fond et la
// terre pleine du biome, décodées, de même taille, carrées, chaque cube un
// quart sur chaque axe.
struct GroundComposeSources {
  ro::formats::Image grass;
  ro::formats::Image dirt;
  // Les texels d'un côté de cube : le quart du côté de la texture.
  std::int32_t cube_texels = 0;
  // La luminance moyenne de l'herbe, sur 255 ; et l'écart moyen du vert au
  // rouge de l'herbe et de la terre, qui dit d'un texel s'il est de terre.
  float grass_mean_luminance = 0.0F;
  float grass_green_over_red = 0.0F;
  float dirt_green_over_red = 0.0F;
};

// Les sources de `grass` et `dirt`, ou rien si elles ne se composent pas
// (vides, de tailles différentes, pas carrées, côté qui ne se partage pas en
// quatre).
[[nodiscard]] std::optional<GroundComposeSources> MakeGroundComposeSources(
    const ro::formats::Image& grass, const ro::formats::Image& dirt);

// UNE TUILE : le cube et un texel de bord autour, pour que le filtrage lise
// au bord le texel du voisin. Rangées du sud au nord, colonnes d'ouest en est,
// RGBA.
inline constexpr std::int32_t kComposedBorderTexels = 1;
[[nodiscard]] std::int32_t ComposedTileSide(const GroundComposeSources& sources) noexcept;

// COMPOSE la tuile du cube du monde (`world_cube_x`, `world_cube_y`), de bloc
// `path_block`, dans `tile` (ComposedTileSide² × 4 octets).
void ComposeCubeTile(const GroundComposeSources& sources, std::uint64_t run_seed,
                     std::int32_t world_cube_x, std::int32_t world_cube_y, std::uint16_t path_block,
                     std::span<std::uint8_t> tile);

// LA TACHE RECOMPOSÉE. Une tache (`GroundTextureRole::kPatch`, `kScatter`)
// se pose tournée sur un emplacement entier, et son pourtour reprend l'herbe
// de fond de sa place : il prolonge le dessin de ses voisins. Elle se
// recompose une fois, à l'ouverture, une version par quart de tour : son MASQUE
// (la part de terre de chaque texel, lue sur l'écart du vert au rouge, lissée
// sur kPatchMaskReachTexels, nulle à moins de kPatchRimTexels du bord) mêle
// l'herbe de fond et la terre pleine lues, comme la composée, au point de
// l'emplacement où le quart de tour `turn` pose le texel. Un emplacement fait
// une période de l'herbe et de la terre : la tache montre au bord l'herbe même
// de ses voisines. Rien si `patch` n'a pas la taille des sources.
inline constexpr std::int32_t kPatchMaskReachTexels = 2;
inline constexpr std::int32_t kPatchRimTexels = 4;
[[nodiscard]] std::optional<ro::formats::Image> ComposePatchTexture(
    const GroundComposeSources& sources, const ro::formats::Image& patch, std::uint8_t turn);

// LE BORD D'UN MOTIF. Une texture du sol est bornée à ses bords ; au raccord
// de deux périodes d'un motif qui se répète (tous les 4 cubes), le bord fait
// lire au filtrage la période suivante. La texture se lit entourée de
// kMotifBorderTexels : le
// texel que montre, au-delà du bord, l'emplacement voisin — le motif `beyond`
// à la place où le quart de tour `turn` pose ce texel, la période suivante.
// L'herbe et la terre s'y bordent d'elles-mêmes ; une tache recomposée, de
// l'herbe de fond. `texture` telle quelle si les deux ne sont pas des carrés
// de même taille.
inline constexpr std::int32_t kMotifBorderTexels = 1;
[[nodiscard]] ro::formats::Image BorderMotif(const ro::formats::Image& texture,
                                             const ro::formats::Image& beyond, std::uint8_t turn);

// LES ATLAS D'UNE FENÊTRE : au plus kComposedAtlasPages pages de
// kComposedAtlasColumns tuiles de large, chacune aussi haute que ses rangées
// employées. Une page vide reste vide.
//
// LE PLAFOND DES PAGES garde la mémoire vidéo de l'arène sous sa porte : avec
// les textures de l'installation (64 texels par cube), une page pleine pèse
// 16 Mio, et le sol montré comme celui de réserve tiennent chacun les leurs ;
// une troisième page par sol passerait la porte. Deux pages tiennent 1 922
// cubes ; une fenêtre de la prairie en compose au plus 1 533 (`survivors
// render --banc`, « terre composée au pire »). Un cube au-delà garde ses
// carreaux de chemin.
inline constexpr std::size_t kComposedAtlasPages = 2;
inline constexpr std::int32_t kComposedAtlasColumns = 31;
inline constexpr std::int32_t kComposedAtlasRows = 31;

// Où un cube composé lit sa tuile : la page, et les coordonnées de texture
// des coins sud-ouest et nord-est du cube.
struct ComposedTileRef {
  std::uint8_t page = 0;
  std::array<float, 2> south_west{};
  std::array<float, 2> north_east{};
};

struct ComposedGround {
  std::array<ro::formats::Image, kComposedAtlasPages> pages;
  // Par cube de la fenêtre, dans l'ordre de `GroundPlan::cubes`.
  std::vector<std::optional<ComposedTileRef>> tiles;
  // Les cubes à composer que les pages ne tenaient pas : ils gardent leurs
  // carreaux de chemin.
  std::size_t overflow = 0;
};

// LES OCTETS DES PAGES qui tiennent `cubes` tuiles composées, au plus ce que
// les pages tiennent : la mémoire vidéo de la terre composée d'un sol.
[[nodiscard]] std::size_t ComposedPagesBytes(const GroundComposeSources& sources,
                                             std::size_t cubes) noexcept;

// Compose chaque cube de la fenêtre que son bloc dit composé.
[[nodiscard]] ComposedGround ComposeGround(const GroundComposeSources& sources,
                                           std::uint64_t run_seed, const GroundPlan& plan);

}  // namespace moonclient
