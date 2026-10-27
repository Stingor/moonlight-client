#include <array>
#include <span>
#include <string_view>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

// ── LA PRAIRIE DE MIDGARD : les pièces du sol généré et leurs densités ─────
// Une emprise s'écrit le nord en haut, « # » bloqué. Elle est le CŒUR que le
// GAT des champs de Prontera bloque sous le modèle, pas sa boîte : un arbre
// bloque un carré de 2 × 2 cases sous son tronc (`roc-cli survivors pieces`,
// ADR 0134). Le modèle posé sur chaque pièce est l'affaire d'app.

// Un arbre seul.
constexpr std::array<std::string_view, 2> kTree = {"##", "##"};
// Un rocher.
constexpr std::array<std::string_view, 2> kRock = {"##", "##"};
// Un bosquet : trois arbres en L.
constexpr std::array<std::string_view, 4> kGrove = {"##..", "##..", "####", "####"};
// Un massif de buissons.
constexpr std::array<std::string_view, 2> kThicket = {"###", "###"};
// Les repères, un par grand emplacement élu.
constexpr std::array<std::string_view, 6> kWindmill = {
    "..##..", ".####.", "######", "######", ".####.", "..##..",
};
constexpr std::array<std::string_view, 3> kWell = {"###", "###", "###"};
constexpr std::array<std::string_view, 4> kRuins = {
    "########",
    "#......#",
    "#......#",
    "##....##",
};

constexpr std::array<TerrainPieceRule, 7> kPlainsPieces{{
    {.piece_id = 1, .key = "rock", .footprint = kRock, .weight = 3},
    {.piece_id = 2, .key = "tree", .footprint = kTree, .weight = 4},
    {.piece_id = 3, .key = "grove", .footprint = kGrove, .weight = 3},
    {.piece_id = 4, .key = "thicket", .footprint = kThicket, .weight = 2},
    {.piece_id = 5, .key = "windmill", .footprint = kWindmill, .weight = 1, .landmark = true},
    {.piece_id = 6, .key = "well", .footprint = kWell, .weight = 1, .landmark = true},
    {.piece_id = 7, .key = "ruins", .footprint = kRuins, .weight = 1, .landmark = true},
}};

// La chance d'un emplacement d'avoir une pièce varie de 800 à 1 000 ‰ d'un
// nœud du bruit de densité à l'autre, un nœud tous les trois morceaux : des
// zones plus ou moins boisées, environ 8,5 % de cases bloquées en moyenne
// (une prairie à bosquets : de 8 à 12 %).
// Une clairière de 12 cases autour du départ, un repère par carré de 5 × 5
// morceaux.
constexpr std::array<TerrainRule, 1> kTerrains{{
    {.terrain_id = kPlainsTerrainId,
     .key = "midgard_plains",
     .pieces = kPlainsPieces,
     .density_min_per_mille = 800,
     .density_max_per_mille = 1000,
     .density_period_chunks = 3,
     .clearing_cells = 12,
     .landmark_period_chunks = 5},
}};

}  // namespace

std::span<const TerrainRule> ShippedTerrains() { return kTerrains; }

}  // namespace ro::sim::survivors
