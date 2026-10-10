#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/formats/image.h"
#include "ro/render/lightmap_atlas.h"

namespace ro::render {

// Maillage du terrain construit à partir d'un GND.
//
// Aucune dépendance à OpenGL : c'est une transformation de données pure, et
// c'est délibéré. Les conventions dans lesquelles on se trompe silencieusement
// — hauteur négative, axe Z inversé, appariement coin ↔ texture — se vérifient
// ainsi par des assertions numériques, sans fenêtre ni carte graphique.

// Un cube GND fait dix unités de côté dans le monde.
inline constexpr float kWorldUnitsPerCube = 10.0F;

// Ce que vaut UNE cellule : un cube en couvre deux.
inline constexpr float kWorldUnitsPerCell = kWorldUnitsPerCube / 2.0F;

struct TerrainVertex {
  std::array<float, 3> position{};
  std::array<float, 2> texture_uv{};
  // Position dans l'atlas des lightmaps. Sans elle, le terrain sort sans ses
  // ombres cuites, qui font l'essentiel du relief perçu d'une carte RO.
  std::array<float, 2> lightmap_uv{};
  // Couleur du carreau, en RGBA normalisé.
  std::array<float, 4> color{};
  // Normale de la face, normalisée.
  std::array<float, 3> normal{};
};

// Un lot de dessin : les sommets partageant une même texture sont contigus.
struct TerrainSurface {
  std::int16_t texture_index = 0;
  std::uint32_t first_vertex = 0;
  std::uint32_t vertex_count = 0;
};

struct TerrainMesh {
  std::vector<TerrainVertex> vertices;
  std::vector<TerrainSurface> surfaces;
  std::array<float, 3> bounds_min{};
  std::array<float, 3> bounds_max{};

  [[nodiscard]] bool empty() const noexcept { return vertices.empty(); }
  // Centre de l'emprise : point de visée naturel pour cadrer une carte entière.
  [[nodiscard]] std::array<float, 3> center() const noexcept;
};

// Les carreaux du GND dont la région de texture est ENTIÈREMENT transparente.
//
// Un carreau de terrain n'est pas à moitié là : ou bien la carte a du sol à cet
// endroit, ou bien elle n'en a pas. Le mappeur exprime le second cas en posant
// un morceau de texture tout en magenta — la couleur de transparence de RO.
//
// Savoir cela AVANT de construire le maillage est ce qui permet de ne pas
// émettre le carreau du tout, plutôt que de le dessiner et de compter sur le
// nuanceur pour le rejeter. Cette dernière voie ne tient pas : le rejet lit
// l'alpha d'un mipmap, et un mipmap grossier moyenne le magenta avec ses
// voisins opaques jusqu'à repasser au-dessus du seuil. La carte se couvre
// alors, de loin, de traits clairs suspendus au-dessus du vide.
//
// Rend un octet par carreau, non nul quand le carreau est invisible. Vide si
// les textures manquent — rien n'est alors écarté.
[[nodiscard]] std::vector<std::uint8_t> FindInvisibleTiles(
    const formats::GndMap& gnd, std::span<const formats::Image> textures);

// Construit le maillage : faces du dessus, plus les faces latérales et
// frontales qui ferment les dénivelés.
//
// L'atlas peut être vide — les coordonnées de lightmap valent alors zéro, et le
// rendu se passe simplement d'ombres.
//
// `invisible_tiles` vient de FindInvisibleTiles. Vide, tout est émis.
[[nodiscard]] TerrainMesh BuildTerrainMesh(const formats::GndMap& gnd, const LightmapAtlas& atlas,
                                           std::span<const std::uint8_t> invisible_tiles = {});

// L'altitude du sol en un point du monde, dans le repère du maillage.
//
// C'est l'INVERSE exact de la construction ci-dessus, et c'est pour cela qu'il
// vit à côté d'elle : les deux partagent le placement des cubes et l'inversion
// de l'axe vertical, et les séparer laisserait l'un dériver de l'autre — un
// personnage flotterait alors au-dessus du sol qu'il est censé fouler.
//
// La hauteur est interpolée dans le cube qui contient le point. Retenir une
// seule hauteur de coin ferait sursauter un personnage d'un cube à l'autre.
// Un point hors de la carte est ramené au cube du bord le plus proche.
[[nodiscard]] float GroundHeightAt(const formats::GndMap& gnd, float world_x, float world_z);

// Le point du monde où se tient un acteur posé sur une CELLULE de la grille.
//
// Une cellule de la grille de franchissement vaut la moitié d'un cube ; le
// point rendu est son centre, à l'altitude du terrain. C'est la conversion
// dont le réseau aura besoin, puisque le serveur ne parle qu'en cellules.
[[nodiscard]] std::array<float, 3> CellCenterPosition(const formats::GndMap& gnd,
                                                      std::int32_t cell_x, std::int32_t cell_y);

// La CELLULE qui contient un point du monde. L'inverse de la précédente.
//
// C'est la conversion dont le clic a besoin : le serveur ne comprend que des
// cellules, et un pointeur désigne un point. Le résultat n'est pas borné à la
// carte — c'est à l'appelant de décider si une cellule hors grille se refuse
// ou se ramène au bord, et les deux réponses sont légitimes selon l'usage.
[[nodiscard]] std::array<std::int32_t, 2> CellAtPosition(const formats::GndMap& gnd, float world_x,
                                                         float world_z);

// Le même, entre deux cellules.
//
// Un acteur qui marche n'est sur aucune case en particulier : il est quelque
// part entre les deux bouts de son pas. L'arrondir à la case la plus proche
// ferait avancer tout le monde par saccades d'une demi-case.
// L'altitude du sol PRATICABLE à une cellule fractionnaire, dans le monde.
//
// Un acteur ne se tient pas sur le décor mais sur le sol que le serveur lui
// donne, et les deux diffèrent là où la carte fait marcher AU-DESSUS du
// terrain : un pont, une arche, une plateforme. Le GND porte le sol que l'on
// VOIT, le GAT celui sur lequel on MARCHE, et il est deux fois plus fin — une
// cellule contre un cube.
//
// C'est là que le jeu lit l'altitude d'un acteur : il parcourt une table de
// vingt octets par cellule — quatre
// hauteurs et un type, la structure du GAT — et interpole entre les quatre
// coins.
//
// ⚠ La cellule se compte comme partout ailleurs ici : un nombre ENTIER désigne
// le CENTRE de sa cellule, comme pour CellPositionAt. Lire les coins au lieu du
// centre décalerait chaque acteur d'une demi-cellule, ce qui ne se voit pas sur
// un terrain plat et saute aux yeux au bord d'une plateforme.
//
// Hors de la grille, on retombe sur la cellule du bord.
[[nodiscard]] float WalkableHeightAt(const formats::GatMap& gat, float cell_x, float cell_y);

// La cellule — fractionnaire — que désigne un point du monde.
//
// L'inverse exact de CellPositionAt : le centre d'une cellule y rend son
// numéro. Les deux vivent côte à côte pour qu'un changement de repère se voie
// des deux côtés à la fois.
[[nodiscard]] std::array<float, 2> FractionalCellAt(const formats::GndMap& gnd, float world_x,
                                                    float world_z);

[[nodiscard]] std::array<float, 3> CellPositionAt(const formats::GndMap& gnd, float cell_x,
                                                  float cell_y);

}  // namespace ro::render
