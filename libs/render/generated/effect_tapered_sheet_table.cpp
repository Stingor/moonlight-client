// LES FEUILLES QUI S'ETIRENT : qui les pose.
//
// Le sous-type 141. 1 appel(s). Colonnes : identifiant, fonction, image,
// octet. Tout le reste est ecrit en dur par le batisseur : voir
// effect_tapered_sheet_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_tapered_sheets.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_tapered_sheet_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectTaperedSheetPattern, 1> kTaperedSheets = {{
    {811, "Effect_InitStackedSprite4", "data\\texture\\effect\\freezing_stick.bmp", 0},
}};

}  // namespace

std::span<const EffectTaperedSheetPattern> EffectTaperedSheetPatternTable() {
  return kTaperedSheets;
}

}  // namespace ro::render
