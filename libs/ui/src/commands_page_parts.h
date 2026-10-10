#pragma once

#include "ro/ui/commands_page.h"

namespace ro::ui::commands_page {

// Ce qu'une mesure en pixels vaut à l'échelle de l'interface.
[[nodiscard]] float Px(float pixels);

// Une case d'un réglage de commande, son marqueur d'aide à la suite.
void Toggle(const char* label, bool value, const char* help, CommandsField field,
            const SkinArt& art, CommandsPageAction& asked);

// Un texte en gris : ce qui informe sans rien demander.
void FaintText(const char* text);

// L'onglet « Autoloots ».
void DrawAutolootTab(const CommandsPageView& view, CommandsPageState& state, const SkinArt& art,
                     CommandsPageAction& asked);

// La liste « @autolootid » et ses presets, dans son nœud repliable.
void DrawAutolootIdNode(const CommandsPageView& view, CommandsPageState& state, const SkinArt& art,
                        CommandsPageAction& asked);

}  // namespace ro::ui::commands_page
