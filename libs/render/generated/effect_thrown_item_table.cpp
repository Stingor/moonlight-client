// LES OBJETS LANCES : ce que chaque site d'appel passe a son batisseur.
//
// 12 objets pour 11 effets, tous du sous-type 53. L'image vient du SITE et
// non du batisseur, et c'est une icone du sac, pas une texture d'effet.
// Le style decide de la forme ET du tic de naissance : voir
// effect_thrown_item_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_thrown_items.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_thrown_item_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectThrownItemPattern, 12> kThrownItems = {{
    {298, "EffectInst_InitBeamTrailParticle", "data\\texture\\유저인터페이스\\item\\빈병.bmp", 0},
    {299, "EffectInst_InitBeamTrailParticle", "data\\texture\\유저인터페이스\\item\\빈포션병.bmp",
     1},
    {308, "EffectInst_InitBeamTrailParticle", "data\\texture\\유저인터페이스\\item\\돌.bmp", 2},
    {539, "EffectInst_InitBeamTrailParticle", "data\\texture\\유저인터페이스\\item\\화염병.bmp",
     10},
    {539, "EffectInst_InitBeamTrailParticle", "data\\texture\\유저인터페이스\\item\\염산병.bmp",
     11},
    {541, "EffectInst_InitBeamTrailParticle", "data\\texture\\유저인터페이스\\item\\버서크포션.bmp",
     1},
    {600, "EffectInst_InitBeamTrailParticle", "data\\texture\\유저인터페이스\\item\\베넘나이프.bmp",
     3},
    {613, "EffectInst_InitBeamTrailParticle", "data\\texture\\유저인터페이스\\item\\수리검.bmp", 3},
    {614, "EffectInst_InitBeamTrailParticle", "data\\texture\\유저인터페이스\\item\\쿠나이_독.bmp",
     3},
    {615, "EffectInst_InitBeamTrailParticle", "data\\texture\\유저인터페이스\\item\\풍마_열화.bmp",
     3},
    {616, "EffectInst_InitBeamTrailParticle", "data\\texture\\effect\\coin_a.bmp", 3},
    {914, "EffectInst_InitBeamTrailParticle",
     "data\\texture\\유저인터페이스\\item\\폭탄버섯포자.bmp", 1},
}};

}  // namespace

std::span<const EffectThrownItemPattern> EffectThrownItemPatternTable() { return kThrownItems; }

}  // namespace ro::render
