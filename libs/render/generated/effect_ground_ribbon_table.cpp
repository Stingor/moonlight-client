// LES RUBANS AU SOL : ce que chaque site d'appel passe a son batisseur.
//
// Quatre batisseurs qui sont UN SEUL semis - ils ecrivent les memes champs
// dans le meme ordre, et ne different que par leurs constantes.
//
// 44 appels pour 12 effets : la plupart posent DEUX noeuds, un par SENS.
// Le reste de l'argument par dix decide du cote ou le ruban se decale.
//
// Une image VIDE veut dire que le batisseur la choisit lui-meme, selon la
// dizaine de son argument : voir effect_ground_ribbon_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_ground_ribbons.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_ground_ribbon_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectGroundRibbonPattern, 44> kGroundRibbons = {{
    {439, "Effect_Build_LineBeams", "", EffectGroundRibbonBuilder::kBuildLineBeams, 0},
    {439, "Effect_Build_LineBeams", "", EffectGroundRibbonBuilder::kBuildLineBeams, 1},
    {512, "Effect_Build_LineBeams", "", EffectGroundRibbonBuilder::kBuildLineBeams, 10},
    {512, "Effect_Build_LineBeams", "", EffectGroundRibbonBuilder::kBuildLineBeams, 11},
    {623, "Effect_Build_LineBeams", "", EffectGroundRibbonBuilder::kBuildLineBeams, 20},
    {623, "Effect_Build_LineBeams", "", EffectGroundRibbonBuilder::kBuildLineBeams, 21},
    {835, "Effect_Build_LineBeams", "", EffectGroundRibbonBuilder::kBuildLineBeams, 10},
    {835, "Effect_Build_LineBeams", "", EffectGroundRibbonBuilder::kBuildLineBeams, 11},
    {836, "Effect_InitLineBeamTextured", "data\\texture\\effect\\throw.bmp",
     EffectGroundRibbonBuilder::kInitTextured, 10},
    {836, "Effect_InitLineBeamTextured", "data\\texture\\effect\\throw.bmp",
     EffectGroundRibbonBuilder::kInitTextured, 11},
    {893, "Effect_InitLineBeam", "", EffectGroundRibbonBuilder::kInitLineBeam, 0},
    {893, "Effect_InitLineBeam", "", EffectGroundRibbonBuilder::kInitLineBeam, 1},
    {904, "Effect_InitLineBeamTextured", "data\\texture\\effect\\throw.bmp",
     EffectGroundRibbonBuilder::kInitTextured, 10},
    {904, "Effect_InitLineBeamTextured", "data\\texture\\effect\\throw.bmp",
     EffectGroundRibbonBuilder::kInitTextured, 11},
    {1409, "Effect_InitLineBeamYellow", "", EffectGroundRibbonBuilder::kInitLineBeamYellow, 0},
    {1409, "Effect_InitLineBeamYellow", "", EffectGroundRibbonBuilder::kInitLineBeamYellow, 1},
    {1568, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\mineffect\\new_crossimpact\\new_crossimpact_line\\line_pitch.tga",
     EffectGroundRibbonBuilder::kInitTextured, 10},
    {1568, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\mineffect\\new_crossimpact\\new_crossimpact_line\\line_red.tga",
     EffectGroundRibbonBuilder::kInitTextured, 10},
    {1568, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\new_crossimpact\\new_crossimpact_line\\line_pitch.tga",
     EffectGroundRibbonBuilder::kInitTextured, 10},
    {1568, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\new_crossimpact\\new_crossimpact_line\\line_red.tga",
     EffectGroundRibbonBuilder::kInitTextured, 10},
    {1568, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\mineffect\\new_crossimpact\\new_crossimpact_line\\line_pitch.tga",
     EffectGroundRibbonBuilder::kInitTextured, 11},
    {1568, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\mineffect\\new_crossimpact\\new_crossimpact_line\\line_red.tga",
     EffectGroundRibbonBuilder::kInitTextured, 11},
    {1568, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\new_crossimpact\\new_crossimpact_line\\line_pitch.tga",
     EffectGroundRibbonBuilder::kInitTextured, 11},
    {1568, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\new_crossimpact\\new_crossimpact_line\\line_red.tga",
     EffectGroundRibbonBuilder::kInitTextured, 11},
    {1575, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\mineffect\\new_gateofhell\\line_hell.tga",
     EffectGroundRibbonBuilder::kInitTextured, 10},
    {1575, "Effect_InitLineBeamTextured", "data\\texture\\effect\\new_gateofhell\\line_hell.tga",
     EffectGroundRibbonBuilder::kInitTextured, 10},
    {1575, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\mineffect\\new_gateofhell\\line_hell.tga",
     EffectGroundRibbonBuilder::kInitTextured, 11},
    {1575, "Effect_InitLineBeamTextured", "data\\texture\\effect\\new_gateofhell\\line_hell.tga",
     EffectGroundRibbonBuilder::kInitTextured, 11},
    {2416, "Effect_InitLineBeamTextured", "data\\texture\\v643",
     EffectGroundRibbonBuilder::kInitTextured, 20},
    {2416, "Effect_InitLineBeamTextured", "data\\texture\\v645",
     EffectGroundRibbonBuilder::kInitTextured, 20},
    {2416, "Effect_InitLineBeamTextured", "data\\texture\\v648",
     EffectGroundRibbonBuilder::kInitTextured, 20},
    {2416, "Effect_InitLineBeamTextured", "data\\texture\\v650",
     EffectGroundRibbonBuilder::kInitTextured, 20},
    {2416, "Effect_InitLineBeamTextured", "data\\texture\\v644",
     EffectGroundRibbonBuilder::kInitTextured, 21},
    {2416, "Effect_InitLineBeamTextured", "data\\texture\\v646",
     EffectGroundRibbonBuilder::kInitTextured, 21},
    {2416, "Effect_InitLineBeamTextured", "data\\texture\\v649",
     EffectGroundRibbonBuilder::kInitTextured, 21},
    {2416, "Effect_InitLineBeamTextured", "data\\texture\\v651",
     EffectGroundRibbonBuilder::kInitTextured, 21},
    {2417, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\mineffect\\new_crossimpact\\new_crossimpact_line\\line_pitch.tga",
     EffectGroundRibbonBuilder::kInitTextured, 30},
    {2417, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\mineffect\\new_crossimpact\\new_crossimpact_line\\line_red.tga",
     EffectGroundRibbonBuilder::kInitTextured, 30},
    {2417, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\new_crossimpact\\new_crossimpact_line\\line_pitch.tga",
     EffectGroundRibbonBuilder::kInitTextured, 30},
    {2417, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\new_crossimpact\\new_crossimpact_line\\line_red.tga",
     EffectGroundRibbonBuilder::kInitTextured, 30},
    {2417, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\mineffect\\new_crossimpact\\new_crossimpact_line\\line_pitch.tga",
     EffectGroundRibbonBuilder::kInitTextured, 31},
    {2417, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\mineffect\\new_crossimpact\\new_crossimpact_line\\line_red.tga",
     EffectGroundRibbonBuilder::kInitTextured, 31},
    {2417, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\new_crossimpact\\new_crossimpact_line\\line_pitch.tga",
     EffectGroundRibbonBuilder::kInitTextured, 31},
    {2417, "Effect_InitLineBeamTextured",
     "data\\texture\\effect\\new_crossimpact\\new_crossimpact_line\\line_red.tga",
     EffectGroundRibbonBuilder::kInitTextured, 31},
}};

}  // namespace

std::span<const EffectGroundRibbonPattern> EffectGroundRibbonPatternTable() {
  return kGroundRibbons;
}

}  // namespace ro::render
