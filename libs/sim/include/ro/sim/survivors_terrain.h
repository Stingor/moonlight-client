#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace ro::sim {

// LE TERRAIN GÉNÉRÉ D'UNE ARÈNE SANS BORD : une plaine praticable semée de
// pièces, bloquantes ou franchissables, fonction entière de (règle de terrain, graine de la
// partie, coordonnées du morceau). Aucun flottant, aucun état : un morceau
// regénéré est identique octet pour octet, quel que soit l'ordre dans lequel
// les morceaux sont demandés, sous MSVC comme sous g++.
//
// Le monde se découpe en MORCEAUX de 32 × 32 cases, chacun en 16 EMPLACEMENTS
// de 8 × 8. Une pièce tient dans son emplacement en laissant libre la bordure
// d'une case : la praticabilité d'une case ne dépend que de son emplacement et
// des chemins (voir plus bas), fonction pure de la graine qu'un morceau calcule
// sans générer de voisin, et le réseau des bordures relie tout le monde. Un morceau sur
// `landmark_period_chunks`² devient un GRAND EMPLACEMENT d'un seul tenant, qui
// porte un repère sous la même règle de bordure.
//
// Les coordonnées de case suivent celles de l'arène : x vers l'est, y vers le
// nord, une case de chemin sur deux `uint16`.

inline constexpr std::int32_t kTerrainWorldCells = 65536;
// Le départ d'une arène générée : le centre du monde, comme pour une carte.
inline constexpr std::int32_t kTerrainStartCell = kTerrainWorldCells / 2;
inline constexpr std::int32_t kTerrainChunkCells = 32;
inline constexpr std::int32_t kTerrainSlotCells = 8;
inline constexpr std::int32_t kTerrainSlotsPerSide = kTerrainChunkCells / kTerrainSlotCells;
inline constexpr std::int32_t kTerrainWorldChunks = kTerrainWorldCells / kTerrainChunkCells;
inline constexpr std::size_t kTerrainChunkArea =
    static_cast<std::size_t>(kTerrainChunkCells) * static_cast<std::size_t>(kTerrainChunkCells);
// La bordure libre de chaque emplacement, et la plus grande emprise qui y tient.
inline constexpr std::int32_t kTerrainBorderCells = 1;
inline constexpr std::int32_t kMostPieceSideCells = kTerrainSlotCells - 2 * kTerrainBorderCells;
inline constexpr std::int32_t kMostLandmarkSideCells = kTerrainChunkCells - 2 * kTerrainBorderCells;
// Les densités se donnent pour mille emplacements.
inline constexpr std::uint32_t kTerrainPerMille = 1000;
inline constexpr std::uint8_t kTerrainQuarterTurns = 4;
// Le flux du `Pcg32` d'un morceau : « CHUNK » en ASCII.
inline constexpr std::uint64_t kTerrainStream = 0x0000'0043'4855'4E4BULL;
// LES TIRAGES D'UN MORCEAU, dans cet ordre et toujours tous : pour chaque
// emplacement, ligne par ligne du sud au nord et d'ouest en est, un
// Chance(densité, 1000), un PickWeighted des pièces ordinaires, un Below(4)
// du quart de tour, puis un Below par axe pour le coin ; enfin, pour le grand
// emplacement, un PickWeighted des repères, un Below(4) et un Below par axe.
// Un emplacement vide, dans la clairière ou dans un morceau à repère, tire
// comme les autres : son voisin ne décale jamais sa suite. Un PickWeighted de
// poids tous nuls ne tire rien, ce qui reste fixé pour une règle donnée.
inline constexpr std::uint32_t kTerrainDrawsPerSlot = 5;
inline constexpr std::uint32_t kTerrainDrawsPerLandmark = 4;

