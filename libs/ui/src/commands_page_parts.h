#pragma once

#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/commands_page.h"

namespace ro::ui::commands_page {

// Une case d'un réglage de commande, son marqueur d'aide à la suite.
void Toggle(const char* label, bool value, const char* help, CommandsField field,
            const SkinArt& art, CommandsPageAction& asked);

// Un texte en gris : ce qui informe sans rien demander.
void FaintText(const char* text);

// L'onglet « Autoloots ».
void DrawAutolootTab(const CommandsPageView& view, CommandsPageState& state, const SkinArt& art,
                     CommandsPageAction& asked);

// CE QUE L'ONGLET « AUTOLOOTS » DONNE À LA RECHERCHE : ses libellés traduits,
// ses commandes et ses types, sous l'onglet `tab`.
void AppendAutolootTerms(std::vector<OptionBodyTerm>& into, int tab);

// UN LIBELLÉ MARQUÉ PAR `TrText`, traduit. ⚠ LA PAGE LE LIT À DEUX ENDROITS —
// en se dessinant, et pour la recherche du panneau —, d'où un littéral déclaré
// une fois, et traduit ici seulement : un `Tr` sur une constante ne serait vu
// d'aucun catalogue.
[[nodiscard]] const char* Translated(const char* marked);

// LES MOTS QUE LE DESSIN ET LA RECHERCHE PARTAGENT. ⚠ LES COMMANDES NE SE
// TRADUISENT PAS : elles se tapent telles quelles.
inline constexpr const char* kAutolootIdCommand = "@autolootid";
inline constexpr const char* kPresetsLabel = core::TrText("Presets :");

// La liste « @autolootid » et ses presets, dans son nœud repliable.
void DrawAutolootIdNode(const CommandsPageView& view, CommandsPageState& state, const SkinArt& art,
                        CommandsPageAction& asked);

}  // namespace ro::ui::commands_page
