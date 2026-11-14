#pragma once

#include "ro/ui/palette_editor.h"

namespace ro::ui {

// LES CALQUES DE L'ÉDITEUR DE STYLE : la barre d'onglets, et ce que chaque
// onglet ajoute sous ses pièces — la teinture qui appartient à l'objet, la
// couleur unique des cheveux, la ligne de peau de la Tête et du Corps.
//
// ⚠ À PART DE LA FENÊTRE pour une raison de TAILLE : la fenêtre a ses propres
// huit cents lignes à tenir.
//
// ⚠ EN-TÊTE PRIVÉ : rien hors de `libs/ui/src` n'appelle ceci.

// LA LISTE DE CE QU'ON RÈGLE : les corps d'abord — le corps porté, puis ses
// variantes, monté, en tenue —, la tête, puis chaque calque porté. Une entrée
// grisée dit pourquoi au survol.
//
// 🔴 UNE LISTE ET NON UNE BARRE D'ONGLETS : dix calques nommés côte à côte
// imposaient à la fenêtre une largeur que rien d'autre ne demande.
//
// ⚠ ELLE ÉCRIT `state.tab`, et le corps choisi dans `action.edit_body` : c'est
// ce que `app` composera à l'image suivante.
void DrawLayerChoice(PaletteEditorState& state, const PaletteEditorView& view, const SkinArt& art,
                     PaletteEditorAction& action);

// L'onglet dont la vue est faite, ou nul.
[[nodiscard]] const PaletteTab* OpenTab(const PaletteEditorView& view);

// Ce que l'onglet ouvert ajoute sous ses pièces.
void DrawLayerNotes(const PaletteEditorView& view, const SkinArt& art, PaletteEditorAction& action);

// LA LIGNE DE PEAU de l'onglet ouvert : pastille et nuancier, teints proposés,
// retour en arrière, et la raison quand elle ne se règle pas. Rend vrai quand
// son nuancier est ouvert ; écrit `state.skin_focus`.
[[nodiscard]] bool DrawSkinLine(PaletteEditorState& state, const PaletteEditorView& view,
                                const SkinArt& art, PaletteEditorAction& action);

// Pourquoi ce calque ne se règle pas, déjà traduit.
[[nodiscard]] const char* TabBlockReason(PaletteTabBlock blocked);

}  // namespace ro::ui
