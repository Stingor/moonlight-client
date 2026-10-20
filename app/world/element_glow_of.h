#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "ro/render/element_glow.h"
#include "ro/sim/actor.h"
#include "ro/sim/attack_element.h"

namespace moonclient {

// LA TEINTE DE LA LUEUR ÉLÉMENTAIRE pour un élément d'attaque, si elle se montre :
// l'option « Lueur élémentaire » cochée, et un élément autre que neutre.
//
// 🔴 L'ÉLÉMENT VIENT DU SEUL PAQUET DU SERVEUR (`Actor::attack_element`) : rien
// ne se déduit ici de l'arme portée ni des états reçus. Sans paquet, `kNone`,
// et aucune lueur — c'est ce qui garde le combat entre joueurs à l'abri.
[[nodiscard]] constexpr std::optional<ro::render::ElementHue> ElementGlowHueOf(
    ro::sim::Element element, bool glow_shown) noexcept {
  if (!glow_shown) {
    return std::nullopt;
  }
  using ro::render::ElementHue;
  using ro::sim::Element;
  switch (element) {
    case Element::kWater:
      return ElementHue::kWater;
    case Element::kEarth:
      return ElementHue::kEarth;
    case Element::kFire:
      return ElementHue::kFire;
    case Element::kWind:
      return ElementHue::kWind;
    case Element::kPoison:
      return ElementHue::kPoison;
    case Element::kHoly:
      return ElementHue::kHoly;
    case Element::kShadow:
      return ElementHue::kShadow;
    case Element::kGhost:
      return ElementHue::kGhost;
    case Element::kUndead:
      return ElementHue::kUndead;
    case Element::kNeutral:
    case Element::kNone:
      return std::nullopt;
  }
  return std::nullopt;
}

// LA LUEUR QUI ACCOMPAGNE LA FLÈCHE de `shooter`, en rouge, vert, bleu, alpha :
// rien quand elle ne se montre pas.
[[nodiscard]] inline std::optional<std::array<std::uint8_t, 4>> ArrowGlowOf(
    const ro::sim::Actor& shooter, bool glow_shown) noexcept {
  const std::optional<ro::render::ElementHue> hue =
      ElementGlowHueOf(shooter.attack_element, glow_shown);
  if (!hue.has_value()) {
    return std::nullopt;
  }
  return ro::render::ArrowGlowTint(*hue);
}

}  // namespace moonclient
