// CE QU'UNE UNITÉ AU SOL POSE SUR CE QU'ELLE TOUCHE.
//
// 🔴 UNE UNITÉ N'EST PAS UN ACTEUR ORDINAIRE. Quand la SOURCE d'un coup est
// une unité posée au sol, le message du coup va au traitement propre aux
// unités et non au répartiteur des acteurs : celui-là ne pose AUCUN effet
// de compétence — ni celui du chemin des dégâts, ni l'effet visé — et se
// borne à choisir l'effet de FRAPPE.
// C'est pourquoi le blizzard de Storm Gust ne renaît pas à chaque battement
// de ses unités.
//
// L'effet de message que porte le coup y vaut TOUJOURS −1 : la carte des
// effets de message de dégâts ne joue pas non plus par ce chemin.
//
// 24 écart(s) ; partout ailleurs, −1 à partir de 493 et
// 0 en deçà.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_skill_unit_hit_effects.py depuis
// le relevé scripts/client_skill_unit_hit_effects.json, pas écrit à la main.

#include <array>

#include "ro/render/skill_hit_effect.h"

namespace ro::render {
namespace {

// Triée par identifiant de compétence : la recherche est dichotomique.
constexpr std::array<SkillHitEffectEntry, 24> kUnitHitEffects = {{
    {18, 49},     {79, 152},    {85, 52},     {89, 51},     {428, 583},   {429, 583},
    {430, 583},   {723, 51},    {2587, 52},   {5217, 1775}, {5219, 1892}, {5221, 1892},
    {5223, 1799}, {5224, 1799}, {5227, 1780}, {5229, 1807}, {5321, 2131}, {5331, 1875},
    {5332, 1883}, {5333, 1879}, {5335, 1922}, {5370, 2045}, {5371, 2034}, {5372, 2059},
}};

}  // namespace

std::span<const SkillHitEffectEntry> SkillUnitHitEffectTable() { return kUnitHitEffects; }

SkillHitEffectDefault SkillUnitHitEffectDefaults() { return {0, 0, 493, 0}; }

}  // namespace ro::render
