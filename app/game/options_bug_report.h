#pragma once

#include <span>

#include "ro/ui/options_window.h"

#include "game/bug_report_context.h"

namespace moonclient {

// Ce que la fenêtre des options montrait quand le joueur a signalé un bug.
struct OptionsBugScene {
  const ro::ui::OptionsWindowState* window = nullptr;
  // Les pages telles qu'elles viennent d'être dessinées : celle de
  // `window->section` porte les rangées de l'onglet ouvert.
  std::span<const ro::ui::OptionSectionView> sections;
};

// LE RAPPORT D'UN BUG D'OPTIONS : la page — son rang dans `OptionSection` et
// son titre —, l'onglet ouvert quand elle en a, et la VALEUR de chaque réglage
// montré, par son identifiant — `[id, valeur]` —, les communs aux onglets en
// tête. Une case vaut 0 ou 1, une liste le rang choisi, une couleur son
// `ImU32` en hexadécimal ; un bouton, une note ou un cadre n'ont pas de valeur.
//
// ⚠ LA LISTE EST BORNÉE en octets, pour que le contexte entier tienne dans la
// colonne du serveur ; `values_cut` dit qu'elle a été coupée.
[[nodiscard]] BugReportContext OptionsBugReport(const OptionsBugScene& scene);

}  // namespace moonclient
