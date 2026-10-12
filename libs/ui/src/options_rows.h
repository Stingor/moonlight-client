#pragma once

#include <span>
#include <vector>

#include "ro/ui/options_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LES RANGÉES D'UNE PAGE DE RÉGLAGES : leur contrôle, leur commande, leur aide
// et leur bouton de retour, rangés en blocs sous leurs titres.
//
// ⚠ UN EN-TÊTE INTERNE : seul le panneau de réglages dessine des rangées, et
// dans SA fenêtre — les colonnes mesurées se rangent dans l'état ImGui de la
// fenêtre courante.

// UNE SUITE DE RANGÉES, sous l'identité `scope`.
//
// `covered` traverse les appels d'une même page : un nuancier ouvert recouvre
// tout ce qui suit, et le neutralise.
//
// `kept`, non nul, est le filtre : une case par rangée, vraie pour celle qui se
// montre. Voir `options_search.h`.
//
// `row_wanted`, non nul : la rangée à amener en haut de la page, par son
// identifiant. Remise à moins un dès qu'elle est dessinée.
void DrawOptionRows(const char* scope, std::span<const OptionRowView> rows, const SkinArt& art,
                    OptionsWindowAction* asked, bool* covered, const std::vector<bool>* kept,
                    int* row_wanted);

}  // namespace ro::ui
