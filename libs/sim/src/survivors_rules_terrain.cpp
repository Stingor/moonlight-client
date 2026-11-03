#include <array>
#include <span>
#include <string_view>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

// ── LA PRAIRIE DE MIDGARD : les pièces du sol généré et leurs densités ─────
// Une emprise s'écrit le nord en haut, « # » couvert. Elle est le SOLIDE du
// modèle qu'app y pose (`roc-cli survivors emprises`) : chaque case que sa
// margelle, son mur, son tronc ou sa pierre couvre au quart, sous tous les
// angles qu'il peut prendre, et aucune case loin de lui. Un arbre bloque le
// carré de 2 × 2 cases de son tronc, comme le GAT des champs de Prontera
// (`roc-cli survivors pieces`, ADR 0134).

// Un arbre seul.
constexpr std::array<std::string_view, 2> kTree = {"##", "##"};
// Un rocher, sous tous ses angles.
constexpr std::array<std::string_view, 4> kRock = {"####", "####", "####", "####"};
// Un bosquet : trois arbres en L.
constexpr std::array<std::string_view, 4> kGrove = {"##..", "##..", "####", "####"};
// Un fourré : deux buissons serrés, franchissables. Son emprise est la place
// où ils se dessinent ; il ne bloque aucune case.
constexpr std::array<std::string_view, 3> kThicket = {"####", "####", "####"};
// Les repères, un par grand emplacement élu. Le moulin : le pied de sa tour,
// ses pointes est et ouest comprises ; ses ailes, en l'air, n'y comptent pas.
constexpr std::array<std::string_view, 7> kWindmill = {
    "..####..", ".######.", "########", "########", "########", ".######.", "..####..",
};
// Le puits : sa margelle ronde.
constexpr std::array<std::string_view, 5> kWell = {".###.", "#####", "#####", "#####", ".###."};
// Les ruines : deux pans de mur au nord, un passage entre eux, des piliers sur
// les ailes.
constexpr std::array<std::string_view, 5> kRuins = {
    "###..###", "###..###", "###..###", "#......#", ".#....#.",
};

constexpr std::array<TerrainPieceRule, 7> kPlainsPieces{{
    {.piece_id = 1, .key = "rock", .footprint = kRock, .weight = 9},
    {.piece_id = 2, .key = "tree", .footprint = kTree, .weight = 8},
    {.piece_id = 3, .key = "grove", .footprint = kGrove, .weight = 6},
    {.piece_id = 4, .key = "thicket", .footprint = kThicket, .weight = 12, .walkable = true},
    {.piece_id = 5, .key = "windmill", .footprint = kWindmill, .weight = 1, .landmark = true},
    {.piece_id = 6, .key = "well", .footprint = kWell, .weight = 1, .landmark = true},
    {.piece_id = 7, .key = "ruins", .footprint = kRuins, .weight = 1, .landmark = true},
}};

// La chance d'un emplacement d'avoir une pièce varie de 250 à 625 ‰ d'un
// nœud du bruit de densité à l'autre, un nœud tous les trois morceaux : des
// clairières et des zones plus boisées autour d'une moyenne de 440 ‰. Par
// emplacement hors chemin, on attend 0,11 rocher, 0,10 arbre, 0,075 bosquet et
// 0,15 fourré.
// Une clairière de 12 cases autour du départ, un repère par carré de 5 × 5
// morceaux. Un carrefour tous les trois morceaux, des chemins de quatre cases
// de large qui s'écartent de sept cases au plus de leur droite.
// ── LE DÉSERT DE MORROC : ses pièces ───────────────────────────────────────

// Un palmier seul : son tronc bloque un carré de 2 × 2 cases.
constexpr std::array<std::string_view, 2> kPalm = {"##", "##"};
// Un bouquet : trois palmiers en L.
constexpr std::array<std::string_view, 4> kPalmGrove = {"##..", "##..", "####", "####"};
// Un cactus : son disque de cinq cases, sous tous ses angles.
constexpr std::array<std::string_view, 5> kCactus = {".###.", "#####", "#####", "#####", ".###."};
// Des ossements, franchissables : leur case, et celles qu'ils effleurent.
constexpr std::array<std::string_view, 3> kBones = {".#.", "###", ".#."};
// Un pilier de ruine du désert, à 0,6 de sa taille : son fût de quatre cases,
// et celles qu'il effleure.
constexpr std::array<std::string_view, 6> kRuinPillar = {"..##..", ".####.", "######",
                                                         "######", ".####.", "..##.."};
// Un pan des ruines du désert, le repère : un carré de quatre cases.
constexpr std::array<std::string_view, 4> kDesertRuins = {"####", "####", "####", "####"};

constexpr std::array<TerrainPieceRule, 6> kDesertPieces{{
    {.piece_id = 1, .key = "palm", .footprint = kPalm, .weight = 8},
    {.piece_id = 2, .key = "palm_grove", .footprint = kPalmGrove, .weight = 5},
    {.piece_id = 3, .key = "cactus", .footprint = kCactus, .weight = 6},
    {.piece_id = 4, .key = "bones", .footprint = kBones, .weight = 8, .walkable = true},
    {.piece_id = 5, .key = "ruin_pillar", .footprint = kRuinPillar, .weight = 3},
    {.piece_id = 6,
     .key = "desert_ruins",
     .footprint = kDesertRuins,
     .weight = 1,
     .landmark = true},
}};

constexpr std::array<TerrainRule, 2> kTerrains{{
    {.terrain_id = kPlainsTerrainId,
     .key = "midgard_plains",
     .pieces = kPlainsPieces,
     .density_min_per_mille = 250,
     .density_max_per_mille = 625,
     .density_period_chunks = 3,
     .clearing_cells = 12,
     .landmark_period_chunks = 5,
     .path_period_chunks = 3,
     .path_width_cells = 4,
     .path_wiggle_cells = 7},
    // Le désert est clairsemé : de 120 à 350 ‰, une clairière de 12 cases, un
    // repère par carré de 6 × 6 morceaux, et aucun chemin.
    {.terrain_id = kDesertTerrainId,
     .key = "morroc_desert",
     .pieces = kDesertPieces,
     .density_min_per_mille = 120,
     .density_max_per_mille = 350,
     .density_period_chunks = 3,
     .clearing_cells = 12,
     .landmark_period_chunks = 6},
}};

}  // namespace

std::span<const TerrainRule> ShippedTerrains() { return kTerrains; }

}  // namespace ro::sim::survivors
