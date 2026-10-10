// LA PETITE BOITE QUI TOURNE : qui la pose.
//
// Le sous-type 114, qui ne lit de son cas qu'une image.
//
// 1 appel(s). Colonnes : identifiant, fonction, image. Tout le reste est
// ecrit en dur par le batisseur : voir effect_spinning_box_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_spinning_boxes.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_spinning_box_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSpinningBoxPattern, 1> kSpinningBoxes = {{
    {517, "Effect_Build_RandomSpinNode", "data\\texture\\effect\\white02.bmp"},
}};

}  // namespace

std::span<const EffectSpinningBoxPattern> EffectSpinningBoxPatternTable() { return kSpinningBoxes; }

}  // namespace ro::render
