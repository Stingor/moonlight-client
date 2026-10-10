// LES COLONNES D'AURA : ce que chaque effet passe a son batisseur.
//
// 1 colonnes pour 1 effets, toutes du sous-type de noeud 131. Le batisseur
// ne recoit qu'une image ; tout le reste est ecrit en dur et vit dans le
// semis, voir
// effect_aura_column_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_aura_columns.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_aura_column_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectAuraColumnPattern, 1> kAuraColumns = {{
    {726, "Effect_Build_SpriteNode83", "data\\texture\\effect\\freeze_a.bmp"},
}};

}  // namespace

std::span<const EffectAuraColumnPattern> EffectAuraColumnPatternTable() { return kAuraColumns; }

}  // namespace ro::render
