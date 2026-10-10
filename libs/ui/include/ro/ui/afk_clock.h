#pragma once

#include <cstdint>

#include <imgui.h>

namespace ro::ui {

// L'HORLOGE DE L'ÉCRAN DE VEILLE : l'heure, et depuis combien de temps.
//
// Deux lignes centrées, posées sur une grille de neuf ancrages. C'est tout ce
// que la veille laisse à l'écran une fois l'interface effacée — d'où l'ombre
// portée, qui n'est pas un ornement : le décor dessous est le jeu lui-même, en
// mouvement, et une heure blanche disparaît dès que la caméra passe devant une
// falaise claire.
//
// ⚠ ELLE NE LIT AUCUNE HORLOGE ET NE COMPTE RIEN : les deux lignes arrivent
// écrites. `ui` ne possède aucun état de jeu, et « depuis quand » est une
// mesure de la veille, pas de la fenêtre.

// LE NOMBRE D'ANCRAGES, en ordre de lecture : haut-gauche, haut-centre,
// haut-droite, puis les deux rangées suivantes.
//
// 🔴 NEUF ET NON QUATRE. Les autres surfaces flottantes de ce client se
// contentent des quatre coins, ce qui suffit à un panneau qu'on range au bord ;
// ici l'écran est VIDE, et les deux places qui comptent — le centre et le
// bas-centre — sont justement celles qu'un choix en quatre coins ne sait pas
// exprimer.
inline constexpr int kAfkClockAnchorCount = 9;
// Combien de colonnes compte cette grille. La ligne s'en déduit par division.
inline constexpr int kAfkClockAnchorColumns = 3;

// La marge la plus large qu'on puisse demander, en pixels d'écran.
inline constexpr int kMostAfkClockMargin = 400;

// Les bornes de la taille de l'heure, en multiples de la police.
inline constexpr float kLeastAfkClockScale = 0.5F;
inline constexpr float kMostAfkClockScale = 6.0F;

// DE COMBIEN LA SECONDE LIGNE EST PLUS PETITE QUE LA PREMIÈRE.
//
// ⚠ ELLE N'A NI TAILLE NI COULEUR À ELLE, et c'est délibéré : ce sont deux
// morceaux d'une même phrase, et deux réglages de plus n'apporteraient qu'une
// façon supplémentaire de les désaccorder.
inline constexpr float kAfkAwayScale = 0.45F;
// Et de combien elle est plus discrète.
inline constexpr float kAfkAwayOpacity = 0.6F;

struct AfkClockView {
  // L'heure. Vide = rien à peindre.
  const char* time = "";
  // « Absent depuis … », ou nul quand le joueur n'en veut pas.
  const char* away = nullptr;

  int anchor = 0;
  int margin = 0;
  float scale = 1.0F;
  bool shadow = true;
  ImU32 color = IM_COL32_WHITE;

  // L'AVANCEMENT DE LA RAMPE, de zéro à un : l'horloge apparaît et s'efface
  // avec la veille.
  float fade = 1.0F;
};

void DrawAfkClock(const AfkClockView& view);

}  // namespace ro::ui
