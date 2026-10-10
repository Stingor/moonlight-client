// Les effets qu'une fabrique d'unité au sol désigne par un NOM.
//
// Huit fabriques sur douze nomment leur effet par une chaîne plutôt que par un
// identifiant. Cette chaîne se résout dans une table de DONNÉES que
// l'installation porte — voir ro::formats::NamedEffectTable —, et sans elle ces
// huit unités ne montrent rien du tout : ni la table de l'apparition ni celle
// de l'entretien ne les connaissent.
//
// ⚠ DEUX MOMENTS, ET ILS NE DISENT PAS LA MÊME CHOSE : l'objet d'action joue
// une méthode à la POSE et une autre à la LEVÉE. Tous les noms d'un même
// moment partent ensemble, sans condition ; c'est la DONNÉE qui les ordonne,
// par le délai de départ de chaque fiche et son drapeau d'infini.
//
// ⚠ LE NOM NE DIT PAS CE QU'ON EN FAIT : à la levée, une fabrique désigne ses
// boucles par leur nom pour les COUPER, avant de semer sa fin.
//
// ⚠ UNE FABRIQUE PEUT AUSSI SEMER PAR NUMÉRO, à la pose : ces entrées-là ont un
// nom vide et un `effect_id`, une classe du gestionnaire attachée à l'unité.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_skill_unit_named_effects.py depuis
// les relevés scripts/client_skill_unit_visuals.json,
// scripts/client_named_effect_actions.json et
// scripts/client_skill_action_cases.json, pas écrit à la main.

#include <array>

#include "ro/render/skill_unit_table.h"

namespace ro::render {
namespace {

// Triée par identifiant d'unité, puis par moment ; les noms d'un même moment
// gardent l'ordre dans lequel sa fabrique les joue.
constexpr std::array<SkillUnitNamedEffectEntry, 40> kSkillUnitNamedEffects = {{
    // IG_CROSS_RAIN
    {275, "new_cross_rain_bottom", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {.unit_id = 275,
     .name = "",
     .moment = SkillUnitNamedMoment::kOnPlace,
     .verb = NamedEffectVerb::kSow,
     .effect_id = 2165,
     .level_as_param = true},
    // SKE_TWINKLING_GALAXY
    {.unit_id = 286,
     .name = "",
     .moment = SkillUnitNamedMoment::kOnPlace,
     .verb = NamedEffectVerb::kSow,
     .effect_id = 2349,
     .level_as_param = true},
    // SKE_STAR_CANNON
    {.unit_id = 287,
     .name = "",
     .moment = SkillUnitNamedMoment::kOnPlace,
     .verb = NamedEffectVerb::kSow,
     .effect_id = 2350,
     .level_as_param = true},
    // NW_GRENADES_DROPPING
    {.unit_id = 288,
     .name = "",
     .moment = SkillUnitNamedMoment::kOnPlace,
     .verb = NamedEffectVerb::kSow,
     .effect_id = 2351,
     .level_as_param = true},
    // NPC_FUUMA
    {.unit_id = 290,
     .name = "",
     .moment = SkillUnitNamedMoment::kOnPlace,
     .verb = NamedEffectVerb::kSow,
     .effect_id = 2352,
     .born_at_creator = true},
    // NW_MISSION_BOMBARD
    {291, "mission_bombard", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {291, "mission_bombard_fire", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {291, "mission_bombard_fire_bottom", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {.unit_id = 291,
     .name = "",
     .moment = SkillUnitNamedMoment::kOnPlace,
     .verb = NamedEffectVerb::kSow,
     .effect_id = 2354},
    // SOA_TOTEM_OF_TUTELARY
    {292, "totem_of_tutelary_bottom_start", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {292, "totem_of_tutelary_bottom_loop", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {292, "totem_of_tutelary_start", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {292, "totem_of_tutelary_loop", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {292, "totem_of_tutelary_bottom_loop", SkillUnitNamedMoment::kOnVanish, NamedEffectVerb::kCut},
    {292, "totem_of_tutelary_bottom_end", SkillUnitNamedMoment::kOnVanish, NamedEffectVerb::kSow},
    {292, "totem_of_tutelary_loop", SkillUnitNamedMoment::kOnVanish, NamedEffectVerb::kCut},
    {292, "totem_of_tutelary_end", SkillUnitNamedMoment::kOnVanish, NamedEffectVerb::kSow},
    // SH_HYUN_ROKS_BREEZE
    {293, "hyun_roks_breeze_bottom", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {293, "hyun_roks_breeze", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    // SS_SBUNSHIN
    {294, "shinkirou_idle_start", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {294, "shinkirou_idle_loop", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {294, "shinkirou_idle_start", SkillUnitNamedMoment::kOnVanish, NamedEffectVerb::kCut},
    {294, "shinkirou_idle_loop", SkillUnitNamedMoment::kOnVanish, NamedEffectVerb::kCut},
    {294, "shinkirou_idle_end", SkillUnitNamedMoment::kOnVanish, NamedEffectVerb::kSow},
    // HN_JACK_FROST_NOVA
    {295, "jack_frost_nova_start_bottom", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {295, "jack_frost_nova_start", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {295, "jack_frost_nova_loop_bottom", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {295, "jack_frost_nova_loop", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {295, "jack_frost_nova_end", SkillUnitNamedMoment::kOnVanish, NamedEffectVerb::kSow},
    // HN_GROUND_GRAVITATION
    {296, "ground_gravitation_bottom_start", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {296, "ground_gravitation_start", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {296, "ground_gravitation_bottom_loop", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {296, "ground_gravitation_loop", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {296, "ground_gravitation_bottom_loop", SkillUnitNamedMoment::kOnVanish,
     NamedEffectVerb::kStop},
    {296, "ground_gravitation_loop", SkillUnitNamedMoment::kOnVanish, NamedEffectVerb::kStop},
    // SS_KUNAITRAP
    {298, "kunaiarea_01_start", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {298, "kunaiarea_01_loop", SkillUnitNamedMoment::kOnPlace, NamedEffectVerb::kSow},
    {298, "kunaiarea_01_loop", SkillUnitNamedMoment::kOnVanish, NamedEffectVerb::kCut},
    {298, "kunaiarea_01_end", SkillUnitNamedMoment::kOnVanish, NamedEffectVerb::kSow},
}};

}  // namespace

std::span<const SkillUnitNamedEffectEntry> SkillUnitNamedEffectTable() {
  return kSkillUnitNamedEffects;
}

}  // namespace ro::render
