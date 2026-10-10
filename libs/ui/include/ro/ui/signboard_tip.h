#pragma once

#include <span>
#include <string>

#include <imgui.h>

namespace ro::ui {

// ── LE TITRE ENTIER D'UNE ENSEIGNE COUPÉE ───────────────────────────────────
//
// Une enseigne de salon ou d'échoppe coupe son titre en « ... » quand il
// déborde sa zone. La souris posée sur cette zone le montre alors en entier,
// dans la bulle du jeu, ancrée un peu au-dessus et à gauche du titre : elle le
// recouvre, et le texte entier s'écrit presque où commençait le coupé.

// Une enseigne telle qu'elle a été peinte, en pixels d'écran.
struct SignboardTipView {
  // Toute l'enseigne : c'est elle que la souris désigne d'abord.
  ImVec2 board_min;
  ImVec2 board_max;
  // La zone du titre.
  ImVec2 title_min;
  ImVec2 title_max;
  // Le titre entier, VIDE quand il n'a pas été coupé.
  std::string cut_title;
};

// L'écart entre le coin haut-gauche du titre et celui de la bulle.
inline constexpr ImVec2 kSignboardTipOffset = {-6.0F, -4.0F};

// Montre le titre entier de l'enseigne sous la souris, s'il a été coupé.
//
// ⚠ LA DERNIÈRE PEINTE GAGNE, et elle seule : une enseigne posée sur le titre
// d'une autre le cache à la souris comme à l'œil. Rien ne se montre quand la
// souris est sur une fenêtre.
void DrawSignboardTip(std::span<const SignboardTipView> boards);

}  // namespace ro::ui