// LES CHEMINS DE TERRE. Le monde se pave de CELLULES de `path_period_chunks`
// morceaux de côté, et chaque cellule porte un CARREFOUR ; chaque carrefour se
// relie à celui de la cellule voisine à l'est et à celui de la voisine au nord.
// Un carrefour se tient à au moins kTerrainPathMarginCells du bord de sa
// cellule : au centre du premier morceau à repère de la cellule s'il y en a un,
// sinon là où le tire un mélange de la graine (sel « SURVPATH », un mélange par
// carrefour : x est son reste entier, y le reste de sa moitié haute). Un
// chemin serpente :
// tous les kTerrainPathBendCells cases le long de son axe, un coude s'écarte
// de la droite d'au plus `path_wiggle_cells` (sel « SURVBEND », un mélange par
// coude), puis le tracé entier de coude en coude se peint d'un disque de
// `path_width_cells` de diamètre. Aucun tirage du `Pcg32` d'un morceau : les
// chemins ne décalent aucune pièce, et un morceau les calcule seul, sans
// consulter un voisin.
//
// UNE PIÈCE ORDINAIRE dont le rectangle d'emprise tournée touche une case de
// chemin ne se pose pas : son emplacement reste vide. Un chemin ne fait donc
// que libérer des cases : la connexité tient.
inline constexpr std::int32_t kTerrainPathBendCells = 16;
inline constexpr std::int32_t kTerrainPathMarginCells = kTerrainChunkCells / 2;

// LA PLACE D'UN REPÈRE, quand la règle a des chemins. Seul le PREMIER morceau
// à repère d'une cellule de chemins (par carré de repères du sud au nord et
// d'ouest en est) porte son repère ; le carrefour de la cellule est son centre,
// et une PLACE de terre l'entoure : un disque de kTerrainPlazaCells cases de
// diamètre, peint comme un chemin, où ne se pose aucune pièce. Le repère se
// dresse au bord de la place, dans son SITE : un carré de
// kTerrainLandmarkSiteCells cases dont le coin le plus proche du centre s'en
// écarte de kTerrainSiteOffsetCells sur chaque axe, dans le quart que tire un
// mélange de la graine (sel « SURVSIDE », un mélange par morceau : bit 0 l'est,
// bit 1 le nord). L'emprise s'y cale contre ce coin ; les deux tirages de coin
// du repère restent faits, et ne servent pas.
//
// Un chemin qui part de la place du côté de son site passe d'abord, en ligne
// droite sur son axe, par une PORTE à TerrainPlazaGateCells(largeur) cases du
// centre, au-delà du site ; de l'autre côté, il rejoint le centre sans porte.
// Les tracés sur un axe avancent toujours : aucun chemin n'entre dans un site,
// ni ne s'arrête dans une emprise. Une porte reste à moins d'un demi-morceau
// de son centre, si bien que les portes de deux places voisines se suivent.
inline constexpr std::int32_t kTerrainPlazaCells = 9;
inline constexpr std::int32_t kTerrainLandmarkSiteCells = 8;
inline constexpr std::int32_t kTerrainSiteOffsetCells = 4;
[[nodiscard]] constexpr std::int32_t TerrainPlazaGateCells(std::int32_t path_width_cells) noexcept {
  return kTerrainSiteOffsetCells + kTerrainLandmarkSiteCells + path_width_cells / 2;
}
// La plus large route qui longe un site sans le toucher, et dont la porte
// reste à moins d'un demi-morceau du centre.
inline constexpr std::int32_t kMostPlazaPathWidthCells =
    2 * (kTerrainChunkCells / 2 - kTerrainSiteOffsetCells - kTerrainLandmarkSiteCells) - 1;

