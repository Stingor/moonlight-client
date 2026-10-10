#pragma once

// LE CADAVRE D'UNE CRÉATURE : quand il paraît, combien de temps il reste, et
// ce qu'il lui reste d'opacité en chemin.
//
// La règle a deux moitiés, et il faut les deux :
//
//   * À la mort, le client joue l'agonie jusqu'à sa dernière image. C'EST DONC
//     LA FIN DE L'AGONIE qui ouvre le fondu, et non le paquet de mort : un
//     corps qui tombe lentement reste entier plus longtemps.
//   * Il pose alors, à la place de l'acteur, un CADAVRE figé sur cette
//     dernière image, qui perd un cran d'opacité toutes les vingt-quatre
//     millisecondes — le pas de ses animations. Parti de l'opacité pleine, il
//     s'efface donc en 255 × 24 = 6 120 ms, et s'en va quand elle est nulle.
//
// La durée du cadavre est un RÉGLAGE : le défaut est celle du client, et le
// joueur peut la raccourcir ou l'allonger. Le fondu reste linéaire, de
// l'opacité pleine à rien, quelle que soit sa durée.
//
// ⚠ CES FONCTIONS NE DISENT PAS QUI DISPARAÎT. Un joueur mort reste à terre et
// c'est le serveur qui le retire ; seule une créature s'efface d'elle-même.
// L'appelant tranche, parce que cela se lit dans le monde et non dans l'image.

#include <cstdint>

namespace ro::render {

// Le pas d'opacité du cadavre, et la durée qui en découle depuis l'opacité
// pleine : celle du client.
inline constexpr std::uint32_t kCorpseFadeMsPerAlpha = 24;
inline constexpr std::uint32_t kClientCorpseFadeMs = 255 * kCorpseFadeMsPerAlpha;

// Les bornes du réglage. Au-dessous, le cadavre ne se verrait plus s'effacer ;
// au-delà, une carte de chasse se couvrirait de corps.
inline constexpr std::uint32_t kShortestCorpseFadeMs = 250;
inline constexpr std::uint32_t kLongestCorpseFadeMs = 30'000;

// L'opacité du corps, de zéro à deux cent cinquante-cinq, `since_death_ms`
// après que le serveur a annoncé la mort. Pleine tant que l'agonie se joue.
[[nodiscard]] std::uint8_t VanishAlphaAt(float agony_ms, std::uint32_t since_death_ms,
                                         std::uint32_t corpse_fade_ms) noexcept;

// Vrai quand le cadavre est effacé et qu'il n'y a plus rien à dessiner.
[[nodiscard]] bool VanishIsOver(float agony_ms, std::uint32_t since_death_ms,
                                std::uint32_t corpse_fade_ms) noexcept;

// ── L'APPARITION ─────────────────────────────────────────────────────────────
//
// Une créature que le serveur fait APPARAÎTRE (et non qui entre simplement en
// vue) naît presque transparente, à l'opacité 68, et gagne huit crans à chaque
// pas du jeu jusqu'à l'opacité pleine : vingt-quatre pas de seize
// millisecondes. Le staff en règle la durée ; le fondu reste linéaire, et zéro
// l'éteint.
inline constexpr std::uint8_t kAppearFromAlpha = 68;
inline constexpr std::uint32_t kClientAppearFadeMs = 384;
inline constexpr std::uint32_t kLongestAppearFadeMs = 3000;

// L'opacité d'une créature `since_appeared_ms` après son apparition.
[[nodiscard]] std::uint8_t AppearAlphaAt(std::uint32_t since_appeared_ms,
                                         std::uint32_t appear_fade_ms) noexcept;

// ── LA SORTIE DE VUE ─────────────────────────────────────────────────────────
//
// Un acteur qui sort du champ de vision s'efface : le client pose l'échéance
// `maintenant + 510` et prend pour opacité le reste à vivre divisé par deux,
// soit un fondu linéaire de la pleine opacité à rien en 510 ms. Le joueur en
// règle la durée ; zéro le fait disparaître d'un coup.
inline constexpr std::uint32_t kClientLeaveFadeMs = 510;
inline constexpr std::uint32_t kLongestLeaveFadeMs = 3000;

// L'opacité d'un acteur `since_left_ms` après sa sortie de vue ; zéro une fois
// le fondu fini.
[[nodiscard]] std::uint8_t LeaveAlphaAt(std::uint32_t since_left_ms,
                                        std::uint32_t leave_fade_ms) noexcept;

}  // namespace ro::render
