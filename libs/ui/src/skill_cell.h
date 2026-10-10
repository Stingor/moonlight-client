#pragma once

#include <span>

#include "ro/ui/skill_window.h"
#include "ro/ui/skin_art.h"

// CE QUE LE JOUEUR FAIT D'UNE CASE DE COMPÉTENCE, et ce qu'elle lui montre.
//
// 🔴 LES DEUX VUES PARTAGENT CES GESTES, et il le faut : la grille et la liste
// montrent le même arbre autrement, pas deux choses différentes. Les écrire deux
// fois, c'est promettre qu'elles divergeront — et le joueur qui bascule d'une vue
// à l'autre perdrait la moitié de ce qu'il vient d'apprendre.
//
// En-tête PRIVÉ : personne hors de cette bibliothèque n'a à poser un demi-geste
// sur une compétence.
namespace ro::ui {

// LE RANG AUQUEL LA COMPÉTENCE PARTIRA, en pastille au centre de son icône.
//
// 🔴 ELLE NE PARAÎT QUE SI LE RÉGLAGE EST BRIDÉ sous le rang appris, ce qui est
// le cas ANORMAL — et c'est tout l'intérêt : le lancement, le double-clic et le
// glisser partent tous à ce rang-là, et il faut pouvoir le voir sans ouvrir le
// menu. Au rang maximal, la pastille ne dirait rien que le chiffre sous l'icône
// ne dise déjà.
//
// ⚠ `always` PENDANT UN GLISSER : là on la montre MÊME au maximum, parce que
// c'est ce rang-là qui va être posé sur la barre — et la molette le règle encore
// en plein vol.
void DrawUseLevelBadge(ImDrawList* into, const SkillCellView& cell, ImVec2 icon_at, float side,
                       bool always = false);

// LE SURVOL : le nom, le rang, et ce qu'il faut avant. `staff_allowed` y
// ajoute l'identifiant, voir `ro/ui/staff_skill_id.h`.
void ShowSkillTooltip(const SkillCellView& cell, std::span<const SkillCellView> siblings,
                      bool staff_allowed);

// TOUT CE QUI SE FAIT SUR UNE CASE : glisser vers la barre, survoler, réserver,
// lancer, décrire, ouvrir le menu.
//
// ⚠ À APPELER JUSTE APRÈS L'ÉLÉMENT DE LA CASE — bouton invisible en grille,
// ligne en liste : les cibles de glisser et les tests de clic se posent sur le
// DERNIER élément soumis.
//
// `icon_side` est la taille de l'icône dans la charge du glisser ; `hot` dit si
// la case est survolée, l'appelant l'ayant déjà demandé pour son propre dessin.
// `staff_allowed` est le droit que `DrawGrimoireTab` a reçu.
void SkillCellGestures(const SkillCellView& cell, std::span<const SkillCellView> siblings,
                       SkillWindowState& state, const SkinArt& art, float icon_side, bool hot,
                       bool staff_allowed, SkillWindowAction& action);

// ── CE QUE LA GRILLE DU GRIMOIRE PRÊTE À CELLE DE LA GUILDE ────────────────
//
// ⚠ UNE SEULE PRÉFÉRENCE « ICÔNES DE COMPÉTENCE », et un seul voile d'attente :
// deux grilles d'icônes qui les écriraient chacune à leur façon finiraient par
// ne plus se ressembler.

// Impose le lissage — ou la netteté — des icônes pour la suite de la liste de
// dessin, puis le rend.
void ImposeSkillIconSampler(ImDrawList* into, bool smooth);
void RestoreSkillIconSampler(ImDrawList* into);

// Le voile d'attente qui monte sur l'icône, et le décompte au milieu.
void PaintSkillCooldown(ImDrawList* into, const SkillCellView& cell, ImVec2 icon_at, float side);

}  // namespace ro::ui
