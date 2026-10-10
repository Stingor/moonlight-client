// LES DISQUES AU SOL : ce que chaque site d'appel passe a son batisseur.
//
// 12 disques pour 6 effets, tous du sous-type 26. UNE RANGEE PAR APPEL :
// un cercle magique de carte en empile trois, chacun avec son image et son
// octet. L'image vient du SITE et non du batisseur, qui n'en connait aucune ;
// ce que l'octet decide vit dans le semis, voir effect_ground_disc_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_ground_discs.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_ground_disc_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectGroundDiscPattern, 12> kGroundDiscs = {{
    {650, "Effect_Build_GroundDiscFromParam1a", "data\\texture\\effect\\mjin2.tga", 0},
    {650, "Effect_Build_GroundDiscFromParam1a", "data\\texture\\effect\\mjin.tga", 1},
    {651, "Effect_Build_GroundDiscFromParam1a", "data\\texture\\effect\\mjin2.tga", 2},
    {651, "Effect_Build_GroundDiscFromParam1a", "data\\texture\\effect\\mjin.tga", 3},
    {687, "Effect_Build_GroundDiscFromParam1a", "data\\texture\\effect\\마법진1-1.bmp", 4},
    {687, "Effect_Build_GroundDiscFromParam1a", "data\\texture\\effect\\마법진1-2.bmp", 14},
    {687, "Effect_Build_GroundDiscFromParam1a", "data\\texture\\effect\\마법진1-3.bmp", 4},
    {688, "Effect_Build_GroundDiscFromParam1a", "data\\texture\\effect\\마법진2-1.bmp", 4},
    {688, "Effect_Build_GroundDiscFromParam1a", "data\\texture\\effect\\마법진2-2.bmp", 14},
    {688, "Effect_Build_GroundDiscFromParam1a", "data\\texture\\effect\\마법진2-3.bmp", 4},
    {695, "Effect_Build_GroundDiscFromParam1a", "data\\texture\\effect\\glow04.bmp", 5},
    {2402, "Effect_Build_GroundDiscFromParam1a", "data\\texture\\effect\\glow04_bk.bmp", 5},
}};

}  // namespace

std::span<const EffectGroundDiscPattern> EffectGroundDiscPatternTable() { return kGroundDiscs; }

}  // namespace ro::render
