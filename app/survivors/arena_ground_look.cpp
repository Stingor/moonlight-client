#include "survivors/arena_ground_look.h"

#include <array>

namespace moonclient {
namespace {

// ── LA PRAIRIE DE MIDGARD ───────────────────────────────────────────────────
// Un catalogue : chaque entrée est courte et se lit seule. Les places des
// modèles se mesurent sur les emprises de `survivors_rules_terrain.cpp`, le
// nord en haut ; un arbre se pose au milieu du carré de 2 × 2 cases que bloque
// son tronc.

constexpr std::array<GroundModelChoice, 2> kTrees{{
    {.model = "나무잡초꽃\\나무01.rsm", .scale = 1.0F, .weight = 5, .shade_cells = 3.0F},
    {.model = "나무잡초꽃\\나무02.rsm", .scale = 0.9F, .weight = 1, .shade_cells = 3.0F},
}};
constexpr std::array<GroundModelChoice, 2> kRocks{{
    {.model = "나무잡초꽃\\바위01.rsm", .scale = 0.7F, .weight = 3, .shade_cells = 1.4F},
    {.model = "나무잡초꽃\\바위02.rsm", .scale = 0.7F, .weight = 1, .shade_cells = 1.4F},
}};
constexpr std::array<GroundModelChoice, 2> kBushes{{
    {.model = "나무잡초꽃\\덤불01.rsm", .scale = 0.3F, .weight = 1, .shade_cells = 1.2F},
    {.model = "나무잡초꽃\\덤불02.rsm", .scale = 0.3F, .weight = 1, .shade_cells = 1.2F},
}};
constexpr std::array<GroundModelChoice, 1> kWindmill{{
    {.model = "외부소품\\풍차.rsm", .scale = 0.4F, .weight = 1, .shade_cells = 4.0F},
}};
constexpr std::array<GroundModelChoice, 1> kWell{{
    {.model = "외부소품\\우물.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 2.0F},
}};
constexpr std::array<GroundModelChoice, 1> kRuinedWall{{
    {.model = "외부소품\\유적02.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 2.0F},
}};
constexpr std::array<GroundModelChoice, 2> kRuinedPillars{{
    {.model = "외부소품\\유적기둥01.rsm", .scale = 1.0F, .weight = 2, .shade_cells = 0.8F},
    {.model = "외부소품\\유적기둥03.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 0.8F},
}};

// Un arbre, un rocher : 2 × 2 cases.
constexpr std::array<PieceModelSlot, 1> kTreeSlots{
    {{.east_cells = 1.0F, .north_cells = 1.0F, .models = kTrees}}};
constexpr std::array<PieceModelSlot, 1> kRockSlots{
    {{.east_cells = 1.0F, .north_cells = 1.0F, .models = kRocks}}};
// Le bosquet en L : un arbre sur chacun de ses trois carrés de 2 × 2.
constexpr std::array<PieceModelSlot, 3> kGroveSlots{{
    {.east_cells = 1.0F, .north_cells = 3.0F, .models = kTrees},
    {.east_cells = 1.0F, .north_cells = 1.0F, .models = kTrees},
    {.east_cells = 3.0F, .north_cells = 1.0F, .models = kTrees},
}};
// Le massif de 3 × 2 : deux buissons.
constexpr std::array<PieceModelSlot, 2> kThicketSlots{{
    {.east_cells = 0.8F, .north_cells = 1.0F, .models = kBushes},
    {.east_cells = 2.2F, .north_cells = 1.0F, .models = kBushes},
}};
// Les repères suivent le quart de tour de leur pièce.
constexpr std::array<PieceModelSlot, 1> kWindmillSlots{
    {{.east_cells = 3.0F, .north_cells = 3.0F, .models = kWindmill, .any_yaw = false}}};
constexpr std::array<PieceModelSlot, 1> kWellSlots{
    {{.east_cells = 1.5F, .north_cells = 1.5F, .models = kWell, .any_yaw = false}}};
// Les ruines : le mur du nord aux deux coins, des piliers sur les ailes.
constexpr std::array<PieceModelSlot, 6> kRuinsSlots{{
    {.east_cells = 1.5F, .north_cells = 3.5F, .models = kRuinedWall, .any_yaw = false},
    {.east_cells = 6.5F, .north_cells = 3.5F, .models = kRuinedWall, .any_yaw = false},
    {.east_cells = 0.5F, .north_cells = 1.5F, .models = kRuinedPillars},
    {.east_cells = 7.5F, .north_cells = 1.5F, .models = kRuinedPillars},
    {.east_cells = 1.0F, .north_cells = 0.5F, .models = kRuinedPillars},
    {.east_cells = 7.0F, .north_cells = 0.5F, .models = kRuinedPillars},
}};

constexpr std::array<PieceLook, 7> kPlainsPieces{{
    {.key = "rock", .slots = kRockSlots},
    {.key = "tree", .slots = kTreeSlots},
    {.key = "grove", .slots = kGroveSlots},
    {.key = "thicket", .slots = kThicketSlots},
    {.key = "windmill", .slots = kWindmillSlots},
    {.key = "well", .slots = kWellSlots},
    {.key = "ruins", .slots = kRuinsSlots},
}};

// L'herbe des champs de Prontera : `prt_초원01` se raccorde à elle-même ;
// `prt_초원04` est une tache de terre ronde bordée d'herbe, qui tourne sans
// couture. Les autres textures des champs (chemins, pierriers, falaises, la
// tache carrée de `prt_초원05`) sont des bords dessinés pour leurs voisines, ou
// trahissent la grille, et ne se posent pas seules.
constexpr std::array<GroundTextureLook, 3> kPlainsTextures{{
    {.texture = "필드바닥\\prt_초원01.bmp"},
    {.texture = "필드바닥\\prt_초원04.bmp", .role = GroundTextureRole::kPatch},
    {.texture = "필드바닥\\prt_초원04.bmp", .role = GroundTextureRole::kScatter},
}};

constexpr std::array<SeedLook, 6> kPlainsSeeds{{
    {.model = "나무잡초꽃\\버섯01.rsm", .weight = 3},
    {.model = "나무잡초꽃\\버섯02.rsm", .weight = 3},
    {.model = "나무잡초꽃\\버섯03.rsm", .weight = 2},
    {.model = "나무잡초꽃\\나무조각01.rsm", .weight = 2},
    {.model = "나무잡초꽃\\나무조각02.rsm", .weight = 3},
    {.model = "나무잡초꽃\\덤불06.rsm", .scale = 0.8F, .weight = 4},
}};

constexpr std::array<BiomeLook, 1> kBiomes{{
    {.terrain_key = "midgard_plains",
     .reference_map = "prt_fild08",
     .textures = kPlainsTextures,
     .pieces = kPlainsPieces,
     .seeds = kPlainsSeeds,
     .seeds_per_chunk = 12,
     .patch_per_mille = 800,
     .scatter_per_mille = 40},
}};

}  // namespace

const BiomeLook* FindBiomeLook(std::string_view terrain_key) {
  for (const BiomeLook& biome : kBiomes) {
    if (biome.terrain_key == terrain_key) return &biome;
  }
  return nullptr;
}

const PieceLook* FindPieceLook(const BiomeLook& biome, std::string_view key) {
  for (const PieceLook& piece : biome.pieces) {
    if (piece.key == key) return &piece;
  }
  return nullptr;
}

}  // namespace moonclient
