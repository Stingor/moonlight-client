#pragma once

#include <array>
#include <cstdint>

#include "ro/render/actor_effect_table.h"

namespace ro::render {

// LA LUEUR ÉLÉMENTAIRE : une teinte discrète qui accompagne la flèche en vol et
// borde le calque de l'arme, de la couleur de l'élément que porte l'attaque.
//
// Le dessin ne connaît pas les éléments du jeu : il reçoit une TEINTE parmi
// neuf, et l'élément neutre n'en a pas — qui l'appelle n'a rien à montrer.
enum class ElementHue : std::uint8_t {
  kWater,
  kEarth,
  kFire,
  kWind,
  kPoison,
  kHoly,
  kShadow,
  kGhost,
  kUndead,
};

// Rouge, vert, bleu de la teinte.
[[nodiscard]] std::array<std::uint8_t, 3> ElementHueRgb(ElementHue hue) noexcept;

// L'alpha des deux lueurs : assez pour se lire sur un fond clair, assez peu
// pour ne pas couvrir le sprite.
inline constexpr std::uint8_t kWeaponGlowAlpha = 120;
inline constexpr std::uint8_t kArrowGlowAlpha = 150;
// Le halo de l'arme déborde de son calque d'un pixel et demi de sprite de
// chaque côté.
inline constexpr float kWeaponGlowGrowPixels = 1.5F;
// La lueur de la flèche est la même image, un peu plus grande qu'elle.
inline constexpr float kArrowGlowScale = 1.2F;

// LE HALO DE L'ARME : une copie du seul calque de l'arme, teinte de l'élément,
// élargie et AJOUTÉE au décor, derrière le corps.
[[nodiscard]] ActorBodyCopy WeaponGlowCopy(ElementHue hue) noexcept;

// La teinte et l'alpha de la lueur d'une flèche, en rouge, vert, bleu, alpha.
[[nodiscard]] std::array<std::uint8_t, 4> ArrowGlowTint(ElementHue hue) noexcept;

}  // namespace ro::render
