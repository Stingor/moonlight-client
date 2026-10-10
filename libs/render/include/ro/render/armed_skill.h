#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

namespace ro::render {

// CE QU'UN SORT QUI ATTEND SA CIBLE MONTRE À L'ÉCRAN.
//
// Le client en montre DEUX choses, et il faut les distinguer pour les régler :
//
//   · LE RANG, en chiffres, collé au curseur — le sprite des chiffres de
//     dégâts, à l'échelle deux, vingt-quatre pixels à droite de la pointe ;
//   · L'ÉTIQUETTE, une bulle de texte centrée en haut de l'écran, qui ne porte
//     que le NOM de la compétence.
//
// 🔴 L'ÉTIQUETTE DU CLIENT NE DIT PAS LE RANG, et c'est pourquoi `banner_level`
// existe : qui garde les deux affichages ne veut pas forcément le rang deux
// fois. Éteindre l'un n'éteint pas l'autre.

// Les défauts sont ceux du client, et ce sont eux qui font foi : un réglage
// remis à zéro doit rendre le jeu tel qu'il est.
inline constexpr float kClientArmedLevelScale = 2.0F;
inline constexpr float kClientArmedLevelOffsetXPixels = 24.0F;

// La hauteur de l'étiquette, en pourcentage de la fenêtre : le client pose la
// sienne à 46 lignes d'un écran qui en faisait 480.
inline constexpr float kClientArmedBannerHeightPercent = 46.0F * 100.0F / 480.0F;

// L'écart d'un chiffre au suivant, AVANT l'échelle : le client en met seize à
// l'échelle deux, et c'est le même pas que ses chiffres de dégâts.
inline constexpr float kArmedLevelDigitPitchPixels = 8.0F;

// La couleur de l'étiquette : celle du client (`0x32FF32`, un COLORREF), rendue
// ici dans la convention des réglages — alpha, bleu, vert, rouge du poids fort
// au faible, comme `SkillAreaLook::color`.
inline constexpr std::uint32_t kClientArmedBannerColor = 0xFF32FF32U;

struct ArmedSkillLook {
  // ── LE RANG AU CURSEUR ────────────────────────────────────────────────────
  bool level_shown = true;
  float level_scale = kClientArmedLevelScale;
  float level_offset_x_pixels = kClientArmedLevelOffsetXPixels;
  // ── L'ÉTIQUETTE ───────────────────────────────────────────────────────────
  bool banner_shown = true;
  bool banner_level = true;
  float banner_height_percent = kClientArmedBannerHeightPercent;
  std::uint32_t banner_color = kClientArmedBannerColor;
};

// Les bornes des trois curseurs. Elles se posent AU DESSIN et non à la saisie :
// les réglages viennent aussi d'un fichier que le joueur peut écrire à la main,
// et cette voie-là ne passe par aucun curseur.
inline constexpr float kSmallestArmedLevelScale = 0.5F;
inline constexpr float kLargestArmedLevelScale = 6.0F;
inline constexpr float kSmallestArmedLevelOffsetPixels = -64.0F;
inline constexpr float kLargestArmedLevelOffsetPixels = 160.0F;
inline constexpr float kSmallestArmedBannerHeightPercent = 0.0F;
inline constexpr float kLargestArmedBannerHeightPercent = 90.0F;

// Les quatre octets d'une couleur de réglage — même convention que la zone des
// sorts, et pour la même raison : le fichier ne sait déclarer qu'un entier.
[[nodiscard]] std::array<std::uint8_t, 4> ArmedBannerColorBytes(std::uint32_t color) noexcept;

// La ligne de base de l'étiquette : centrée en abscisse, à la hauteur réglée.
[[nodiscard]] glm::vec2 ArmedBannerBaseline(const ArmedSkillLook& look, float viewport_width,
                                            float viewport_height) noexcept;

// OÙ SE POSE LE CHIFFRE D'INDICE `index`, les UNITÉS d'abord — l'ordre dans
// lequel le client détache ses chiffres, et celui de `NumberGlyphs`.
//
// ⚠ LES UNITÉS SONT À DROITE : le client pose le chiffre de poids fort à
// l'écart réglé et remonte vers la droite d'un pas par rang. Poser l'indice zéro
// à gauche écrirait le nombre à l'envers dès la dizaine.
//
// `cursor_px` est la pointe du curseur, recentrage compris.
[[nodiscard]] glm::vec2 ArmedLevelDigitAt(const ArmedSkillLook& look, glm::vec2 cursor_px,
                                          std::size_t digit_count, std::size_t index) noexcept;

}  // namespace ro::render
