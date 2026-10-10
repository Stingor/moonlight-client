// L'AURA DU PERSONNAGE : les noeuds des sous-types 99 et 35.
//
// 6 identifiants, DEUX batisseurs, DEUX octets par appel - le nombre de
// pistes vives et la marque de teinte. Les cinq images sont les memes
// pour les six.
//
// ENGENDRE par scripts/gen_client_effect_job_auras.py - ne pas modifier
// a la main.

#include <array>
#include <string_view>

#include "ro/render/effect_job_aura_seed.h"

namespace ro::render {
namespace {

constexpr std::array<std::string_view, 5> kAuraFrames = {{
    "data\\texture\\effect\\super1.bmp",
    "data\\texture\\effect\\super2.bmp",
    "data\\texture\\effect\\super3.bmp",
    "data\\texture\\effect\\super4.bmp",
    "data\\texture\\effect\\super5.bmp",
}};

constexpr std::array<EffectJobAuraPattern, 6> kJobAuras = {{
    {203, "Effect_Build_SuperParticles2", kAuraFrames, 1, 0, false},
    // 1 piste, bleu, 100/100/255
    {254, "Effect_Build_SuperParticles2", kAuraFrames, 4, 0, false},
    // 4 pistes, bleu, 100/100/255
    {418, "Effect_Build_SuperParticles", kAuraFrames, 2, 2, true},
    // 2 pistes, rouge, 255/100/100 puis 120/0/0
    {455, "Effect_Build_SuperParticles2", kAuraFrames, 2, 1, false},
    // 2 pistes, ROUGE, 255/20/20 - et il deborde sur les pistes suivantes
    {485, "Effect_Build_SuperParticles", kAuraFrames, 2, 1, true},
    // 2 pistes, blanc dore, 90/90/80 puis 60/60/50 puis 30/30/20
    {486, "Effect_Build_SuperParticles", kAuraFrames, 2, 0, true},
    // 2 pistes, bleu, 100/100/255 puis 0/0/120
}};

}  // namespace

std::span<const EffectJobAuraPattern> EffectJobAuraPatternTable() { return kJobAuras; }

}  // namespace ro::render
