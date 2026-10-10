// LES RUBANS QUI TOURNENT : les noeuds des sous-types 77 et 91.
//
// 3 identifiants, DEUX batisseurs. L'appel ne donne qu'une image et
// une inclinaison de depart ; tout le reste se tire, piste par piste.
//
// ENGENDRE par scripts/gen_client_effect_spin_ribbons.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_spin_ribbon_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSpinRibbonPattern, 3> kSpinRibbons = {{
    {346, "Effect_Build_RisingRing4dFromParam", "data\\texture\\effect\\blue_ivy.bmp", 0, false, 4,
     5, 9},  // sous-type 77 : alpha 100 d'entree, rayon 14, il ne fait que tourner
    {394, "Effect_Build_RisingRing4dFromParam", "data\\texture\\effect\\fire_ivy.bmp", 0, false, 10,
     5, 9},  // sous-type 77 : alpha 100 d'entree, rayon 14, il ne fait que tourner
    {408, "Effect_Build_RisingRing5bFromParam", "data\\texture\\effect\\fire_ivy.bmp", 0, true, 10,
     5, 9},  // sous-type 91 : invisible au depart, rayon 5, il monte et enfle
}};

}  // namespace

std::span<const EffectSpinRibbonPattern> EffectSpinRibbonPatternTable() { return kSpinRibbons; }

}  // namespace ro::render
