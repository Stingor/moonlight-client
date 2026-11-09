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

// Le tronc de `나무02` est au sud-est de son pivot.
constexpr std::array<GroundModelChoice, 2> kTrees{{
    {.model = "나무잡초꽃\\나무01.rsm", .scale = 1.0F, .weight = 5, .shade_cells = 3.0F},
    {.model = "나무잡초꽃\\나무02.rsm",
     .scale = 0.9F,
     .weight = 1,
     .shade_cells = 3.0F,
     .centre_east_cells = 0.16F,
     .centre_north_cells = -0.39F},
}};
// Un rocher : la pierre ronde de `바위01`, seule ; la pierre penchée de
// `바위02` ne se pose pas sur la prairie.
constexpr std::array<GroundModelChoice, 1> kRocks{{
    {.model = "나무잡초꽃\\바위01.rsm", .scale = 0.6F, .weight = 1, .shade_cells = 1.2F},
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
// Le pan de mur, jusqu'à la tête du héros, tient dans son carré de 3 × 3.
constexpr std::array<GroundModelChoice, 1> kRuinedWall{{
    {.model = "외부소품\\유적02.rsm", .scale = 0.97F, .weight = 1, .shade_cells = 2.0F},
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
// Le fourré de 4 × 3 : deux buissons serrés, franchissables.
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

// ── LE DÉSERT DE MORROC ─────────────────────────────────────────────────────
// Les modèles des champs de Sograt (moc_fild01) : palmiers, cactus,
// ossements, piliers et pans de ruines du désert. Les emprises sont celles de
// `survivors_rules_terrain.cpp` ; un palmier se pose au milieu du carré de
// 2 × 2 cases que bloque son tronc, comme un arbre de la prairie.
constexpr std::array<GroundModelChoice, 2> kPalms{{
    {.model = "나무잡초꽃\\야자수01.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 3.0F},
    {.model = "나무잡초꽃\\야자수02.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 3.0F},
}};
constexpr std::array<GroundModelChoice, 3> kCacti{{
    {.model = "나무잡초꽃\\선인장01.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 0.8F},
    {.model = "나무잡초꽃\\선인장02.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 0.8F},
    {.model = "나무잡초꽃\\선인장03.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 0.8F},
}};
constexpr std::array<GroundModelChoice, 1> kBonesModel{{
    {.model = "외부소품\\뼈다구04.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 1.0F},
}};
constexpr std::array<GroundModelChoice, 2> kDesertPillars{{
    {.model = "외부소품\\사막유적기둥01.rsm", .scale = 0.6F, .weight = 2, .shade_cells = 0.8F},
    {.model = "외부소품\\사막유적기둥03.rsm", .scale = 0.6F, .weight = 1, .shade_cells = 0.8F},
}};
constexpr std::array<GroundModelChoice, 1> kFallenRuinModel{{
    {.model = "외부소품\\사막유적기둥04.rsm", .scale = 0.6F, .weight = 1, .shade_cells = 0.8F},
}};
constexpr std::array<GroundModelChoice, 1> kDesertWall{{
    {.model = "외부소품\\사막유적02.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 2.0F},
}};

constexpr std::array<PieceModelSlot, 1> kPalmSlots{
    {{.east_cells = 1.0F, .north_cells = 1.0F, .models = kPalms}}};
// Le bouquet en L : un palmier sur chacun de ses trois carrés de 2 × 2.
constexpr std::array<PieceModelSlot, 3> kPalmGroveSlots{{
    {.east_cells = 1.0F, .north_cells = 3.0F, .models = kPalms},
    {.east_cells = 1.0F, .north_cells = 1.0F, .models = kPalms},
    {.east_cells = 3.0F, .north_cells = 1.0F, .models = kPalms},
}};
constexpr std::array<PieceModelSlot, 1> kCactusSlots{
    {{.east_cells = 2.5F, .north_cells = 2.5F, .models = kCacti}}};
constexpr std::array<PieceModelSlot, 1> kBonesSlots{
    {{.east_cells = 1.5F, .north_cells = 1.5F, .models = kBonesModel}}};
constexpr std::array<PieceModelSlot, 1> kRuinPillarSlots{
    {{.east_cells = 0.5F, .north_cells = 0.5F, .models = kDesertPillars}}};
constexpr std::array<PieceModelSlot, 1> kFallenRuinSlots{
    {{.east_cells = 3.0F, .north_cells = 3.0F, .models = kFallenRuinModel}}};
// Le repère : un pan des ruines du désert, au milieu de son carré de 4 × 4.
constexpr std::array<PieceModelSlot, 1> kDesertRuinsSlots{
    {{.east_cells = 2.0F, .north_cells = 2.0F, .models = kDesertWall, .any_yaw = false}}};

constexpr std::array<PieceLook, 7> kDesertPieces{{
    {.key = "palm", .slots = kPalmSlots},
    {.key = "palm_grove", .slots = kPalmGroveSlots},
    {.key = "cactus", .slots = kCactusSlots},
    {.key = "bones", .slots = kBonesSlots},
    {.key = "ruin_pillar", .slots = kRuinPillarSlots},
    {.key = "fallen_ruin", .slots = kFallenRuinSlots},
    {.key = "desert_ruins", .slots = kDesertRuinsSlots},
}};

// Le sable plein de Sograt, `moc_맨땅08`, le seul de la série qui se raccorde à
// lui-même : les autres sont des bords de rocher ou de sable clair, dessinés
// pour leurs voisins, et quadrillent le sol posés seuls ; la tache claire de
// `moc_맨땅06` y paraît en carré, son sable n'ayant pas la teinte de `08`.
// Éparses, un rocher et les deux fossiles, tous trois sur le même sable. Le
// désert n'a ni taches sous ses pièces ni chemins : leurs carreaux sont le
// sable lui-même.
constexpr std::array<GroundTextureLook, 7> kDesertTextures{{
    {.texture = "필드바닥\\moc_맨땅08.bmp"},
    {.texture = "필드바닥\\moc_맨땅04.bmp", .role = GroundTextureRole::kScatter},
    {.texture = "필드바닥\\moc_화석01.bmp", .role = GroundTextureRole::kScatter, .weight = 2},
    {.texture = "필드바닥\\moc_화석02.bmp", .role = GroundTextureRole::kScatter},
    {.texture = "필드바닥\\moc_맨땅08.bmp", .role = GroundTextureRole::kPathFull},
    {.texture = "필드바닥\\moc_맨땅08.bmp", .role = GroundTextureRole::kPathEdge},
    {.texture = "필드바닥\\moc_맨땅08.bmp", .role = GroundTextureRole::kPathOuter},
}};

// Le semis : de petits cactus et quelques os épars.
constexpr float kSmallCactusScale = 0.5F;
constexpr std::array<SeedLook, 2> kDesertSeeds{{
    {.model = "나무잡초꽃\\선인장03.rsm", .scale = kSmallCactusScale, .weight = 3},
    {.model = "외부소품\\뼈다구04.rsm", .scale = kSmallCactusScale, .weight = 1},
}};

// ── LES NEIGES DE LUTIE ─────────────────────────────────────────────────────
// Les modèles du champ de Lutie (xmas_fild01) : sapins, bonshommes de neige,
// clôtures, cadeaux et traîneau. Les emprises sont celles de
// `survivors_rules_terrain.cpp` ; un sapin se pose au milieu du carré de 2 × 2
// cases que bloque son tronc. Il se pose au double de sa taille : plus petit,
// ses branches basses descendent sous la tête du héros et murent 5 × 5 cases.
constexpr std::array<GroundModelChoice, 2> kFirs{{
    {.model = "크리스마스마을\\xmas_나무1.rsm", .scale = 2.0F, .weight = 2, .shade_cells = 3.0F},
    {.model = "크리스마스마을\\xmas_나무2.rsm", .scale = 2.0F, .weight = 3, .shade_cells = 3.0F},
}};
// Les bonshommes de neige suivent le quart de tour de leur pièce : de face,
// ils tiennent dans leur carré de 2 × 2.
constexpr std::array<GroundModelChoice, 2> kSnowmen{{
    {.model = "크리스마스마을\\xmas_눈사람2.rsm", .scale = 0.75F, .weight = 1, .shade_cells = 1.2F},
    {.model = "크리스마스마을\\xmas_눈사람4.rsm", .scale = 0.6F, .weight = 1, .shade_cells = 1.2F},
}};
constexpr std::array<GroundModelChoice, 1> kFenceModel{{
    {.model = "크리스마스마을\\xmas_울타리.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 1.0F},
}};
constexpr std::array<GroundModelChoice, 2> kGiftModels{{
    {.model = "크리스마스마을\\xmas_상자1.rsm", .scale = 0.6F, .weight = 2, .shade_cells = 0.6F},
    {.model = "크리스마스마을\\xmas_상자2.rsm", .scale = 0.6F, .weight = 1, .shade_cells = 0.6F},
}};
// Les paquets au pied du sapin du repère, en retrait du bord de son emprise.
constexpr std::array<GroundModelChoice, 1> kGreatGift{{
    {.model = "크리스마스마을\\xmas_상자1.rsm", .scale = 0.9F, .weight = 1, .shade_cells = 0.8F},
}};
constexpr std::array<GroundModelChoice, 1> kSleighModel{{
    {.model = "크리스마스마을\\xmas_썰매1.rsm", .scale = 0.66F, .weight = 1, .shade_cells = 1.2F},
}};
// Le sapin du repère, que ses paquets distinguent.
constexpr std::array<GroundModelChoice, 1> kGreatFir{{
    {.model = "크리스마스마을\\xmas_나무1.rsm", .scale = 2.0F, .weight = 1, .shade_cells = 4.0F},
}};

constexpr std::array<PieceModelSlot, 1> kFirSlots{
    {{.east_cells = 1.0F, .north_cells = 1.0F, .models = kFirs}}};
constexpr std::array<PieceModelSlot, 3> kFirGroveSlots{{
    {.east_cells = 1.0F, .north_cells = 3.0F, .models = kFirs},
    {.east_cells = 1.0F, .north_cells = 1.0F, .models = kFirs},
    {.east_cells = 3.0F, .north_cells = 1.0F, .models = kFirs},
}};
constexpr std::array<PieceModelSlot, 1> kSnowmanSlots{
    {{.east_cells = 1.0F, .north_cells = 1.0F, .models = kSnowmen, .any_yaw = false}}};
// La clôture suit le quart de tour de sa pièce, couchée le long de sa ligne.
constexpr std::array<PieceModelSlot, 1> kFenceSlots{
    {{.east_cells = 3.0F, .north_cells = 0.5F, .models = kFenceModel, .any_yaw = false}}};
// Les cadeaux de 3 × 3 : trois paquets épars, franchissables.
constexpr std::array<PieceModelSlot, 3> kGiftSlots{{
    {.east_cells = 0.8F, .north_cells = 0.8F, .models = kGiftModels},
    {.east_cells = 2.2F, .north_cells = 1.2F, .models = kGiftModels},
    {.east_cells = 1.3F, .north_cells = 2.3F, .models = kGiftModels},
}};
constexpr std::array<PieceModelSlot, 1> kSleighSlots{
    {{.east_cells = 1.0F, .north_cells = 1.0F, .models = kSleighModel, .any_yaw = false}}};
// Le repère de 4 × 4 : le grand sapin au milieu, un paquet à chaque coin.
constexpr std::array<PieceModelSlot, 5> kXmasTreeSlots{{
    {.east_cells = 2.0F, .north_cells = 2.0F, .models = kGreatFir},
    {.east_cells = 0.8F, .north_cells = 0.8F, .models = kGreatGift, .any_yaw = false},
    {.east_cells = 3.2F, .north_cells = 0.8F, .models = kGreatGift, .any_yaw = false},
    {.east_cells = 0.8F, .north_cells = 3.2F, .models = kGreatGift, .any_yaw = false},
    {.east_cells = 3.2F, .north_cells = 3.2F, .models = kGreatGift, .any_yaw = false},
}};

constexpr std::array<PieceLook, 7> kSnowPieces{{
    {.key = "fir", .slots = kFirSlots},
    {.key = "fir_grove", .slots = kFirGroveSlots},
    {.key = "snowman", .slots = kSnowmanSlots},
    {.key = "fence", .slots = kFenceSlots},
    {.key = "gifts", .slots = kGiftSlots},
    {.key = "sleigh", .slots = kSleighSlots},
    {.key = "xmas_tree", .slots = kXmasTreeSlots},
}};

// La neige pleine `xmas_눈바닥01` se raccorde à elle-même. `05`, une tache de
// neige tassée au milieu de son carreau, se pose sous les pièces ; `02`, des
// traces légères, et `06`, des touffes d'herbe sous la neige, s'éparpillent.
// `03` est une bande et `04` un bord en biais : posés seuls, ils
// quadrilleraient le sol. Les chemins sont la neige pleine, comme le sable du
// désert.
constexpr std::array<GroundTextureLook, 8> kSnowTextures{{
    {.texture = "필드바닥\\xmas_눈바닥01.bmp"},
    {.texture = "필드바닥\\xmas_눈바닥05.bmp", .role = GroundTextureRole::kPatch},
    {.texture = "필드바닥\\xmas_눈바닥02.bmp", .role = GroundTextureRole::kScatter, .weight = 2},
    {.texture = "필드바닥\\xmas_눈바닥06.bmp", .role = GroundTextureRole::kScatter},
    {.texture = "필드바닥\\xmas_눈바닥01.bmp", .role = GroundTextureRole::kPathFull},
    {.texture = "필드바닥\\xmas_눈바닥01.bmp", .role = GroundTextureRole::kPathEdge},
    {.texture = "필드바닥\\xmas_눈바닥01.bmp", .role = GroundTextureRole::kPathOuter},
    {.texture = "필드바닥\\xmas_눈바닥01.bmp", .role = GroundTextureRole::kPathInner},
}};

// Le semis : l'herbe sous la neige, et quelques boules de Noël tombées. Une
// boule mesure près de quatre cases à l'échelle 1 : c'est une décoration de
// sapin, que les cartes de Lutie posent entre 0,04 et 0,24. Tombée au sol,
// elle garde la taille d'une boule du grand sapin, sous la case.
constexpr std::array<SeedLook, 3> kSnowSeeds{{
    {.model = "크리스마스마을\\xmas_풀.rsm", .scale = 0.6F, .weight = 6},
    {.model = "크리스마스마을\\xmas_구슬2.rsm", .scale = 0.2F, .weight = 1},
    {.model = "크리스마스마을\\xmas_구슬3.rsm", .scale = 0.24F, .weight = 1},
}};

constexpr std::array<BiomeLook, 3> kBiomes{{
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
    {.terrain_key = "morroc_desert",
     .reference_map = "moc_fild01",
     .display_name = ro::core::TrText("Désert de Sograt"),
     .sign_background = "field_s2",
     .field_track = "bgm\\24.mp3",
     .textures = kDesertTextures,
     .pieces = kDesertPieces,
     .seeds = kDesertSeeds,
     .seeds_per_chunk = 6,
     .patch_per_mille = 0,
     .scatter_per_mille = 20},
    {.terrain_key = "lutie_snowfield",
     .reference_map = "xmas_fild01",
     .display_name = ro::core::TrText("Neiges de Lutie"),
     .sign_background = "field_s2",
     .field_track = "bgm\\55.mp3",
     .textures = kSnowTextures,
     .pieces = kSnowPieces,
     .seeds = kSnowSeeds,
     .seeds_per_chunk = 8,
     .patch_per_mille = 500,
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
