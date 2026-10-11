#pragma once

#include <span>

#include "ro/ui/character_window.h"

#include "game/bug_report_context.h"

namespace moonclient {

// Ce que la fiche de personnage montrait quand le joueur a signalé un bug : les
// vues mêmes qu'elle vient de dessiner. Une vue nulle ne se rapporte pas.
struct CharacterBugScene {
  const ro::ui::CharacterWindowState* window = nullptr;
  const ro::ui::CharacterWindowView* view = nullptr;
  const ro::ui::CharacterHeaderView* header = nullptr;
  std::span<const ro::ui::EquipSlotView> slots;
  const ro::ui::CharacterStatsView* stats = nullptr;
  const ro::ui::PresetTabView* presets = nullptr;
  const ro::ui::CharacterTitlesView* titles = nullptr;
  const ro::ui::GuildView* guild = nullptr;
  std::span<const ro::ui::SkillGroupView> groups;
  ro::ui::SkillPointTally points;
  const ro::ui::CompanionTabsView* companions = nullptr;
};

// LE RAPPORT D'UN BUG DE LA FICHE : l'onglet, le métier, les niveaux, la vie et
// la magie — toujours —, puis ce que l'onglet montre :
//   - équipement et costume : les pièces portées par emplacement — `[item,
//     raffinement]` —, les caractéristiques et les nombres dérivés ;
//   - presets : leur nombre, la capture en cours, le dernier refus, et chaque
//     preset — `[nom, pièces, costumes]` ;
//   - titres : celui qu'on porte, combien on en possède, la recherche ;
//   - guilde : la guilde, le rang du joueur, le dernier mot du serveur ;
//   - grimoire : le groupe ouvert, les points, et les points réservés —
//     `[skill, niveaux]` ;
//   - homoncule et mercenaire : leur fiche et leur profil d'IA.
//
// ⚠ LES LISTES SONT BORNÉES en octets, pour que le contexte entier tienne dans
// la colonne du serveur ; `…_cut` dit qu'une liste a été coupée.
[[nodiscard]] BugReportContext CharacterBugReport(const CharacterBugScene& scene);

}  // namespace moonclient
