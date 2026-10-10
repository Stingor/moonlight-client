// LE CYLINDRE DE NOTES DE SEVERE RAINSTORM : les noeuds du sous-type 173.
//
// 4 appel(s) du meme cas, un noeud chacun : le rayon et l'octet viennent
// du site, tout le reste est en dur dans le batisseur.
//
// ENGENDRE par scripts/gen_client_effect_rainstorm_cylinders.py - ne pas
// modifier a la main.

#include <array>
#include <string_view>

#include "ro/render/effect_rainstorm_cylinder_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectRainstormCylinderPattern, 4> kRainstormCylinders = {{
    {1286, "Effect_InitSevereRainstorm", 3.0F, 3, 120},
    {1286, "Effect_InitSevereRainstorm", 3.0F, 3, 120},
    {1286, "Effect_InitSevereRainstorm", 8.0F, 3, 120},
    {1286, "Effect_InitSevereRainstorm", 8.0F, 3, 120},
}};

constexpr std::array<std::string_view, 9> kRainstormCylinderImages = {{
    "data\\texture\\effect\\new_severerainstorm\\new_severerainstorm_cylinder\\music_01.tga",
    "data\\texture\\effect\\new_severerainstorm\\new_severerainstorm_cylinder\\music_02.tga",
    "data\\texture\\effect\\new_severerainstorm\\new_severerainstorm_cylinder\\music_03.tga",
    "data\\texture\\effect\\new_severerainstorm\\new_severerainstorm_cylinder\\music_04.tga",
    "data\\texture\\effect\\new_severerainstorm\\new_severerainstorm_cylinder\\music_05.tga",
    "data\\texture\\effect\\new_severerainstorm\\new_severerainstorm_cylinder\\music_06.tga",
    "data\\texture\\effect\\new_severerainstorm\\new_severerainstorm_cylinder\\music_07.tga",
    "data\\texture\\effect\\new_severerainstorm\\new_severerainstorm_cylinder\\music_08.tga",
    "data\\texture\\effect\\new_severerainstorm\\new_severerainstorm_cylinder\\music_09.tga",
}};

}  // namespace

std::span<const EffectRainstormCylinderPattern> EffectRainstormCylinderPatternTable() {
  return kRainstormCylinders;
}

std::span<const std::string_view> RainstormCylinderImages() { return kRainstormCylinderImages; }

}  // namespace ro::render
