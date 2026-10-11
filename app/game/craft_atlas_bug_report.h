#pragma once

#include "ro/ui/craft_atlas_window.h"

#include "game/bug_report_context.h"

namespace moonclient {

// LE RAPPORT D'UN BUG DE L'ATLAS DES RECETTES, selon ce que la fenêtre montrait :
//   - la navigation : l'onglet, la recherche, « réalisable maintenant », la
//     profondeur du retour, et ce que le staff prépare ;
//   - la fiche de l'item choisi : son identifiant, son nom, ce que le sac en
//     contient, et sa recette — skill, niveaux, rendement, combien de fois elle
//     est réalisable — avec ses premiers ingrédients, chacun avec ce qu'il en
//     MANQUE pour une fabrication : c'est la ligne que le joueur conteste le plus
//     souvent ;
//   - ce que la fiche dit encore de lui, en comptes : fournées, usages, flèches.
// ⚠ LA FICHE EST CELLE QUE L'IMAGE A DESSINÉE : à composer avant que l'Atlas ne
// la refasse.
[[nodiscard]] BugReportContext CraftAtlasBugReport(const ro::ui::CraftAtlasWindowState& window,
                                                   const ro::ui::CraftAtlasView& view);

}  // namespace moonclient
