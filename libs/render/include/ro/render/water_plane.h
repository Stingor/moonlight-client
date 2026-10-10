#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/formats/gnd.h"
#include "ro/formats/image.h"
#include "ro/formats/rsw.h"

namespace ro::render {

// LA NAPPE D'EAU D'UNE CARTE : une grille plate, posée à l'altitude que le
// fichier donne, ondulée par une houle et habillée d'une animation de 32 images.
//
// Aucune dépendance à OpenGL : c'est une transformation de données, et c'est ce
// qui permet d'éprouver au banc les conventions dans lesquelles on se trompe
// sans le voir — le sens de l'axe vertical, l'orientation de la houle, quelles
// cellules reçoivent de l'eau.
//
// Le quadrilatère d'eau d'une cellule et l'avance de l'animation suivent le
// client d'origine ; le renderer de GRFEditor, implémentation de référence
// indépendante, les recoupe.

// Combien d'images compte une animation d'eau.
//
// ⚠ TRENTE-DEUX, ET CE N'EST PAS UNE LIMITE QU'ON SE DONNE : le client remet
// son compteur à zéro dès qu'il atteint `32 × cadence`, et son index d'image
// est masqué sur cinq bits. Une trente-troisième image ne serait jamais lue.
inline constexpr int kWaterFrameCount = 32;

// Sur combien de cubes la texture se répète avant de recommencer.
//
// Le client prend `index_de_cube & 3` : la nappe pave donc par carrés de quatre
// cubes, et la texture se raccorde d'un carré au suivant.
inline constexpr int kWaterTileCubes = 4;

// L'OPACITÉ DE LA NAPPE, celle du client : il peint ses quatre sommets
// en `0x90FFFFFF`, donc un alpha de 144 sur 255.
//
// ⚠ ELLE NE SE DÉDUIT PAS DE LA TEXTURE, qui est un JPEG donc opaque. Sans
// elle, l'eau cacherait le fond au lieu de le voiler.
inline constexpr float kWaterOpacity = 144.0F / 255.0F;

// LA CADENCE À LAQUELLE L'EAU AVANCE, en tics par seconde.
//
// 🔴 LE CLIENT D'ORIGINE N'EN A PAS : il avance sa houle et son image d'UN TIC
// PAR IMAGE RENDUE, si bien que son eau va plus vite sur une machine rapide.
// Sur la zone d'eau d'einbroch, il compte tantôt 70, tantôt 106 tics par
// seconde, sur LA MÊME machine et LA MÊME carte. Un minuteur rendrait le même
// chiffre deux fois.
//
// On ne reprend donc pas ce comptage : chez nous l'eau avance avec le TEMPS, à
// une cadence figée, et elle ondule pareil qu'on plafonne la cadence à 30 ou
// qu'on la laisse filer à 144. Soixante est la lecture canonique d'« un tic par
// image », et c'est le chiffre que GRFEditor a retenu pour le défilement des
// textures.
inline constexpr float kWaterTicksPerSecond = 60.0F;

// Où le client range les images d'eau, et comment il les nomme.
//
// ⚠ LE DOSSIER EST EN CORÉEN, en octets CP949 tels quels — c'est un chemin
// d'ARCHIVE, à passer à `Vfs::ReadRaw`, jamais au chemin transcodé.
[[nodiscard]] std::string WaterTexturePath(std::int32_t type, int frame);

struct WaterVertex {
  std::array<float, 3> position{};
  std::array<float, 2> texture_uv{};
  // LA DIAGONALE DU SOMMET : la somme de ses deux indices de cube.
  //
  // C'est l'argument de la houle, et c'est ce qui la fait courir en biais sur
  // la carte plutôt que le long d'un axe. Le client la recalcule à chaque
  // sommet ; on la range ici pour que le banc puisse la lire sans carte
  // graphique.
  float wave_step = 0.0F;
};

// Une zone d'eau : la tranche de sommets qui la couvre, et ses réglages.
//
// ⚠ UNE CARTE PEUT EN AVOIR PLUSIEURS, à des niveaux différents — jor_dun02 en
// porte 3 × 3 avec deux altitudes. Chacune a sa propre texture, sa propre houle
// et sa propre cadence, donc son propre lot de dessin.
struct WaterZoneMesh {
  formats::GndWaterZone settings;
  std::uint32_t first_vertex = 0;
  std::uint32_t vertex_count = 0;
};

struct WaterPlane {
  std::vector<WaterVertex> vertices;
  std::vector<WaterZoneMesh> zones;

