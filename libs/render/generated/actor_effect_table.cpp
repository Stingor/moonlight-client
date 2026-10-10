// CE QU'UN EFFET CHANGE SUR L'ACTEUR, au lieu de dessiner.
//
// 137 identifiants : ceux que le tri des cas sans bâtisseur range sur l'acteur,
// et 48 que l'entonnoir de la teinte et du mot de rendu y ajoute.
//
// Les bits sont ceux du mot de drapeaux de rendu de l'acteur.
//
// ⚠ QUI BAISSE UN BIT : L'ACTEUR, À CHAQUE IMAGE. Aucune écriture du mot
// ne retire de bit, mais l'acteur remet le mot à zéro au premier pas qui
// suit chacun de ses dessins, et les instances d'effet vivantes le
// relèvent ensuite. Un bit vit tant que son instance le relève : jusqu'à
// sa vie, ou jusqu'à la mort de l'instance pour ceux que le cas lève
// avant de comparer son âge — la dernière colonne.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_actor_effects.py depuis le relevé
// scripts/client_effect_actor_changes.json, pas écrit à la main.

#include "ro/render/actor_effect_table.h"

#include <array>

namespace ro::render {
namespace {

// Triée par identifiant d'effet : la recherche est dichotomique.
//  69 kTint
//  45 kRenderFlag
//   8 kSpriteClone
//   7 kOrder
//   7 kAppearance
//   1 kSpin
//
// ⚠ LE FORMATAGE EST COUPÉ SUR CE TABLEAU, et c'est délibéré. À cent
// colonnes, clang-format étale chaque effet sur huit lignes : la table
// passe de quatre-vingt-six lignes à sept cents, et le plafond de fichier
// se met à parler d'un catalogue qui n'a pas grandi. Une entrée par ligne
// se lit aussi mieux — on y compare des colonnes.
// clang-format off
constexpr std::array<ActorEffectChange, 137> kActorEffectChanges = {{
    {120, ActorEffectFamily::kTint, 0x0, {50, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "EffectUpdate_CloakingAlpha50", 0},
    {166, ActorEffectFamily::kSpriteClone, 0x0, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "AfterimageGhost_Update", 5},
    {220, ActorEffectFamily::kTint, 0x0, {255, 250, 120, 120}, {}, 151, 0, -1, 0, 0x0, 0, "Actor_OnMsg_AppearanceEffects", 0},
    {295, ActorEffectFamily::kOrder, 0x0, {-1, -1, -1, -1}, {}, 0, 165, -1, 0, 0x0, 0, "EffectInst_DispatchUICommand_A5", 0},
    {296, ActorEffectFamily::kOrder, 0x0, {-1, -1, -1, -1}, {}, 0, 165, -1, 0, 0x0, 0, "EffectInst_DispatchUICommand_A5", 0},
    {303, ActorEffectFamily::kTint, 0x8, {-1, 255, 255, 150}, {}, 121, 122, 44, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 20},
    {361, ActorEffectFamily::kTint, 0x1, {-1, 255, 0, 255}, {}, 11, 0, -1, 0, 0x0, 0, "", 0},
    {366, ActorEffectFamily::kTint, 0x8, {-1, 255, 200, 200}, {}, 41, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 20},
    {368, ActorEffectFamily::kTint, 0x1, {-1, 255, 100, 100}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {369, ActorEffectFamily::kTint, 0x0, {-1, 255, 255, 160}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {375, ActorEffectFamily::kRenderFlag, 0x8, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {376, ActorEffectFamily::kTint, 0x8, {-1, 255, 120, 50}, {}, 21, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 10},
    {377, ActorEffectFamily::kRenderFlag, 0x1, {-1, -1, -1, -1}, {}, 36, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 5},
    {378, ActorEffectFamily::kTint, 0x1, {-1, 100, 100, 255}, {}, 66, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 55},
    {379, ActorEffectFamily::kTint, 0x0, {-1, -1, -1, 255}, {}, 201, 0, -1, 0, 0x0, 0, "", 0},
    {380, ActorEffectFamily::kTint, 0x1, {-1, -2, -2, -2}, {}, 61, 122, 30, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 30},
    {382, ActorEffectFamily::kRenderFlag, 0x4, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {383, ActorEffectFamily::kTint, 0x8, {-1, 250, 250, 250}, {}, 81, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 50, 2, {0, -2, -2, 0}},
    {385, ActorEffectFamily::kTint, 0x1, {-1, 200, 150, 50}, {}, 71, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 40},
    {387, ActorEffectFamily::kRenderFlag, 0x1, {-1, -1, -1, -1}, {ActorFieldWrite{0x4C, 0, false, false, 0, 0, -1, -1}, ActorFieldWrite{0x4C, 0, false, false, 0, 0, -1, -1}}, 51, 122, 30, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 30},
    {389, ActorEffectFamily::kTint, 0x1, {-1, 220, 250, 220}, {}, 26, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 5},
    {392, ActorEffectFamily::kTint, 0x8, {-1, 100, 100, 100}, {}, 30, 0, -1, 0, 0x0, 0, "", 0, {-1, 250, 250, 250}, 0, 6},
    {396, ActorEffectFamily::kTint, 0x9, {-1, 255, 89, 182}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {399, ActorEffectFamily::kTint, 0x8, {-1, 255, 255, 200}, {}, 51, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 20},
    {400, ActorEffectFamily::kTint, 0x8, {-1, 255, 255, 255}, {}, 51, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 20},
    {402, ActorEffectFamily::kTint, 0x1, {-1, 250, 250, 200}, {}, 66, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 5},
    {403, ActorEffectFamily::kTint, 0x0, {-1, -2, -2, -2}, {}, 61, 0, -1, 0, 0x0, 0, "", 0},
    {409, ActorEffectFamily::kTint, 0x1, {-1, 255, 0, 255}, {}, 11, 0, -1, 0, 0x0, 0, "", 0},
    {413, ActorEffectFamily::kRenderFlag, 0x20, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, true, 1, -17, -1, -1}}, 51, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 17},
    {414, ActorEffectFamily::kRenderFlag, 0x40, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, true, 2, 0, -1, -1}}, 18, 0, -1, 0, 0x0, 0, "", 0},
    {415, ActorEffectFamily::kRenderFlag, 0x80, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, true, 1, -30, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 30},
    {417, ActorEffectFamily::kRenderFlag, 0x100, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, false, 1, 0, -1, -1}}, 15, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x100},
    {419, ActorEffectFamily::kRenderFlag, 0x200, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {420, ActorEffectFamily::kRenderFlag, 0x10, {-1, -1, -1, -1}, {ActorFieldWrite{0xF0, 0, true, false, 1, 0, -1, -1}}, 46, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x10},
    {421, ActorEffectFamily::kRenderFlag, 0x10, {-1, -1, -1, -1}, {ActorFieldWrite{0xF0, 45, false, false, 0, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {422, ActorEffectFamily::kRenderFlag, 0x400, {-1, -1, -1, -1}, {ActorFieldWrite{0xF1, 0, true, false, 1, 0, -1, -1}}, 46, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x400},
    {423, ActorEffectFamily::kRenderFlag, 0x400, {-1, -1, -1, -1}, {ActorFieldWrite{0xF1, 45, false, false, 0, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {424, ActorEffectFamily::kRenderFlag, 0x800, {-1, -1, -1, -1}, {ActorFieldWrite{0xF2, 0, true, false, 1, 0, 200, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {425, ActorEffectFamily::kRenderFlag, 0x1000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, true, 1, -30, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 30},
    {426, ActorEffectFamily::kRenderFlag, 0x2000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, false, 1, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {427, ActorEffectFamily::kRenderFlag, 0x800, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, true, 1, 50, -1, -1}}, 150, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x800},
    {442, ActorEffectFamily::kRenderFlag, 0x8000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF7, 0, true, false, 1, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {445, ActorEffectFamily::kRenderFlag, 0x10000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, true, 1, -35, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 36},
    {446, ActorEffectFamily::kRenderFlag, 0x20040, {-1, -1, -1, -1}, {ActorFieldWrite{0xF6, 0, true, false, 1, 0, -1, -1, true}, ActorFieldWrite{0xF4, 0, true, true, 1, 11, -1, -1}}, 25, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x20000},
    {449, ActorEffectFamily::kRenderFlag, 0x4000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF6, 0, true, false, 1, 0, -1, -1}}, 51, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x4000},
    {457, ActorEffectFamily::kAppearance, 0x0, {-1, -1, -1, -1}, {ActorFieldWrite{0x70, 27, false, false, 0, 0, -1, 5}, ActorFieldWrite{0x70, 2, false, false, 0, 0, -1, 35}}, 0, 0, -1, 0, 0x0, 0, "CActorSprite_SetAnimPlayState", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x0, EffectLedPose{5, 12, 5, 5, 12, 5, 25}},
    {458, ActorEffectFamily::kRenderFlag, 0x2000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, false, 1, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {466, ActorEffectFamily::kRenderFlag, 0x40, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, true, 1, -14, -1, -1}}, 50, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 14},
    {474, ActorEffectFamily::kRenderFlag, 0x2, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {488, ActorEffectFamily::kRenderFlag, 0x800, {-1, -1, -1, -1}, {ActorFieldWrite{0xF2, 0, true, false, 1, 0, -1, -1}, ActorFieldWrite{0xF2, 0, true, false, -1, 144, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {493, ActorEffectFamily::kTint, 0x1, {-1, 255, 0, 255}, {}, 81, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}},
    {496, ActorEffectFamily::kTint, 0x1, {-1, 250, 250, 250}, {}, 31, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 10},
    {500, ActorEffectFamily::kTint, 0x9, {-1, 0, 0, 255}, {}, 31, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}},
    {502, ActorEffectFamily::kTint, 0x9, {-1, 250, 250, 100}, {}, 141, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}},
    {505, ActorEffectFamily::kTint, 0x1, {-1, 250, 250, 100}, {}, 36, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 10},
    {512, ActorEffectFamily::kTint, 0x8, {-1, 255, 255, 150}, {}, 101, 122, 44, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 20},
    {521, ActorEffectFamily::kTint, 0x9, {-1, 255, 0, 0}, {}, 51, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 10},
    {538, ActorEffectFamily::kTint, 0x9, {-1, 0, 255, 0}, {}, 31, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}},
    {540, ActorEffectFamily::kRenderFlag, 0x10, {-1, -1, -1, -1}, {ActorFieldWrite{0xF0, 0, true, false, -3, 45, -1, -1}}, 16, 0, -1, 0, 0x0, 0, "", 0},
    {542, ActorEffectFamily::kRenderFlag, 0x4000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF6, 0, false, false, 0, 0, -1, -1}}, 151, 0, -1, 0, 0x0, 0, "", 0},
    {543, ActorEffectFamily::kRenderFlag, 0x4000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF6, 0, true, false, 1, 0, -1, -1}, ActorFieldWrite{0xF6, 0, true, false, -1, 175, -1, -1}}, 200, 0, -1, 0, 0x0, 0, "", 0},
    {544, ActorEffectFamily::kRenderFlag, 0x40000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF6, 0, false, false, 0, 0, -1, -1}, ActorFieldWrite{0xF6, 25, false, false, 0, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {548, ActorEffectFamily::kRenderFlag, 0x40000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF6, 0, true, true, 3, 0, -1, -1}}, 17, 0, -1, 0, 0x0, 0, "", 0},
    {549, ActorEffectFamily::kRenderFlag, 0x4000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF6, 0, true, true, 3, 0, -1, -1}}, 17, 0, -1, 0, 0x0, 0, "", 0},
    {550, ActorEffectFamily::kRenderFlag, 0x2000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, false, 1, 0, -1, -1}}, 51, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 31},
    {563, ActorEffectFamily::kOrder, 0x0, {-1, -1, -1, -1}, {}, 100, 122, -1, 0, 0x0, 0, "", 0},
    {599, ActorEffectFamily::kTint, 0x1, {-1, 250, 250, 100}, {}, 26, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 5},
    {602, ActorEffectFamily::kTint, 0x2000, {-1, 250, 50, 50}, {ActorFieldWrite{0xF4, 0, true, false, 1, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 21},
    {605, ActorEffectFamily::kOrder, 0x0, {-1, -1, -1, -1}, {}, 0, 109, 0, 604, 0x0, 0, "", 0},
    {617, ActorEffectFamily::kSpriteClone, 0x0, {-1, 155, 155, 255}, {}, 0, 0, -1, 0, 0x0, 0, "EffectUpdate_AfterimageBlueEvery20", 20},
    {625, ActorEffectFamily::kRenderFlag, 0x4000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF6, 20, false, false, 0, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {626, ActorEffectFamily::kRenderFlag, 0x40000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF6, 20, false, false, 0, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {652, ActorEffectFamily::kOrder, 0x0, {-1, -1, -1, -1}, {}, 0, 88, 0, 1, 0xFFFF0000, 0, "", 0},
    {653, ActorEffectFamily::kOrder, 0x0, {-1, -1, -1, -1}, {}, 0, 88, 0, 1, 0xFFFF0000, 0, "", 0},
    {654, ActorEffectFamily::kOrder, 0x0, {-1, -1, -1, -1}, {}, 0, 88, 0, 1, 0xFFFF00FF, 0, "", 0},
    {655, ActorEffectFamily::kRenderFlag, 0x80000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF2, 0, true, false, 1, 0, 200, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {656, ActorEffectFamily::kRenderFlag, 0x80000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF2, 0, true, false, -5, 200, -1, -1}}, 40, 0, -1, 0, 0x0, 0, "", 0},
    {667, ActorEffectFamily::kRenderFlag, 0x2, {-1, -1, -1, -1}, {ActorFieldWrite{0xF2, 0, true, true, 8, 0, -1, -1}, ActorFieldWrite{0xF2, 200, false, false, 0, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {720, ActorEffectFamily::kTint, 0x1, {-1, 205, 205, 0}, {}, 11, 0, -1, 0, 0x0, 0, "", 0},
    {733, ActorEffectFamily::kTint, 0x1, {-1, 155, 155, 255}, {}, 11, 0, -1, 0, 0x0, 0, "", 0},
    {744, ActorEffectFamily::kTint, 0x0, {50, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "EffectUpdate_CloakingAlpha50", 0},
    {750, ActorEffectFamily::kTint, 0x1, {-1, 155, 155, 255}, {}, 41, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 10},
    {752, ActorEffectFamily::kSpriteClone, 0x0, {-1, 225, 225, 255}, {}, 0, 0, -1, 0, 0x0, 0, "EffectUpdate_AfterimagePaleBlueEvery10", 10},
    {768, ActorEffectFamily::kTint, 0x9, {-1, 255, 0, 255}, {}, 31, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}},
    {777, ActorEffectFamily::kTint, 0x9, {-1, 155, 155, 255}, {}, 61, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}},
    {779, ActorEffectFamily::kTint, 0x9, {-1, 155, 155, 255}, {}, 61, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}},
    {782, ActorEffectFamily::kTint, 0x1, {-1, 250, 250, 0}, {}, 31, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 10},
    {786, ActorEffectFamily::kTint, 0x1, {-1, 250, 250, 250}, {}, 91, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 10},
    {791, ActorEffectFamily::kTint, 0x9, {-1, 0, 0, 255}, {}, 41, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 6},
    {792, ActorEffectFamily::kSpriteClone, 0x0, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "ActorAttachedEffect_SpawnJobVariant", 20},
    {799, ActorEffectFamily::kTint, 0x1, {-1, 0, 0, 255}, {}, 41, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 6},
    {805, ActorEffectFamily::kTint, 0x1, {-1, -2, -2, -2}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {809, ActorEffectFamily::kTint, 0x0, {50, 0, 0, 0}, {}, 0, 0, -1, 0, 0x0, 0, "Effect_InitAssassinCloak", 0},
    {812, ActorEffectFamily::kTint, 0x0, {-1, 0, 0, 0}, {}, 61, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 6},
    {823, ActorEffectFamily::kRenderFlag, 0x100000, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {824, ActorEffectFamily::kSpriteClone, 0x0, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "EffectUpdate_AfterimageCloneEquip", 1},
    {832, ActorEffectFamily::kTint, 0x9, {-1, 5, 5, 255}, {}, 53, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 4},
    {835, ActorEffectFamily::kTint, 0x8, {-1, 255, 255, 150}, {}, 101, 122, 44, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 20},
    {847, ActorEffectFamily::kTint, 0x1, {-1, 255, 89, 182}, {}, 91, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 4},
    {848, ActorEffectFamily::kTint, 0x1, {-1, 182, 89, 255}, {}, 91, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 4},
    {864, ActorEffectFamily::kTint, 0x1, {-1, 182, 89, 255}, {}, 91, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 4},
    {886, ActorEffectFamily::kTint, 0x1009, {-1, 0, 0, 255}, {ActorFieldWrite{0xF4, 0, true, false, 1, 0, -1, -1, true}}, 31, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x1000},
    {890, ActorEffectFamily::kSpin, 0x0, {-1, -1, -1, -1}, {ActorFieldWrite{0x4C, 0, false, false, 0, 0, -1, -1}, ActorFieldWrite{0x4C, 0, false, false, 0, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {907, ActorEffectFamily::kRenderFlag, 0x2, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {925, ActorEffectFamily::kSpriteClone, 0x0, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "EffectUpdate_AfterimageClone", 5},
    {928, ActorEffectFamily::kTint, 0x9, {-1, 0, 0, 255}, {}, 41, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 6},
    {934, ActorEffectFamily::kRenderFlag, 0x9, {-1, -1, -1, -1}, {}, 35, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 10},
    {936, ActorEffectFamily::kTint, 0x1009, {-1, 255, 0, 0}, {ActorFieldWrite{0xF4, 0, true, false, 1, 0, -1, -1, true}}, 31, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x1000},
    {937, ActorEffectFamily::kTint, 0x1009, {-1, 255, 255, 0}, {ActorFieldWrite{0xF4, 0, true, false, 1, 0, -1, -1, true}}, 31, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x1000},
    {938, ActorEffectFamily::kTint, 0x1009, {-1, 80, 40, 0}, {ActorFieldWrite{0xF4, 0, true, false, 1, 0, -1, -1, true}}, 31, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x1000},
    {943, ActorEffectFamily::kTint, 0x9, {-1, 55, 255, 55}, {}, 53, 0, -1, 0, 0x0, 0, "", 0, {-1, 255, 255, 255}, 4},
    {963, ActorEffectFamily::kTint, 0x0, {-1, 105, 105, 105}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {1003, ActorEffectFamily::kRenderFlag, 0x4000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF6, 20, false, false, 0, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {1004, ActorEffectFamily::kSpriteClone, 0x0, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 0, "CActorSprite_CopyCtorClone2", 1, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x0, {}, {100, 192, 192, 192}, 5.0F},
    {1037, ActorEffectFamily::kTint, 0x0, {-1, 251, 200, 4}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {1065, ActorEffectFamily::kTint, 0x1, {-1, 255, 255, 255}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {1072, ActorEffectFamily::kRenderFlag, 0x10000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, true, 1, -10, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 11},
    {1081, ActorEffectFamily::kSpriteClone, 0x0, {-1, 200, 200, 0}, {}, 0, 0, -1, 0, 0x0, 0, "CActorSprite_CopyCtorCloneEquip", 5},
    {1082, ActorEffectFamily::kTint, 0x1, {-1, 0, 255, 0}, {}, 11, 0, -1, 0, 0x0, 0, "", 0},
    {1131, ActorEffectFamily::kTint, 0x1, {-1, 255, 255, 255}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {1148, ActorEffectFamily::kTint, 0x0, {-1, 220, 150, 0}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {1207, ActorEffectFamily::kTint, 0x8, {-1, 250, 250, 250}, {}, 81, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 50, 2, {0, -2, -2, 0}},
    {1208, ActorEffectFamily::kTint, 0x8, {-1, 250, 250, 250}, {}, 81, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 50, 2, {0, -2, -2, 0}},
    {1209, ActorEffectFamily::kTint, 0x0, {-1, 155, 155, 255}, {}, 61, 0, -1, 0, 0x0, 0, "", 0, {-1, 50, 50, 50}},
    {1224, ActorEffectFamily::kRenderFlag, 0x40, {-1, -1, -1, -1}, {ActorFieldWrite{0xF4, 0, true, true, 1, -14, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 14},
    {1398, ActorEffectFamily::kRenderFlag, 0x40000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF6, 0, false, false, 0, 0, -1, -1}, ActorFieldWrite{0xF6, 25, false, false, 0, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {1420, ActorEffectFamily::kRenderFlag, 0x4000, {-1, -1, -1, -1}, {ActorFieldWrite{0xF6, 0, false, false, 0, 0, -1, -1}, ActorFieldWrite{0xF6, 15, false, false, 0, 0, -1, -1}}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {1545, ActorEffectFamily::kTint, 0x1009, {-1, 0, 0, 255}, {ActorFieldWrite{0xF4, 0, true, false, 1, 0, -1, -1, true}}, 31, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x1000},
    {1602, ActorEffectFamily::kAppearance, 0x0, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 1080, "CHalluPlant_ctor", 0},
    {1603, ActorEffectFamily::kAppearance, 0x0, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 1079, "CHalluPlant_ctor", 0},
    {1604, ActorEffectFamily::kAppearance, 0x0, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 1081, "CHalluPlant_ctor", 0},
    {1605, ActorEffectFamily::kAppearance, 0x0, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 1082, "CHalluPlant_ctor", 0},
    {1606, ActorEffectFamily::kAppearance, 0x0, {-1, -1, -1, -1}, {}, 0, 0, -1, 0, 0x0, 1078, "CHalluPlant_ctor", 0},
    {2277, ActorEffectFamily::kTint, 0x0, {-1, 105, 105, 105}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {2278, ActorEffectFamily::kTint, 0x0, {-1, 251, 200, 4}, {}, 0, 0, -1, 0, 0x0, 0, "", 0},
    {2373, ActorEffectFamily::kTint, 0x1009, {-1, 255, 0, 0}, {ActorFieldWrite{0xF4, 0, true, false, 1, 0, -1, -1, true}}, 31, 0, -1, 0, 0x0, 0, "", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x1000},
    {2426, ActorEffectFamily::kAppearance, 0x0, {-1, -1, -1, -1}, {ActorFieldWrite{0x70, 2, false, false, 0, 0, -1, 35}, ActorFieldWrite{0x70, 27, false, false, 0, 0, -1, 5}}, 0, 0, -1, 0, 0x0, 0, "CActorSprite_SetAnimPlayState", 0, {-1, -1, -1, -1}, 0, 2, {0, 0, 0, 0}, 0x0, EffectLedPose{5, 12, 5, 5, 12, 5, 25}},
}};
// clang-format on

}  // namespace

std::span<const ActorEffectChange> ActorEffectChangeTable() { return kActorEffectChanges; }

}  // namespace ro::render