// UNE PIÈCE : une règle de jeu, sans aucun fichier. `app` associe sa clé à des
// modèles. L'emprise est un dessin de lignes de même largeur, « # » couvert,
// « . » libre, la PREMIÈRE ligne au nord ; d'au plus kMostPieceSideCells de
// côté pour une pièce ordinaire, kMostLandmarkSideCells pour un repère
// (kTerrainLandmarkSiteCells si la règle a des chemins), et sans case libre
// enclavée. Une case couverte est BLOQUÉE, sauf sous une pièce franchissable.
struct TerrainPieceRule {
  std::uint16_t piece_id = 0;
  const char* key = "";
  std::span<const std::string_view> footprint;
  std::uint32_t weight = 0;
  // Un repère ne se pose que sur un grand emplacement, jamais ailleurs.
  bool landmark = false;
  // UNE PIÈCE FRANCHISSABLE (un fourré) ne bloque aucune case : le héros, les
  // monstres et leurs apparitions la traversent. Son emprise reste la place
  // où `app` la dessine, qu'aucun chemin ne touche et qu'aucun semis ne prend.
  bool walkable = false;
};

struct TerrainRule {
  std::uint16_t terrain_id = 0;  // 0 est réservé : l'arène lit la carte de `map_name`
  const char* key = "";
  std::span<const TerrainPieceRule> pieces;
  // LA DENSITÉ : la chance, pour mille, qu'un emplacement porte une pièce. Un
  // nœud tous les `density_period_chunks` morceaux tire sa valeur dans
  // [min, max] ; un emplacement interpole les quatre nœuds qui l'entourent.
  std::uint32_t density_min_per_mille = 0;
  std::uint32_t density_max_per_mille = 0;
  std::int32_t density_period_chunks = 1;
  // Aucun emplacement à moins de `clearing_cells` du départ (distance de
  // Tchebychev à sa case la plus proche) ne porte de pièce.
  std::int32_t clearing_cells = 0;
  // Un morceau à repère par carré de `landmark_period_chunks` de côté ; 0 :
  // aucun repère.
  std::int32_t landmark_period_chunks = 0;
  // Les chemins : un carrefour par cellule de `path_period_chunks` morceaux de
  // côté ; 0 : aucun chemin. Un chemin s'écarte de sa droite d'au plus
  // `path_wiggle_cells` ; écart et largeur ensemble tiennent dans
  // kTerrainPathMarginCells, si bien qu'un chemin ne quitte jamais les deux
  // cellules qu'il relie. Avec des repères, la largeur tient dans
  // kMostPlazaPathWidthCells.
  std::int32_t path_period_chunks = 0;
  std::int32_t path_width_cells = 0;
  std::int32_t path_wiggle_cells = 0;
};

// Une pièce posée : `x` et `y` sont la case du monde du coin sud-ouest de son
// emprise TOURNÉE, `quarter` ses quarts de tour dans le sens trigonométrique
// (vus du ciel, nord en haut).
struct PlacedPiece {
  std::uint16_t piece_id = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::uint8_t quarter = 0;
  [[nodiscard]] bool operator==(const PlacedPiece&) const = default;
};

struct TerrainChunk {
  std::int32_t chunk_x = 0;
  std::int32_t chunk_y = 0;
  // Un octet par case, 1 praticable : `local_y × 32 + local_x`, la ligne 0 au sud.
  std::array<std::uint8_t, kTerrainChunkArea> passable{};
  // Un octet par case, 1 sur un chemin, dans le même ordre ; une case de
  // chemin est toujours praticable.
  std::array<std::uint8_t, kTerrainChunkArea> path{};
  // Dans l'ordre des emplacements ; un repère seul dans un morceau à repère.
  std::vector<PlacedPiece> pieces;

  // Faux hors du morceau.
  [[nodiscard]] bool Passable(std::int32_t local_x, std::int32_t local_y) const noexcept;
  [[nodiscard]] bool OnPath(std::int32_t local_x, std::int32_t local_y) const noexcept;
  [[nodiscard]] bool operator==(const TerrainChunk&) const = default;
};

struct FootprintSize {
  std::int32_t width = 0;   // d'ouest en est
  std::int32_t height = 0;  // du sud au nord
  [[nodiscard]] bool operator==(const FootprintSize&) const = default;
};

