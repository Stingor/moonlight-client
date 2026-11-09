#pragma once

#include "ro/ui/palette_editor.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LES PRÉRÉGLAGES DE L'ÉDITEUR DE STYLE, ET LE CODE QUI VOYAGE : le dernier
// style non validé, la liste rangée, le champ de nom, et les boutons du code.
//
// ⚠ PUREMENT LOCAUX : le serveur n'en sait rien et n'a pas à en savoir. Un
// préréglage n'est pas une apparence, c'est un brouillon qu'on reprend.
//
// ⚠ À PART DE LA FENÊTRE pour une raison de TAILLE : c'est une section entière,
// avec son champ resynchronisé et ses gestes, et la fenêtre qui l'héberge a ses
// propres huit cents lignes à tenir.
//
// ⚠ EN-TÊTE PRIVÉ : rien hors de `libs/ui/src` n'appelle ceci.

// Dessine la section repliable ; ce que le joueur y demande part dans `action`.
void DrawPalettePresets(PaletteEditorState& state, const PaletteEditorView& view,
                        const SkinArt& art, PaletteEditorAction& action);

}  // namespace ro::ui
