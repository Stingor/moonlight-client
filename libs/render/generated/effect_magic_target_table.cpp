// LES GRILLES DE VISEE : les noeuds du sous-type 112.
//
// 1 identifiant, UN SEUL batisseur, et il ne prend AUCUN parametre :
// son image est une constante, et tout le reste vient de la figure et du
// mouvement.
//
// ENGENDRE par scripts/gen_client_effect_magic_targets.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_magic_target_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectMagicTargetPattern, 1> kMagicTargets = {{
    {513, "Effect_Build_MagicTarget",
     "data\\texture\\effect\\magic_target.tga"},  // EF_GROUNDSAMPLE
}};

}  // namespace

std::span<const EffectMagicTargetPattern> EffectMagicTargetPatternTable() { return kMagicTargets; }

}  // namespace ro::render
