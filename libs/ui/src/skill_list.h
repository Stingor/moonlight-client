#pragma once

#include <string_view>

#include "ro/ui/skill_window.h"
#include "ro/ui/skin_art.h"

// LE MÊME ARBRE, LU COMME UN ARBRE.
//
// 🔴 LA GRILLE MONTRE LA DISPOSITION DU CLIENT, pas la STRUCTURE. Les cases y
// sont à la place que ses tables leur donnent — une place choisie pour tenir en
// sept colonnes, pas pour dire ce qui mène à quoi. La liste, elle, range par
// PROFONDEUR : un prérequis paraît toujours avant ce qu'il débloque, et le coude
// tracé dans la gouttière dit lequel.
//
// ⚠ ET ELLE ÉCRIT CE QUE LA GRILLE TAIT : le coût, la portée, le rang sur son
// maximum. Une case de soixante-douze pixels n'a pas la place ; une ligne, si.
//
// En-tête PRIVÉ : c'est une vue du grimoire, et le grimoire est un onglet de la
// fiche.
namespace ro::ui {

void DrawSkillList(SkillWindowState& state, const SkillGroupView& group, std::string_view filter,
                   const SkinArt& art, bool staff_allowed, SkillWindowAction& action);

}  // namespace ro::ui
