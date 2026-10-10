// L'EVENTAIL DE SPRITES : les noeuds du sous-type 167.
//
// 3 identifiants, UN SEUL batisseur, UN SEUL octet par appel - et cet
// octet choisit le sprite ET la teinte.
//
// ENGENDRE par scripts/gen_client_effect_sprite_fans.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_sprite_fan_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSpriteFanPattern, 3> kSpriteFans = {{
    {901, "Effect_InitSprActFan", "data\\sprite\\이팩트\\m_ef03", 0},
    // octet 9, la teinte des CALQUES, a quatre-vingt-cinq pour cent d'alpha
    {923, "Effect_InitSprActFan", "data\\sprite\\이팩트\\m_ef03", 1},
    // octet 10, un bleu vif - 55, 63, 255
    {941, "Effect_InitSprActFan", "data\\sprite\\이팩트\\m_ef05", 4},
    // octet 20, AUCUNE : le client garde celle du quad precedent
}};

}  // namespace

std::span<const EffectSpriteFanPattern> EffectSpriteFanPatternTable() { return kSpriteFans; }

}  // namespace ro::render
