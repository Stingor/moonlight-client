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
constexpr std::array<std::string_view, 1> kRuinPillar = {"#"};
constexpr std::array<std::string_view, 6> kFallenRuin = {"..##..", ".####.", "######",
                                                         "######", ".####.", "..##.."};
// Un pan des ruines du désert, le repère : un carré de quatre cases.
constexpr std::array<std::string_view, 4> kDesertRuins = {"####", "####", "####", "####"};

constexpr std::array<TerrainPieceRule, 7> kDesertPieces{{
    {.piece_id = 1, .key = "palm", .footprint = kPalm, .weight = 8},
    {.piece_id = 2, .key = "palm_grove", .footprint = kPalmGrove, .weight = 5},
    {.piece_id = 3, .key = "cactus", .footprint = kCactus, .weight = 6},
    {.piece_id = 4, .key = "bones", .footprint = kBones, .weight = 8, .walkable = true},
    {.piece_id = 5, .key = "ruin_pillar", .footprint = kRuinPillar, .weight = 2},
    {.piece_id = 6,
     .key = "desert_ruins",
     .footprint = kDesertRuins,
     .weight = 1,
     .landmark = true},
    {.piece_id = 7, .key = "fallen_ruin", .footprint = kFallenRuin, .weight = 1},
}};

// ── LES NEIGES DE LUTIE : leurs pièces ─────────────────────────────────────

// Un sapin seul : son tronc bloque un carré de 2 × 2 cases.
constexpr std::array<std::string_view, 2> kFir = {"##", "##"};
// Un bosquet : trois sapins en L.
constexpr std::array<std::string_view, 4> kFirGrove = {"##..", "##..", "####", "####"};
// Un bonhomme de neige, sous tous ses angles.
constexpr std::array<std::string_view, 2> kSnowman = {"##", "##"};
// Une clôture : un mur bas en ligne, de six cases.
constexpr std::array<std::string_view, 1> kFence = {"######"};
// Des cadeaux, franchissables : la place où ils se dessinent.
constexpr std::array<std::string_view, 3> kGifts = {"###", "###", "###"};
// Un traîneau, sous tous ses angles.
constexpr std::array<std::string_view, 2> kSleigh = {"##", "##"};
// Le grand sapin décoré, le repère : son tronc et sa guirlande.
constexpr std::array<std::string_view, 4> kXmasTree = {"####", "####", "####", "####"};

constexpr std::array<TerrainPieceRule, 7> kSnowPieces{{
    {.piece_id = 1, .key = "fir", .footprint = kFir, .weight = 9},
    {.piece_id = 2, .key = "fir_grove", .footprint = kFirGrove, .weight = 6},
    {.piece_id = 3, .key = "snowman", .footprint = kSnowman, .weight = 3},
    {.piece_id = 4, .key = "fence", .footprint = kFence, .weight = 4},
    {.piece_id = 5, .key = "gifts", .footprint = kGifts, .weight = 6, .walkable = true},
    {.piece_id = 6, .key = "sleigh", .footprint = kSleigh, .weight = 1},
    {.piece_id = 7, .key = "xmas_tree", .footprint = kXmasTree, .weight = 1, .landmark = true},
}};

// ── LE LABORATOIRE DE LIGHTHALZEN : ses pièces ─────────────────────────────
// Les modèles des quatre étages du labo (lhz_dun01..04). Les emprises sont le
// solide de leurs modèles (`roc-cli survivors emprises`).

// Une cuve à monstre : la machine et sa cloche de verre, sous tous ses angles.
constexpr std::array<std::string_view, 4> kTank = {"####", "####", "####", "####"};
// Des caisses : trois caisses en L.
constexpr std::array<std::string_view, 4> kCrates = {"##..", "##..", "####", "####"};
// Une cuve brisée et ses éclats.
constexpr std::array<std::string_view, 4> kBrokenTube = {"####", "####", "####", "####"};
// Un tuyau couché, en ligne.
constexpr std::array<std::string_view, 1> kPipe = {"###"};
// Une barricade aux bandes jaunes et noires.
constexpr std::array<std::string_view, 2> kBarricade = {"###", "###"};
// Des béchers, franchissables : la place où ils se dessinent.
constexpr std::array<std::string_view, 3> kBeakers = {"###", "###", "###"};
// La pompe, le repère : son socle, et sa cuve qui s'amincit vers le nord-est.
constexpr std::array<std::string_view, 6> kPump = {"##...", "###..", "####.",
                                                   "#####", "#####", "#####"};

constexpr std::array<TerrainPieceRule, 7> kLabPieces{{
    {.piece_id = kLabTankPieceId, .key = "tank", .footprint = kTank, .weight = 10},
    {.piece_id = 3, .key = "crates", .footprint = kCrates, .weight = 6},
    {.piece_id = 4, .key = "broken_tube", .footprint = kBrokenTube, .weight = 4},
    {.piece_id = 5, .key = "pipe", .footprint = kPipe, .weight = 4},
    {.piece_id = 6, .key = "barricade", .footprint = kBarricade, .weight = 3},
    {.piece_id = 7, .key = "beakers", .footprint = kBeakers, .weight = 6, .walkable = true},
    {.piece_id = 8, .key = "pump", .footprint = kPump, .weight = 1, .landmark = true},
}};

constexpr std::array<TerrainRule, 4> kTerrains{{
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
    // La neige est entre les deux : de 200 à 500 ‰, une clairière de 12 cases,
    // un repère par carré de 5 × 5 morceaux, et les chemins de la prairie,
    // les traces de pas de Lutie.
    {.terrain_id = kSnowTerrainId,
     .key = "lutie_snowfield",
     .pieces = kSnowPieces,
     .density_min_per_mille = 200,
     .density_max_per_mille = 500,
     .density_period_chunks = 3,
     .clearing_cells = 12,
     .landmark_period_chunks = 5,
     .path_period_chunks = 3,
     .path_width_cells = 4,
     .path_wiggle_cells = 7},
    // Le labo est un intérieur : des salles et des blocs de couloirs, des
    // portes de huit cases, un tiers des bords ouverts en halls, un bloc de
    // couloirs pour trois morceaux environ ; de 300 à 600 ‰ de pièces dans
    // les salles, un repère par carré de 5 × 5 morceaux.
    {.terrain_id = kLabTerrainId,
     .key = "lighthalzen_biolab",
     .pieces = kLabPieces,
     .density_min_per_mille = 300,
     .density_max_per_mille = 600,
     .density_period_chunks = 3,
     .clearing_cells = 12,
     .landmark_period_chunks = 5,
     .rooms = true,
     .door_cells = 8,
     .corridor_per_mille = 350,
     .hall_per_mille = 350},
}};

}  // namespace

std::span<const TerrainRule> ShippedTerrains() { return kTerrains; }

}  // namespace ro::sim::survivors
