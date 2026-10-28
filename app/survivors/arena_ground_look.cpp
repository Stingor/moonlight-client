#include "survivors/arena_ground_look.h"

#include <array>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

// ── LA PRAIRIE DE MIDGARD ───────────────────────────────────────────────────
// Un catalogue : chaque entrée est courte et se lit seule. Les places des
// modèles se mesurent sur les emprises de `survivors_rules_terrain.cpp`, le
// nord en haut : le solide de chaque modèle, sous tous ses angles, tombe dans
// l'emprise de sa pièce (`roc-cli survivors emprises`, essai
// `arena_piece_solids_test`). Un arbre se pose au milieu du carré de 2 × 2
// cases que bloque son tronc.

constexpr std::array<GroundModelChoice, 2> kTrees{{
    {.model = "나무잡초꽃\\나무01.rsm", .scale = 1.0F, .weight = 5, .shade_cells = 3.0F},
    {.model = "나무잡초꽃\\나무02.rsm", .scale = 0.9F, .weight = 1, .shade_cells = 3.0F},
}};
// La pierre penchée de `바위02` a son pied à l'ouest de son pivot.
constexpr std::array<GroundModelChoice, 2> kRocks{{
    {.model = "나무잡초꽃\\바위01.rsm", .scale = 0.6F, .weight = 3, .shade_cells = 1.2F},
    {.model = "나무잡초꽃\\바위02.rsm",
     .scale = 0.6F,
     .weight = 1,
     .shade_cells = 1.2F,
     .centre_east_cells = -0.69F},
}};
constexpr std::array<GroundModelChoice, 2> kBushes{{
    {.model = "나무잡초꽃\\덤불01.rsm", .scale = 0.25F, .weight = 1, .shade_cells = 1.0F},
    {.model = "나무잡초꽃\\덤불02.rsm", .scale = 0.25F, .weight = 1, .shade_cells = 1.0F},
}};
constexpr std::array<GroundModelChoice, 1> kWindmill{{
    {.model = "외부소품\\풍차.rsm",
     .scale = 0.4F,
     .weight = 1,
     .shade_cells = 4.0F,
     .centre_east_cells = -0.08F,
     .centre_north_cells = 0.41F},
}};
// Le fond du puits est à 5 unités sous le pied de sa margelle.
constexpr std::array<GroundModelChoice, 1> kWell{{
    {.model = "외부소품\\우물.rsm",
     .scale = 1.0F,
     .weight = 1,
     .shade_cells = 2.0F,
     .sink_units = 5.0F},
}};
constexpr std::array<GroundModelChoice, 1> kRuinedWall{{
    {.model = "외부소품\\유적02.rsm",
     .scale = 1.0F,
     .weight = 1,
     .shade_cells = 2.0F,
     .centre_north_cells = -0.12F},
}};
constexpr std::array<GroundModelChoice, 2> kRuinedPillars{{
    {.model = "외부소품\\유적기둥01.rsm", .scale = 1.0F, .weight = 2, .shade_cells = 0.8F},
    {.model = "외부소품\\유적기둥03.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 0.8F},
}};

// Un arbre : 2 × 2 cases ; un rocher : 4 × 4.
constexpr std::array<PieceModelSlot, 1> kTreeSlots{
    {{.east_cells = 1.0F, .north_cells = 1.0F, .models = kTrees}}};
constexpr std::array<PieceModelSlot, 1> kRockSlots{
    {{.east_cells = 2.0F, .north_cells = 2.0F, .models = kRocks}}};
// Le bosquet en L : un arbre sur chacun de ses trois carrés de 2 × 2.
constexpr std::array<PieceModelSlot, 3> kGroveSlots{{
    {.east_cells = 1.0F, .north_cells = 3.0F, .models = kTrees},
    {.east_cells = 1.0F, .north_cells = 1.0F, .models = kTrees},
    {.east_cells = 3.0F, .north_cells = 1.0F, .models = kTrees},
}};
// Le massif de 4 × 3 : deux buissons serrés.
constexpr std::array<PieceModelSlot, 2> kThicketSlots{{
    {.east_cells = 1.5F, .north_cells = 1.5F, .models = kBushes},
    {.east_cells = 2.5F, .north_cells = 1.5F, .models = kBushes},
}};
// Les repères suivent le quart de tour de leur pièce, au milieu de leur
// emprise : le pied de la tour du moulin, de 8 × 7, la margelle du puits, de
// 5 × 5.
constexpr std::array<PieceModelSlot, 1> kWindmillSlots{
    {{.east_cells = 4.0F, .north_cells = 3.5F, .models = kWindmill, .any_yaw = false}}};
constexpr std::array<PieceModelSlot, 1> kWellSlots{
    {{.east_cells = 2.5F, .north_cells = 2.5F, .models = kWell, .any_yaw = false}}};
// Les ruines de 8 × 5 : un pan de mur à chaque coin du nord, un pilier au
// milieu d'une case sur chaque aile.
constexpr std::array<PieceModelSlot, 6> kRuinsSlots{{
    {.east_cells = 1.5F, .north_cells = 3.5F, .models = kRuinedWall, .any_yaw = false},
    {.east_cells = 6.5F, .north_cells = 3.5F, .models = kRuinedWall, .any_yaw = false},
    {.east_cells = 0.5F, .north_cells = 1.5F, .models = kRuinedPillars},
    {.east_cells = 7.5F, .north_cells = 1.5F, .models = kRuinedPillars},
    {.east_cells = 1.5F, .north_cells = 0.5F, .models = kRuinedPillars},
    {.east_cells = 6.5F, .north_cells = 0.5F, .models = kRuinedPillars},
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
// couture. Les pierriers, les falaises et la tache carrée de `prt_초원05` sont
// des bords dessinés pour leurs voisines, ou trahissent la grille, et ne se
// posent pas seuls.
//
// Les chemins prennent les carreaux des chemins des champs : la terre de
// `prt_흙02` ; la bande de terre de `prt_초원02`, à cheval sur ses colonnes
// ouest et est : la colonne 0, terre à l'ouest et herbe à l'est, et la
// colonne 3, l'inverse, lisière au milieu ; le cube (1, 2) de `prt_초원03`,
// terre au seul coin nord-ouest ; le cube (1, 1) de `prt_초원04`, herbe au
// seul coin sud-ouest, lisière en biais.
constexpr std::array<GroundTextureLook, 7> kPlainsTextures{{
    {.texture = "필드바닥\\prt_초원01.bmp"},
    {.texture = "필드바닥\\prt_초원04.bmp", .role = GroundTextureRole::kPatch},
    {.texture = "필드바닥\\prt_초원04.bmp", .role = GroundTextureRole::kScatter},
    {.texture = "필드바닥\\prt_흙02.bmp", .role = GroundTextureRole::kPathFull},
    {.texture = "필드바닥\\prt_초원02.bmp",
     .role = GroundTextureRole::kPathEdge,
     .east_quarter_u = 3},
    {.texture = "필드바닥\\prt_초원03.bmp",
     .role = GroundTextureRole::kPathOuter,
     .quarter_u = 1,
     .quarter_v = 2},
    {.texture = "필드바닥\\prt_초원04.bmp",
     .role = GroundTextureRole::kPathInner,
     .quarter_u = 1,
     .quarter_v = 1},
}};

// LES CHAMPIGNONS se posent à 0,65 de leur taille : entier, le plus
// voyant, l'orange, est presque aussi large que le héros (les trois modèles ont
// la même forme).
constexpr float kMushroomScale = 0.65F;

// Le long des chemins : des touffes, des buissons bas, un champignon sur huit.
constexpr std::array<SeedLook, 3> kPlainsVergeSeeds{{
    {.model = "나무잡초꽃\\덤불06.rsm", .scale = 0.8F, .weight = 4},
    {.model = "나무잡초꽃\\덤불05.rsm", .scale = 0.6F, .weight = 3},
    {.model = "나무잡초꽃\\버섯03.rsm", .scale = kMushroomScale, .weight = 1},
}};

// Un champignon sur quatre ; souches et touffes font le reste.
constexpr std::array<SeedLook, 6> kPlainsSeeds{{
    {.model = "나무잡초꽃\\버섯01.rsm", .scale = kMushroomScale, .weight = 1},
    {.model = "나무잡초꽃\\버섯02.rsm", .scale = kMushroomScale, .weight = 1},
    {.model = "나무잡초꽃\\버섯03.rsm", .scale = kMushroomScale, .weight = 1},
    {.model = "나무잡초꽃\\나무조각01.rsm", .weight = 2},
    {.model = "나무잡초꽃\\나무조각02.rsm", .weight = 3},
    {.model = "나무잡초꽃\\덤불06.rsm", .scale = 0.8F, .weight = 4},
}};

constexpr std::array<BiomeLook, 1> kBiomes{{
    {.terrain_key = "midgard_plains",
     .reference_map = "prt_fild08",
     .display_name = ro::core::TrText("Prairie de Midgard"),
     .sign_background = "field_s2",
     .field_track = "bgm\\12.mp3",
     .textures = kPlainsTextures,
     .pieces = kPlainsPieces,
     .seeds = kPlainsSeeds,
     .seeds_per_chunk = 12,
     .verge_seeds = kPlainsVergeSeeds,
     .verge_per_mille = 120,
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
