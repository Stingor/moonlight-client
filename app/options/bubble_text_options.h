#pragma once

#include "ro/ui/chat_bubble.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LA TAILLE DU TEXTE DES BULLES DE PAROLE ─────────────────────────────────
//
// Un curseur de la page des bulles, en pour cent de la taille de l'interface.
// La bulle entière grandit avec son texte : voir `ChatBubbleLook`.
//
// ⚠ EN BIBLIOTHÈQUE, hors du catalogue des options : sa clé, sa rangée et ses
// bornes s'éprouvent sur un banc. `row_id` est l'identifiant que le catalogue
// donne à la rangée.

inline constexpr const char* kBubbleTextScaleKey = "bubbles_text_scale";

// Déclare la taille au fichier. ⚠ AVANT SA LECTURE, comme tout réglage.
void DeclareBubbleTextScale(SettingsFile& into, ro::ui::ChatBubbleLook& look);

// La rangée du curseur. ⚠ ELLE MONTRE LA VALEUR BORNÉE : une valeur relue hors
// bornes s'y lit telle que la bulle l'emploie.
[[nodiscard]] ro::ui::OptionRowView BubbleTextScaleRow(int row_id,
                                                       const ro::ui::ChatBubbleLook& look);

// Ce que le curseur demande, arrondi au pour cent et ramené dans les bornes.
void ChangeBubbleTextScale(ro::ui::ChatBubbleLook& look, float asked_percent);

}  // namespace moonclient