// La taille de l'emprise après `quarter` quarts de tour : un quart impair
// échange largeur et hauteur. Le dessin se lit sur sa première ligne.
[[nodiscard]] FootprintSize FootprintSizeOf(const TerrainPieceRule& piece,
                                            std::uint8_t quarter) noexcept;
// Vrai si la case (`east`, `north`) de l'emprise tournée, comptée depuis son
// coin sud-ouest, est couverte (« # ») ; faux hors de l'emprise. Elle est
// bloquée si la pièce n'est pas franchissable.
[[nodiscard]] bool FootprintCovers(const TerrainPieceRule& piece, std::uint8_t quarter,
                                   std::int32_t east, std::int32_t north) noexcept;

// Le morceau qui contient une case, par division arrondie vers −∞.
[[nodiscard]] std::int32_t TerrainChunkOf(std::int32_t cell) noexcept;

// LA GRAINE D'UN MORCEAU : un pas du finaliseur SplitMix64 par composante
// (graine de la partie, sel du terrain, x, y), en arithmétique non signée. Le
// rendu en dérive sa graine visuelle.
[[nodiscard]] std::uint64_t TerrainChunkSeed(std::uint64_t run_seed, std::int32_t chunk_x,
                                             std::int32_t chunk_y) noexcept;

// La chance, pour mille, qu'un emplacement centré sur la case (x, y) porte une
// pièce : le bruit de valeur entier de la règle, dans [min, max].
[[nodiscard]] std::uint32_t TerrainDensityPerMille(const TerrainRule& rule, std::uint64_t run_seed,
                                                   std::int64_t cell_x, std::int64_t cell_y);

// Une case du monde, sur 64 bits : un carrefour peut tomber hors du monde.
struct TerrainCell {
  std::int64_t x = 0;
  std::int64_t y = 0;
  [[nodiscard]] bool operator==(const TerrainCell&) const = default;
};

// La cellule des chemins qui contient le morceau `chunk`, sur un axe (division
// vers −∞) ; et le carrefour de la cellule (cell_x, cell_y). Sans chemin dans
// la règle, la cellule 0 et le carrefour (0, 0).
[[nodiscard]] std::int64_t TerrainPathCellOf(const TerrainRule& rule, std::int64_t chunk) noexcept;
[[nodiscard]] TerrainCell TerrainPathCrossing(const TerrainRule& rule, std::uint64_t run_seed,
                                              std::int64_t cell_x, std::int64_t cell_y);

// La place de la cellule de chemins (cell_x, cell_y) : son centre, qui est le
// carrefour, et le quart de son site. `exists` est faux sans repère dans la
// cellule, ou sans chemin dans la règle.
struct TerrainPlaza {
  bool exists = false;
  TerrainCell centre;
  bool site_east = false;
  bool site_north = false;
  [[nodiscard]] bool operator==(const TerrainPlaza&) const = default;
};
[[nodiscard]] TerrainPlaza TerrainPathPlaza(const TerrainRule& rule, std::uint64_t run_seed,
                                            std::int64_t cell_x, std::int64_t cell_y);

// Le morceau (chunk_x, chunk_y), défini pour tout couple d'entiers. Une règle
// refusée par CheckSurvivorsRules ne fait pas planter : une pièce qui ne tient
// pas dans son emplacement n'est pas posée.
[[nodiscard]] TerrainChunk GenerateTerrainChunk(const TerrainRule& rule, std::uint64_t run_seed,
                                                std::int32_t chunk_x, std::int32_t chunk_y);

// La praticabilité d'une case du monde, par génération de son morceau : la
// référence qu'un sol mis en cache doit rendre. Faux hors de
// [0, kTerrainWorldCells) sur un axe.
[[nodiscard]] bool TerrainPassableAt(const TerrainRule& rule, std::uint64_t run_seed,
                                     std::int32_t x, std::int32_t y);

}  // namespace ro::sim
