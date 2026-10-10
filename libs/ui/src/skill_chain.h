#pragma once

#include <cstddef>
#include <span>

#include <imgui.h>

#include "ro/ui/skill_window.h"

// LES FLÈCHES DE CHAÎNE DU GRIMOIRE : d'où vient une compétence, et ce qu'elle
// ouvre.
//
// 🔴 ELLES ONT QUITTÉ LA FENÊTRE PARCE QU'ELLE PASSAIT SON PLAFOND, et c'est
// elles qui sont parties parce qu'elles forment un tout : un parcours de graphe,
// sa réduction, et le trait qui en sort. La fenêtre, elle, n'a plus qu'à dire
// QUAND les tracer.
//
// En-tête PRIVÉ : personne hors de cette bibliothèque n'a à tracer une demi-
// branche d'arbre de compétences. La seule chose qui en sorte est
// `SkillEdgeIsRedundant`, qui est une RÈGLE de l'arbre et non un dessin — voir
// `ro/ui/skill_window.h`.
namespace ro::ui {

// LE RAYON APPROCHÉ D'UNE CASE, en part de sa largeur : la flèche part de son
// BORD et jamais de son centre — sous l'icône elle serait cachée, et sa pointe
// doit rester lisible.
//
// ⚠ ICI ET NON DANS LE CORPS : c'est l'appelant qui connaît la largeur d'une
// case, et c'est donc lui qui fait la multiplication.
inline constexpr float kChainTrimShare = 0.45F;

// LA BRANCHE ENTIÈRE, EN LARGEUR, depuis la case survolée.
//
// `upstream` vrai remonte vers les prérequis, faux descend vers ce qu'ils
// ouvrent. `centers` et `shown` s'indexent comme `group.cells` ; `trim` est le
// rayon approché d'une case, d'où la flèche part et où elle s'arrête.
void WalkChain(ImDrawList* into, const SkillGroupView& group, std::span<const ImVec2> centers,
               std::span<const char> shown, std::size_t focus, bool upstream, float trim);

}  // namespace ro::ui
