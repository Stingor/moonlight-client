// L'ÉTINCELLE DU SOIN : les nœuds du sous-type 58.
//
// UN SEUL bâtisseur, UN SEUL identifiant. La figure est celle du carré
// teinté ; c'est le MOUVEMENT qui fait cette famille — l'alpha paraît en
// vingt tics, tient jusqu'au centième, puis s'efface.
//
// ENGENDRÉ par scripts/gen_client_effect_heal_sparkle.py — ne pas
// modifier à la main.

#include <array>

#include "ro/render/effect_heal_sparkle_seed.h"

namespace ro::render {
namespace {

// Triée par identifiant d'effet : la recherche est dichotomique.
constexpr std::array<EffectHealSparklePattern, 1> kHealSparkles = {{
    {309, "Effect_Build_HealPikapika", "data\\texture\\effect\\pikapika2.bmp", 1, 10.0F},
}};

}  // namespace

std::span<const EffectHealSparklePattern> EffectHealSparklePatternTable() { return kHealSparkles; }

}  // namespace ro::render
