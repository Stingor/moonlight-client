// L'EFFET qu'une unité posée au sol fait naître, en plus de son modèle.
//
// ⚠ La clé est l'identifiant d'UNITÉ, pas celui de la compétence. Le client
// range cet effet avec le modèle de l'unité, et chercher par compétence ne le
// trouve pas — c'est ainsi que Pneuma restait sans image alors que son fichier
// est dans l'archive.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_skill_unit_effects.py depuis le
// relevé scripts/client_skill_unit_effects.json, pas écrit à la main.

#include <array>

#include "ro/render/skill_unit_table.h"

namespace ro::render {
namespace {

// Triée par identifiant d'unité : la recherche est dichotomique.
constexpr std::array<SkillUnitEffectEntry, 16> kSkillUnitEffects = {{
    {133, 141},
    {148, 145},
    {216, 145},
    {263, 1125},
    {267, 1772},
    {270, 1793},
    {271, 1794},
    {272, 1776},
    {277, 2130},
    {278, 2216},
    {279, 2232},
    {280, 2224},
    {281, 2208},
    {282, 2042},
    {283, 2031},
    {284, 2056},
}};

}  // namespace

std::span<const SkillUnitEffectEntry> SkillUnitEffectTable() { return kSkillUnitEffects; }

}  // namespace ro::render
