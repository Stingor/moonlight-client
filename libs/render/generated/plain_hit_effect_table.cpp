// L'IMPACT D'UN COUP ORDINAIRE : ce qu'un coup SANS compétence pose.
//
// 🔴 IL EN POSE UN, LUI AUSSI. Le client remplit les MÊMES deux champs du
// message de dégâts — l'effet de FRAPPE et celui du MESSAGE — dans le
// traitement du coup lui-même, au lieu du répartiteur des compétences. Deux
// choses en décident : la SORTE de coup et la CLASSE de l'attaquant.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_plain_hit_effects.py depuis le
// relevé scripts/client_plain_hit_effects.json, pas écrit à la main.

#include "ro/render/plain_hit_effect_table.h"

#include <array>

namespace ro::render {
namespace {

// Les sortes de coup qui posent un effet de frappe : les deux CRITIQUES, et
// elles seules.
constexpr std::array<PlainHitByDamageType, 2> kStrikeEffects = {{
    {10, 1},
    {13, 1},
}};

// Les classes d'attaquant qui écartent l'effet du message de son défaut.
constexpr std::array<PlainHitByJob, 16> kMsgEffects = {{
    {4046, 434},
    {4047, 434},
    {4048, 434},
    {4225, 434},
    {4226, 434},
    {4238, 434},
    {4239, 434},
    {4240, 434},
    {4241, 434},
    {4242, 434},
    {4243, 434},
    {4302, 434},
    {4303, 434},
    {4316, 434},
    {22383, 2420},
    {22384, 2420},
}};

}  // namespace

std::span<const PlainHitByDamageType> PlainHitStrikeEffectTable() { return kStrikeEffects; }

std::span<const PlainHitByJob> PlainHitMsgEffectTable() { return kMsgEffects; }

PlainHitEffectDefaults PlainHitDefaults() { return {-1, 0, -2}; }

}  // namespace ro::render
