// D'une UNITÉ POSÉE AU SOL — un piège, un mur, une zone — au MODÈLE qui la
// dessine.
//
// Ce n'est ni un acteur ni un effet : c'est un modèle 3D, du même genre que
// l'Emperium. Le client le choisit par un switch sur le métier d'affichage de
// l'acteur, lequel vaut l'identifiant d'unité.
//
// Les chemins sont relatifs au dossier des modèles, et les noms coréens sont
// ceux de l'archive : « 트랩 » veut dire piège.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_skill_units.py depuis le relevé
// scripts/client_skill_units.json, pas écrit à la main.

#include "ro/render/skill_unit_table.h"

#include <array>

namespace ro::render {
namespace {

// Triée par identifiant : la recherche est dichotomique.
constexpr std::array<SkillUnitEntry, 27> kSkillUnits = {{
    {143, "외부소품\\트랩03_3.rsm"},
    {144, "외부소품\\트랩02.rsm"},
    {145, "외부소품\\트랩01.rsm"},
    {147, "외부소품\\트랩03.rsm"},
    {148, "외부소품\\트랩03_6.rsm"},
    {149, "외부소품\\트랩03_4.rsm"},
    {150, "외부소품\\트랩03_5.rsm"},
    {151, "외부소품\\트랩03_2.rsm"},
    {152, "외부소품\\트랩04.rsm"},
    {153, "외부소품\\트랩05.rsm"},
    {203, "외부소품\\트랩01.rsm"},
    {210, "event\\3차트랩_변화01.rsm"},
    {211, "event\\3차트랩_변수01.rsm"},
    {212, "event\\3차트랩_변지01.rsm"},
    {213, "event\\3차트랩_변풍01.rsm"},
    {214, "event\\3차트랩_화01.rsm"},
    {215, "event\\3차트랩_수01.rsm"},
    {216, "event\\3차트랩_풍01.rsm"},
    {217, "event\\3차트랩_지01.rsm"},
    {218, "외부소품\\트랩03_3.rsm"},
    {222, "외부소품\\트랩03_3.rsm"},
    {229, "event\\3차트랩_가시01.rsm"},
    {230, "외부소품\\트랩03_3.rsm"},
    {20852, "effect\\deepblindtrap\\deep_trap.rsm2"},
    {20853, "effect\\solidtrap\\sol_trap.rsm2"},
    {20854, "effect\\swifttrap\\swi_trap.rsm2"},
    {20855, "effect\\flametrap\\fla_trap.rsm2"},
}};

}  // namespace

std::span<const SkillUnitEntry> SkillUnitTable() { return kSkillUnits; }

}  // namespace ro::render