  [[nodiscard]] bool empty() const noexcept { return vertices.empty(); }
};

// Les 32 images d'une animation d'eau, pour UN type de nappe.
//
// ⚠ PAR TYPE ET NON PAR ZONE : deux zones d'une même carte partagent
// généralement le même type, et le charger deux fois paierait deux fois
// trente-deux images pour la même eau.
struct WaterAnimation {
  std::int32_t type = 0;
  std::vector<formats::Image> frames;
};

// D'OÙ VIENT L'EAU D'UNE CARTE, et il y a deux sources qui ne se recouvrent pas.
//
// Le GND la porte à partir de sa version 1.8, découpée en zones ; avant cela
// c'est le RSW qui la porte, en un seul niveau, jusqu'à sa version 2.6 où la
// section a déménagé. Une carte sans eau rend une liste VIDE.
//
// ⚠ L'ORDRE COMPTE : le GND est interrogé d'abord. Une carte récente porte les
// deux, et c'est celle du GND qui fait foi — le champ du RSW y est un reste.
[[nodiscard]] std::vector<formats::GndWaterZone> WaterZonesOf(const formats::GndMap& gnd,
                                                              const formats::RswScene& rsw);

// Bâtit la nappe : un quadrilatère par cube que l'eau peut atteindre.
//
// ⚠ « PEUT ATTEINDRE », PAS « ATTEINT » : le cube est retenu dès qu'un de ses
// quatre coins passe sous le CREUX de la houle. Le client, lui, retranche à
// chaque image les cubes que la vague ne couvre pas à cet instant ; refaire ce
// tri demanderait de rebâtir le maillage soixante fois par seconde pour gagner
// une frange d'un cube au bord de l'eau. On garde donc le maillage le plus
// large, qui contient tout ce que le client dessine à un instant ou à un autre.
//
// 🔴 `invisible_tiles` VIENT DE `FindInvisibleTiles`, ET IL N'EST PAS FACULTATIF
// EN PRATIQUE. Le client ne pose son eau que sur un cube DONT LE SOL A DESSINÉ
// UN QUAD — il lit les quatre coins de ce quad pour décider. Un carreau que sa
// texture rend tout transparent est un TROU creusé par le mappeur : le terrain
// ne l'émet pas, le client n'y met pas d'eau, et nous non plus. Sans cette
// liste, hu_fild01 se couvre d'une nappe bleue par-dessus le vide, très
// exactement là où le client ne montre rien.
[[nodiscard]] WaterPlane BuildWaterPlane(const formats::GndMap& gnd, const formats::RswScene& rsw,
                                         std::span<const std::uint8_t> invisible_tiles = {});

// L'image à montrer à cet instant, de 0 à 31.
//
// `texture_cycling` est un nombre de TICS PAR IMAGE : trois, le défaut, fait
// vingt images par seconde.
[[nodiscard]] int WaterFrameAt(const formats::GndWaterZone& zone, std::uint32_t elapsed_ms);

// La phase de la houle à cet instant, en DEGRÉS.
//
// ⚠ EN DEGRÉS ET NON EN RADIANS : le client range cette phase telle quelle dans
// sa zone d'eau, l'avance de `wave_speed` par tic et la replie dès qu'elle
// passe 180. La replier pareil n'est pas cosmétique — une phase laissée courir
// une nuit entière perdrait son incrément dans les décimales que la mantisse
// n'a plus, et la houle s'arrêterait sans que rien ne le dise.
[[nodiscard]] float WaterPhaseAt(const formats::GndWaterZone& zone, std::uint32_t elapsed_ms);

}  // namespace ro::render
