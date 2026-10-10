#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace ro::render {

// LA GARDE DU CHEVALIER, ET LE COUP QUI LA DÉPENSE.
//
// 🔴 CET EFFET N'A NI COMPÉTENCE NI CIBLE, et c'est ce qui le rend invisible
// aux trois tables d'effets de compétence. Auto Counter ne montre rien quand
// on l'emploie : il ARME quelque chose sur son lanceur, et c'est le coup
// SUIVANT qui le dépense. Le client tient cela dans un drapeau de l'acteur,
// qui a trois gestes :
//
//   · l'effet soixante et un, et lui seul, met l'acteur dans l'ÉTAT D'ACTION
//     13 — la posture de garde — juste après lui avoir annoncé sa durée par le
//     message 82 ;
//   · l'entrée dans cet état ALLUME le drapeau — c'est le SEUL chemin qui
//     l'allume, tous les autres le rendent à zéro — et en sortir l'éteint ;
//   · une action de dégâts le DÉPENSE : quand l'acteur en porte une alors que
//     le drapeau est armé, l'effet 131 naît sur LUI, reçoit la commande 50, et
//     le drapeau retombe.
//
// ⚠ UNE SEULE FOIS PAR GARDE, ET SUR LE PORTEUR, non sur ce qu'il touche. Tous
// nos autres effets de coup se posent sur la cible ou dépendent de la
// compétence employée ; celui-ci ne dépend que de l'état du frappeur.
//
// ⚠ LA DURÉE DE LA POSTURE VAUT `niveau × 400 ms`, et nous ne la portons pas : l'effet que le
// serveur annonce arrive NU, sans le niveau qui le borne. Ce n'est pas une perte, parce que le
// client dépense la garde au premier coup et l'éteint à tout changement
// d'état — c'est ce que nous portons, et cela suffit à ne jamais la montrer
// deux fois.

// L'effet qui ARME la garde : la colonne d'anneaux bleus du chevalier.
inline constexpr std::uint16_t kAutoCounterGuardEffect = 61;

// L'effet que le coup fait jaillir sur le porteur de la garde. Il a un fichier
// — `AutoCounter.str` — et son propre bruit, `knight_autocounter.wav`.
inline constexpr std::uint16_t kAutoCounterStrikeEffect = 131;

// LES SORTES DE COUP QUI DÉPENSENT LA GARDE — sept sortes d'action de dégâts.
// Les trois autres sortes que le client distingue à cet endroit ne portent pas
// de dégâts : un ramassage, une assise, un relevé.
//
// ⚠ CE N'EST PAS LA MÊME GARDE QUE `DamageTypeShowsHitEffect`, qui vient d'un
// AUTRE répartiteur — celui des impacts de compétence — et qui écarte deux
// sortes que celui-ci accepte. Les confondre ferait rater la garde sur un coup
// de zone.
inline constexpr std::array<std::uint8_t, 7> kAutoCounterDamageTypes = {0, 4, 8, 9, 10, 11, 13};

// Si cette sorte de coup dépense la garde.
[[nodiscard]] inline bool AutoCounterSpentBy(std::uint8_t damage_type) noexcept {
  return std::ranges::find(kAutoCounterDamageTypes, damage_type) != kAutoCounterDamageTypes.end();
}

}  // namespace ro::render
