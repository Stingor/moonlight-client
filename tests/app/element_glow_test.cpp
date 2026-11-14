// LA LUEUR ÉLÉMENTAIRE : quelle teinte un élément donne, quand elle se montre, et
// ce que le halo de l'arme reprend du corps.

#include "ro/render/element_glow.h"

#include <array>
#include <cstdint>
#include <optional>
#include <set>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_layers.h"
#include "ro/sim/actor.h"
#include "ro/sim/attack_element.h"
#include "ro/sim/status_effects.h"

#include "world/element_glow_of.h"

using moonclient::ArrowGlowOf;
using moonclient::ElementGlowHueOf;
using ro::render::ElementHue;
using ro::sim::Element;

namespace {

// L'état d'Aspersio tel que le serveur le pose : un endow que l'on VOIT, et
// dont rien ne doit être déduit.
constexpr ro::sim::StatusEffectId kAspersioStatus = 17;
// Un arc quelconque dans la main.
constexpr std::uint32_t kBowView = 11;

constexpr std::array<Element, 9> kColoredElements = {
    Element::kWater, Element::kEarth,  Element::kFire,  Element::kWind,  Element::kPoison,
    Element::kHoly,  Element::kShadow, Element::kGhost, Element::kUndead};

}  // namespace

TEST_CASE("Lueur élémentaire : chaque élément a sa teinte, le neutre et « aucun » n'en ont pas",
          "[app][element_glow]") {
  STATIC_CHECK(ElementGlowHueOf(Element::kFire, true) == ElementHue::kFire);
  STATIC_CHECK(ElementGlowHueOf(Element::kWater, true) == ElementHue::kWater);
  STATIC_CHECK(ElementGlowHueOf(Element::kShadow, true) == ElementHue::kShadow);
  STATIC_CHECK(ElementGlowHueOf(Element::kUndead, true) == ElementHue::kUndead);
  STATIC_CHECK_FALSE(ElementGlowHueOf(Element::kNeutral, true).has_value());
  // 🔴 REÇU « AUCUN » : PAS DE LUEUR.
  STATIC_CHECK_FALSE(ElementGlowHueOf(Element::kNone, true).has_value());

  // Neuf teintes, toutes différentes.
  std::set<std::array<std::uint8_t, 3>> seen;
  for (const Element element : kColoredElements) {
    const std::optional<ElementHue> hue = ElementGlowHueOf(element, true);
    REQUIRE(hue.has_value());
    seen.insert(ro::render::ElementHueRgb(*hue));
  }
  CHECK(seen.size() == kColoredElements.size());
}

TEST_CASE("Lueur élémentaire : l'option décochée l'éteint partout", "[app][element_glow]") {
  for (const Element element : kColoredElements) {
    CHECK_FALSE(ElementGlowHueOf(element, false).has_value());
  }
  ro::sim::Actor archer;
  archer.attack_element = Element::kFire;
  CHECK(ArrowGlowOf(archer, true).has_value());
  CHECK_FALSE(ArrowGlowOf(archer, false).has_value());
}

TEST_CASE("Lueur élémentaire : l'élément d'un autre joueur n'est jamais montré sans paquet",
          "[app][element_glow]") {
  // Un archer qui tient un arc et porte un endow visible, mais dont le serveur
  // n'a rien dit — une carte de combat entre joueurs, un duel.
  ro::sim::Actor archer;
  archer.look.weapon = kBowView;
  archer.status_effects.insert(kAspersioStatus);
  REQUIRE(archer.attack_element == Element::kNone);
  CHECK_FALSE(ElementGlowHueOf(archer.attack_element, true).has_value());
  CHECK_FALSE(ArrowGlowOf(archer, true).has_value());
}

TEST_CASE("Lueur élémentaire : la flèche prend la teinte de l'élément, alpha modéré",
          "[app][element_glow]") {
  ro::sim::Actor archer;
  archer.attack_element = Element::kWater;
  const std::optional<std::array<std::uint8_t, 4>> tint = ArrowGlowOf(archer, true);
  REQUIRE(tint.has_value());
  const std::array<std::uint8_t, 3> water = ro::render::ElementHueRgb(ElementHue::kWater);
  CHECK((*tint)[0] == water[0]);
  CHECK((*tint)[1] == water[1]);
  CHECK((*tint)[2] == water[2]);
  CHECK((*tint)[3] == ro::render::kArrowGlowAlpha);
  CHECK((*tint)[3] < 255);
}

TEST_CASE("Lueur élémentaire : le halo de l'arme ne reprend que l'arme, en addition",
          "[app][element_glow]") {
  const ro::render::ActorBodyCopy halo = ro::render::WeaponGlowCopy(ElementHue::kFire);
  CHECK(halo.parts == ro::render::PartBit(ro::render::ActorPart::kWeapon));
  CHECK(halo.additive);
  CHECK_FALSE(halo.in_front);
  CHECK(halo.scale == 1.0F);
  CHECK(halo.grow_pixels > 0.0F);
  CHECK(halo.grow_pixels <= 2.0F);
  const std::array<std::uint8_t, 3> fire = ro::render::ElementHueRgb(ElementHue::kFire);
  CHECK(halo.tint[0] == fire[0]);
  CHECK(halo.tint[1] == fire[1]);
  CHECK(halo.tint[2] == fire[2]);
  CHECK(halo.tint[3] == ro::render::kWeaponGlowAlpha);
}
